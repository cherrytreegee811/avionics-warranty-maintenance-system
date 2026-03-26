@echo off
REM Run script for Avionics Warranty Maintenance System

if not exist build (
    echo Build directory not found. Please run build.bat first.
    exit /b 1
)

echo Running Aircraft and MMA components...
echo.
echo === Aircraft Component ===
call build\src\AircraftApp.exe
echo.
echo === MMA Component ===
call build\src\MMAApp.exe
echo.
echo Execution completed
