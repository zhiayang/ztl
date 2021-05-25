#!/usr/bin/env fish

printf "printf:\t"
time ./bench printf

printf "zpr:\t"
time ./bench zpr

printf "fmt:\t"
time ./bench fmt

printf "fmt2:\t"
time ./bench fmt2

