#!/bin/bash

mkdir -p bin || exit 1
cd bin || exit 1

cmake ..
make
./client
