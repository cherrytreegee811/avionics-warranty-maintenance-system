@echo off
REM Build script for Avionics Warranty Maintenance System

if not exist build (
    mkdir build
)

echo Building all targets...
cmake -S all -B build
if %errorlevel% neq 0 (
    echo CMake configuration failed
    exit /b %errorlevel%
)

cmake --build build
if %errorlevel% neq 0 (
    echo Build failed
    exit /b %errorlevel%
)

echo Build completed successfully
