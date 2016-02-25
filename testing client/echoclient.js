// TODO - Listen and create/destroy via TCP

'use strict';

const EchoClient = require('./client.js');

const fs = require('fs');
const config = JSON.parse(fs.readFileSync('config.json', 'utf8'));
const content = config.contentUrl ? fs.readFileSync(config.contentUrl) : config.content;

const bunyan = require('bunyan');
const log = bunyan.createLogger({
    name: 'client',
    level: 'debug',
    streams: [{path: `${config.logFile}`}]
});

const cluster = require('cluster');

if (cluster.isMaster) {
    const numCPUs = config.multicore ? require('os').cpus().length : 1;
    for (let i = 0; i < numCPUs; ++i) {
        cluster.fork()
    }

    let num_connections = {};
    if (config.showConnections) {
        setInterval(() => {
            let clients = Object.keys(num_connections);
            if (clients.length > 0) {
                let current_connections = 0;
                for (let i in clients) {
                    current_connections += num_connections[clients[i]];
                }
                console.log(`Active connections: ${current_connections}`);
            }
        }, config.showConnections).unref();
    }

    cluster.on('message', msg => {
        if (msg.type && msg.type === 'connections') {
            num_connections[msg.id] = msg.connections;
        }
    });
    cluster.on('exit', (worker, code, signal) => {
        if (!worker.suicide) {
            if (signal) {
                console.log(`Worker was killed by signal: ${signal}`);
            } else if (code !== 0) {
                console.log(`Worker exited with error code: ${code}`);
                setTimeout(cluster.fork, config.reconnect);
            }
        }
        delete num_connections[worker.id];
    });

    process.on('SIGINT', () => {
        setInterval(() => {
            for (let id in cluster.workers) {
                cluster.workers[id].kill();
            }
        }).unref();
    });
} else {
    let connections = {};
    let connectionTotal = config.connections;

//     let i = 0;
//     let j = 0;
//     setInterval(() => {
//     for (j += connectionTotal; i < j; ++i) {
    for (let i = 0; i < connectionTotal; ++i) {
        createEchoClient(i, connections);
    }
//     }, 5000);
    if (config.showConnections) {
        setInterval(() => {
            process.send({
                type: 'connections',
                id: process.pid,
                connections: Object.keys(connections).length
            });
        }, config.showConnections).unref();
    }
}

function createEchoClient(id, connections, waitDuration) {
    let client = new EchoClient(content);
    client.on('connect', conn => log.debug(conn, 'Connected to server.'));
    client.on('data',    info => log.debug(info, 'Received data from server.'));
    client.on('error',   err  => log.warn(err,   'Connection abnormally terminated.'));
    client.on('close',   had_error => {
        log.debug({
            connectionTime: client.connectionDuration[0]*1e9+client.connectionDuration[1],
            requests: client.sendRequests,
            bytesSent: client.bytesWritten
        }, 'Connection closed.');

        if (had_error) {
            setTimeout(() => createEchoClient(id, connections, (waitDuration || 1)*10),
                       (waitDuration || 0));
        } else {
            delete connections[id];
            if (Object.keys(connections).length == 0) process.exit(0);
        }
    });
    client.connect({host: config.address, port: Number(config.port)}, repeatCondition());
    connections[id] = client;
}

function repeatCondition() {
    let conditions = [
        'time',
        'repeat',
        'timeout'
    ].map(b => {
        if (config[b] != null) {
            switch (b) {
                case 'time':
                    return client => config[b] > process.hrtime(client.connectionTime)[0];
                case 'repeat':
                    return client => config[b] > client.sendRequests;
                case 'timeout':
                    return client => {
                        let diffTime = process.hrtime(client.sendTime);
                        config[b] > diffTime[0]*1e9+diffTime[1];
                    }
                default:
                    break;
            }
        }
    }).reduce((a, b) => b && a.concat(b) || a, []);
    return client => {
        for (let i in conditions) {
            if (conditions[i](client))
                return true;
        }
        return false;
    };
}
