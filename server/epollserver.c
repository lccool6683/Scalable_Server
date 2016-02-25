#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define HOST_NAME_MAX 255
#define EV_BUFSIZE 8
#define LISTEN_PORT 1234

static int setNonblock(int fd);

sem_t mutex;

int main(void) {
    FILE *fp = fopen("server.log", "a");
    char hostname[HOST_NAME_MAX];

    time_t timeVal;
    char *timeStr;

    pid_t pid;
    pid_t childpid;
    int num_forks;

    int i, num_events;

    int optval;
    int server_fd;
    int epoll_fd;
    struct sockaddr_in server_addr;
    struct epoll_event event;
    struct epoll_event *equeue;

    sem_init(&mutex, 0, 1);

    pid = getpid();
    if (gethostname(hostname, HOST_NAME_MAX) == -1) {
        perror("(gethostname) Unable to get host name");
    }

    /* Initialize server */
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
        perror("(socket) Unable to open socket");
    optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (const void *) &optval , sizeof optval);

    memset(&server_addr, 0, sizeof server_addr);
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(LISTEN_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof server_addr) == -1)
        perror("(bind) Unable to bind socket");
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("(listen) Unable to listen on socket");
    }
    printf("Listening on %s:%d\n", hostname, LISTEN_PORT);

    time(&timeVal);
    timeStr = ctime(&timeVal);
    timeStr[strlen(timeStr)-2] = '\0';
    fprintf(fp,
            "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
            "\"time\":\"%s\",\"v\":0,\"level\":30,\"local\":{"
            "\"address\":\"%d\",\"port\":%d},\"msg\":\"%s\"}\n",
            hostname, (int) pid, timeStr,
            server_addr.sin_addr.s_addr, LISTEN_PORT, "Server started.");
    fflush(fp);

    /* Use multiple processes */
    num_forks = (int) sysconf(_SC_NPROCESSORS_ONLN);
    for (i = 0; i < num_forks; ++i)
        if ((childpid = fork()) <= 0)
            break;

    /* Setup epoll */
    if ((epoll_fd = epoll_create(1)) == -1)
        perror("(epoll_create) Unable to create a descriptor");
    event.events  = EPOLLIN | EPOLLET;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
        perror("(epoll_ctl) Unable to add event");
    equeue = (struct epoll_event *) calloc(EV_BUFSIZE, sizeof event);

    /* Main event loop */
    for (;;) {
        /* NOTE: Blocks until there is an event */
        num_events = epoll_wait(epoll_fd, equeue, EV_BUFSIZE, -1);

        for (i = 0; i < num_events; ++i) {
            if ((equeue[i].events & EPOLLERR)
                || (equeue[i].events & EPOLLHUP)
                || (!(equeue[i].events & EPOLLIN))) {
                perror("(epoll_wait) Unhandled error");
                close(equeue[i].data.fd);
                continue;
            }

            /* accept_connection(); */
            if (server_fd == equeue[i].data.fd) {
                int client_fd;
                char host[NI_MAXHOST],
                     port[NI_MAXSERV];
                struct sockaddr_in client_addr;
                socklen_t client_size = sizeof client_addr;

                for (;;) {
                    if ((client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_size)) == -1) {
                        if ( !(errno == EAGAIN || errno == EWOULDBLOCK) ) {
                            /* NOTE: Likely to be EMFILE. Reached process fd limit. */
                            perror("(accept) Connection lost");
                        }
                        break;
                    }
                    if (getnameinfo(
                            (struct sockaddr *) &client_addr, client_size,
                            host, sizeof host,
                            port, sizeof port,
                            NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
                        /*
                        printf("Accepted connection on descriptor %d "
                                "(%s:%s)\n", client_fd, host, port);
                        */
                        sem_wait(&mutex);
                        time(&timeVal);
                        timeStr = ctime(&timeVal);
                        timeStr[strlen(timeStr)-2] = '\0';
                        fprintf(fp,
                                "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
                                "\"time\":\"%s\",\"v\":0,\"level\":20,\"remote\":{"
                                "\"address\":\"%s\",\"port\":%s},\"msg\":\"%s\"}\n",
                                hostname, (int) pid, timeStr,
                                host, port, "Accepted connection.");
                        fflush(fp);
                        sem_post(&mutex);

                        if (setNonblock(client_fd) == -1) {
                            abort();
                        }
                        event.events  = EPOLLIN | EPOLLET;
                        event.data.fd = client_fd;
                        if ((epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event)) == -1) {
                            perror("(epoll_ctl) Unable to add event");
                        }
                    }
                }

            /* read_connection(); */
            } else {
                ssize_t total = 0;
                ssize_t count;
                char buf[BUFSIZ];

                struct sockaddr_in addr;
                socklen_t len =  sizeof addr;

                while ((count = read(equeue[i].data.fd, buf, sizeof buf))) {
                    if (count == -1) {
                        if ( !(errno == EAGAIN || errno == EWOULDBLOCK) ) {
                            perror("(read) Unable to read from descriptor");
                            close(equeue[i].data.fd);
                        }
                        break;
                    }
                    total += count;

                    /* Echo response. NOTE: Use fd 1 to write to stdout. */
                    if ((write(equeue[i].data.fd, buf, count)) == -1) {
                        if ( !(errno == EAGAIN || errno == EWOULDBLOCK) ) {
                            perror("(write) Unable to write to descriptor");
                            close(equeue[i].data.fd);
                        }
                    }
                }
                if (count == 0) close(equeue[i].data.fd);

                sem_wait(&mutex);
                time(&timeVal);
                timeStr = ctime(&timeVal);
                timeStr[strlen(timeStr)-2] = '\0';
                getpeername(equeue[i].data.fd, (struct sockaddr*) &addr, &len);
                if (total > 0) {
                    fprintf(fp,
                            "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
                            "\"time\":\"%s\",\"v\":0,\"level\":20,\"remote\":{"
                            "\"address\":\"%s\",\"port\":%d,"
                            "\"bytesWritten\":%d},\"msg\":\"%s\"}\n",
                            hostname, (int) pid, timeStr,
                            inet_ntoa(addr.sin_addr), addr.sin_port,
                            (int) total, "Data received.");

                } else {
                    fprintf(fp,
                            "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
                            "\"time\":\"%s\",\"v\":0,\"level\":20,\"remote\":{"
                            "\"address\":\"%s\",\"port\":%d},\"msg\":\"%s\"}\n",
                            hostname, (int) pid, timeStr,
                            inet_ntoa(addr.sin_addr), addr.sin_port, "Connection closed.");
                }
                fflush(fp);
                sem_post(&mutex);
            }
        }
    }
    free(equeue);
    close(server_fd);
    return EXIT_SUCCESS;
}

static int setNonblock(int fd) {
    int flags;
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        perror("(fcntl) Unable to get flags");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("(fcntl) Unable to set flags");
        return -1;
    }
    return 0;
}
