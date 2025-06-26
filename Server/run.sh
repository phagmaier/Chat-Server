#!/bin/bash

mkdir -p build || exit 1
cd build || exit 1

cmake ..
make
./server
