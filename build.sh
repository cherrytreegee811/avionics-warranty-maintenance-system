#!/bin/bash
# Build script for Avionics Warranty Maintenance System

set -e

if [ ! -d "build" ]; then
    mkdir build
fi

echo "Building all targets..."
cmake -S all -B build
cmake --build build

echo "Build completed successfully"
