#!/bin/bash

gcc -Wall -Wextra -Wpedantic -O3 -march=native -o dc dc.c
#byacc -o bc.c bc.y
gcc -Wall -Wextra -Wpedantic -O3 -march=native -o bc bc.c
