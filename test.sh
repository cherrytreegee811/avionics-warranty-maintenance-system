#!/bin/bash
# Test script for Avionics Warranty Maintenance System

set -e

if [ ! -d "build" ]; then
    echo "Build directory not found. Please run ./build.sh first."
    exit 1
fi

echo "Running tests..."
cd build
ctest --output-on-failure
cd ..

echo "All tests passed"
