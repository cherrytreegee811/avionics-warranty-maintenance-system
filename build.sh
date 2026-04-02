#!/bin/bash
# Build script for Avionics Warranty Maintenance System

python -m venv venv
source ./venv/bin/activate
pip install -r requirements.txt

cmake-format --config-files .cmake-format -i $(find . -name "CMakeLists.txt" -type f)

find . -name "*.cpp" -o -name "*.h" | grep -v "./build" | grep -v "./_deps" | grep -v "./cpm_modules" | xargs clang-format

set -e

if [ ! -d "build" ]; then
    mkdir build
fi

echo "Building all targets..."
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build

echo "Build completed successfully"