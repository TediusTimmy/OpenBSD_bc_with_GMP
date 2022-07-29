#!/bin/bash
# flex -o scan.c scan.l
# byacc -d -o bc.c bc.y

# OpenSSL BN
gcc -DYY_NO_INPUT -D_GNU_SOURCE -O3 -Wall -Wpedantic -o bcOpenBSD bc.c scan.c tty.c ../dc/bcode.c ../dc/dc.c ../dc/inout.c ../dc/mem.c ../dc/stack.c -lcrypto -ledit
# GMP
gcc -DYY_NO_INPUT -D_GNU_SOURCE -O3 -Wall -Wpedantic -o bcOpenBSD_GMP bc.c scan.c tty.c ../dcGMP/bcode.c ../dcGMP/dc.c ../dcGMP/inout.c ../dcGMP/mem.c ../dcGMP/stack.c -lgmp -ledit