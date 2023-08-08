#!/bin/sh

if [ ! build/ ]; then
    mkdir build
fi

cmake .
cmake --build .

