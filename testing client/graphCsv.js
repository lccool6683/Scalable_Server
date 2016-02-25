/**
 * Wrapper for using the gnuplot program. Converts 2d CSV files to a PNG graph.
 */
'use strict';

const gnuplot = require('gnuplot');

let infile  = process.argv[2];
let outfile = process.argv[3];

gnuplot()
    .set('terminal pngcairo enh')
    .set(`output '${outfile}'`)
    .set('datafile separator ","')
    .plot(`'${infile}' u 1:2 notitle`, {end: true});
