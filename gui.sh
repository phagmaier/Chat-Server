#!/bin/bash

# Build directory setup
mkdir -p build || exit 1
cd build || exit 1

cmake ..
make
echo "RUNNING SERVER..."
./runme
