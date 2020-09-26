#!/usr/bin/env sh
set -e

echo "compiling..."
clang++ -o bench -O3 -std=c++17 benchmark.cpp -lfmt

printf "printf:\t"
time -f "\t%e real\t%U user\t%S sys" ./bench printf

printf "zpr:\t"
time -f "\t%e real\t%U user\t%S sys" ./bench zpr

printf "fmt:\t"
time -f "\t%e real\t%U user\t%S sys" ./bench fmt

printf "fmt2:\t"
time -f "\t%e real\t%U user\t%S sys" ./bench fmt2

