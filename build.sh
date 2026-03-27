#!/bin/bash
# Build script for Avionics Warranty Maintenance System

set -e

if [ ! -d "build" ]; then
    mkdir build
fi

echo "Building all targets..."
cmake -S all -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

echo "Build completed successfully"
