/**
 * Converts client log files into CSV format.
 */
'use strict';

const readline = require('readline');
const fs = require('fs');

const rl = readline.createInterface({
    input: fs.createReadStream(`${process.argv[2]}`)
});

/// The earliest logged entry.
let origin;

rl.on('line', line => {
    let date = Date.parse(JSON.parse(line).time).valueOf();
    if (origin == null) origin = date;
    if (origin > date)  origin = date;
});
rl.on('close', () => {
    let rl2 = readline.createInterface({
        input: fs.createReadStream(`${process.argv[2]}`)
    });
    let output = fs.createWriteStream(`${process.argv[3]}`);

    console.log(`Origin: ${new Date(origin)}`, origin);

    rl2.setMaxListeners(0);
    rl2.on('line', line => {
        try {
            let ln = JSON.parse(line);
            let template = `${Date.parse(ln.time).valueOf()-origin},${ln.responseTime}\n`;
            if (output.write(template) == false) {
                rl2.pause();
                output.once('drain', () => {
                    output.write(template)
                    rl2.resume();
                });
            }
        } catch (e) {
            console.log(e, line);
        }
    });
});
