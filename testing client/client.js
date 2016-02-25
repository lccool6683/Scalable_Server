/// Client class object
'use strict';

const net = require('net');
const EventEmitter = require('events').EventEmitter;

class EchoClient extends EventEmitter {
    constructor(content) {
        super();
        this.content = content;
        this.sendRequests = 0;
        this.on('send', () => this.sendRequests++);
    }

    connect(conf, pred) {
        this.socket = net.connect(conf);
        this.socket.on('connect', () => {
            this.connectionTime = process.hrtime();
            this.emit('connect', {
                local:  {address: this.socket.localAddress,  port: this.socket.localPort},
                remote: {address: this.socket.remoteAddress, port: this.socket.remotePort}
            });
            this.send();
        });
        this.socket.on('data', buf => {
            let diffTime = this.sendTime ? process.hrtime(this.sendTime) : null;
            this.emit('data', {
                requestNo: this.sendRequests,
                responseTime: diffTime ? diffTime[0]*1e9+diffTime[1] : null
            });
            if (pred && pred(this)) {
                this.send();
            } else {
                this.disconnect();
            }
        });
        this.socket.on('error', err       => this.emit('error', err));
        this.socket.on('close', had_error => {
            this.connectionDuration = this.connectionTime ?
                                        process.hrtime(this.connectionTime) : [0, 0];
            this.emit('close', had_error);
        });
    }

    send(content) {
        if (this.socket) {
            this.sendTime = process.hrtime();
            this.socket.write(content ? content : this.content, this.emit('send'));
        }
    }

    disconnect() {
        this.socket.end();
        this.socket.unref();
        setTimeout(() => { if (this.socket) this.socket.destroy(); }, 3000)
            .unref();
    }

    get bytesRead() { return this.socket ? this.socket.bytesRead : 0; }
    get bytesWritten() { return this.socket ? this.socket.bytesWritten : 0; }
}
module.exports = EchoClient;
