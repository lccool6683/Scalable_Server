'use strict';

const readline = require('readline');
const fs = require('fs');

let rl = readline.createInterface({
    input: fs.createReadStream(`server.log`)
});
let clients = {};

rl.on('line', line => {
    let logEntry = JSON.parse(line);
    if (logEntry.remote && logEntry.remote.bytesWritten) {
        if (!clients[logEntry.remote.address]) {
            clients[logEntry.remote.address] = {
                requests:     0,
                bytesWritten: 0
            };
        }
        clients[logEntry.remote.address].requests++;
        clients[logEntry.remote.address].bytesWritten += logEntry.remote.bytesWritten;
    }
});
rl.on('close', () => {
    let filename = `server.csv`;

    fs.writeFileSync(filename, `Hostname,Requests,BytesTransferred\n`);
    Object
        .keys(clients)
        .forEach((addr) => {
            let data = `${addr},${clients[addr].requests},${clients[addr].bytesWritten}\n`;
            fs.writeFile(filename, data, {flag: 'a'});
        });
});
