@echo off
REM Test script for Avionics Warranty Maintenance System

if not exist build (
    echo Build directory not found. Please run build.bat first.
    exit /b 1
)

echo Running tests...
cd build
ctest --output-on-failure
if %errorlevel% neq 0 (
    echo Tests failed
    exit /b %errorlevel%
)

cd ..
echo All tests passed
