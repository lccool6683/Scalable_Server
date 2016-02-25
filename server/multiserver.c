#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>


#define HOST_NAME_MAX 255
#define EV_BUFSIZE 8
#define LISTEN_PORT 1234

void *readClient(void *ptr);

sem_t mutex;

int main(void) {
    FILE *fp = fopen("server.log", "a");
    char hostname[HOST_NAME_MAX];

    time_t timeVal;
    char *timeStr;

    pid_t pid;

    int optval;
    int server_fd;
    struct sockaddr_in server_addr;

    int *client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof client_addr;

    pthread_t thread;

    sem_init(&mutex, 0, 1);

    pid = getpid();
    if (gethostname(hostname, HOST_NAME_MAX) == -1) {
        perror("(gethostname) Unable to get host name");
    }

    /* Initialize server */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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

    /* Main accept loop */
    for (;;) {
        client_fd = (int *) malloc(sizeof *client_fd);
        if ((*(int *)client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_size)) == -1) {
            perror("(accept) Connection lost");
        } else {
            pthread_create(&thread, NULL, readClient, (void *) client_fd);
            pthread_detach(thread);
        }
    }
    close(server_fd);
    return EXIT_SUCCESS;
}

void *readClient(void *ptr) {
    FILE *fp = fopen("server.log", "a");
    int fd;

    char hostname[HOST_NAME_MAX];
    pid_t pid;

    ssize_t total = 0;
    ssize_t count;
    char buf[BUFSIZ];

    time_t timeVal;
    char *timeStr;

    char *host;
    int port;

    struct sockaddr_in addr;
    socklen_t len =  sizeof addr;

    if (!ptr) pthread_exit(0);
    fd = *(int *)ptr;

    getpeername(fd, (struct sockaddr*) &addr, &len);
    host = inet_ntoa(addr.sin_addr);
    port = addr.sin_port;
    pid = getpid();
    if (gethostname(hostname, HOST_NAME_MAX) == -1) {
        perror("(gethostname) Unable to get host name");
    }

    sem_wait(&mutex);
    time(&timeVal);
    timeStr = ctime(&timeVal);
    timeStr[strlen(timeStr)-2] = '\0';
    fprintf(fp,
            "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
            "\"time\":\"%s\",\"v\":0,\"level\":20,\"remote\":{"
            "\"address\":\"%s\",\"port\":%d},\"msg\":\"%s\"}\n",
            hostname, (int) pid, timeStr,
            host, port, "Accepted connection.");
    fflush(fp);
    sem_post(&mutex);

    while ((count = read(fd, buf, sizeof buf))) {
        if (count == -1) {
            perror("(read) Unable to read from descriptor");
            break;
        }
        total += count;

        /* Echo response. NOTE: Use fd 1 to write to stdout. */
        if ((write(fd, buf, count)) == -1) {
            perror("(write) Unable to write to descriptor");
            break;
        }
        sem_wait(&mutex);
        time(&timeVal);
        timeStr = ctime(&timeVal);
        timeStr[strlen(timeStr)-2] = '\0';
        fprintf(fp,
                "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
                "\"time\":\"%s\",\"v\":0,\"level\":20,\"remote\":{"
                "\"address\":\"%s\",\"port\":%d,"
                "\"bytesWritten\":%d},\"msg\":\"%s\"}\n",
                hostname, (int) pid, timeStr,
                host, port, (int) count, "Data received.");
        fflush(fp);
        sem_post(&mutex);
    }
    close(fd);

    sem_wait(&mutex);
    time(&timeVal);
    timeStr = ctime(&timeVal);
    timeStr[strlen(timeStr)-2] = '\0';
    fprintf(fp,
            "{\"name\":\"server\",\"hostname\":\"%s\",\"pid\":%d,"
            "\"time\":\"%s\",\"v\":0,\"level\":20,\"remote\":{"
            "\"address\":\"%s\",\"port\":%d},\"msg\":\"%s\"}\n",
            hostname, (int) pid, timeStr,
            host, port, "Connection closed.");
    fflush(fp);
    sem_post(&mutex);

    free(ptr);
    pthread_exit(0);
}
