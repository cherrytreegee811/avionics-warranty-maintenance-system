#!/bin/bash
# Build script for Avionics Warranty Maintenance System

python -m venv venv
source ./venv/bin/activate
pip install -r requirements.txt

cmake-format --config-files .cmake-format -i $(find . -name "CMakeLists.txt" -type f | grep -v "./cmake/")

find . -name "*.cpp" -o -name "*.h" | grep -v "./build" | grep -v "./_deps" | grep -v "./cpm_modules" | xargs clang-format -i

set -e

if [ ! -d "build" ]; then
    mkdir build
fi

echo "Building all targets..."
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

# Seed Warranty Data for now
# TODO: Remove and get the warranty data from the landed planes
echo "12345,1,2027-12-31,Aviation Warranty Corp" > mma_warranty_data.csv
echo "23456,0,2025-07-20,Aviation Warranty Corp" >> mma_warranty_data.csv

echo "Build completed successfully"