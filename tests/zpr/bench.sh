#!/usr/bin/env fish

if command test benchmark.cpp -nt bench
	echo "compiling..."
	clang++ -o bench -O3 -std=c++17 benchmark.cpp -I../../ -lfmt
end

printf "printf:\t"
time ./bench printf

printf "zpr:\t"
time ./bench zpr

printf "zpr2:\t"
time ./bench zpr2

printf "fmt:\t"
time ./bench fmt

printf "fmt2:\t"
time ./bench fmt2

