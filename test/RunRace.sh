#!/bin/bash -x

# Warm up
$1 $2 > /dev/null

# Time trial, ten laps.
time $1 $2
time $1 $2
time $1 $2
time $1 $2
time $1 $2
time $1 $2
time $1 $2
time $1 $2
time $1 $2
time $1 $2
