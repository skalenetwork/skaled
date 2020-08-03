#!/bin/bash
while true; do
ps --pid $1 -o pid=,rss=,vsz= >> mem.log
gnuplot show_mem.plt
sleep 1
done
