'use strict';

require('net')
    .createServer()
    .on('connection', client => client.pipe(client))
    .listen({
        host: process.argv[2]         || '0.0.0.0',
        port: Number(process.argv[3]) || 1234
    });
