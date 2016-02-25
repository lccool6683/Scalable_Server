#!/bin/bash

# Streamlines the graphing process
# cat logs/c*.log > client.log
node generateCsv.js client.log client.csv
node graphCsv.js client.csv client.png
xdg-open client.png && echo -e '-- Finished --\nSaved as: client.png'
