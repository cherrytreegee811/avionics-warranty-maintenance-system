#!/bin/bash
# Run script for Avionics Warranty Maintenance System

set -e

if [ ! -d "build" ]; then
    echo "Build directory not found. Please run ./build.sh first."
    exit 1
fi

echo "Running Aircraft and MMA components..."
echo ""
echo "=== Aircraft Component ==="
./build/src/AircraftApp
echo ""
echo "=== MMA Component ==="
./build/src/MMAApp
echo ""
echo "Execution completed"
