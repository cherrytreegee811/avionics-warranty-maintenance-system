@echo off
REM Build script for Avionics Warranty Maintenance System

setlocal enabledelayedexpansion

python -m venv venv
if %errorlevel% neq 0 (
    echo Failed to create virtual environment
    exit /b %errorlevel%
)

call venv\Scripts\activate.bat
if %errorlevel% neq 0 (
    echo Failed to activate virtual environment
    exit /b %errorlevel%
)

pip install -r requirements.txt
if %errorlevel% neq 0 (
    echo Failed to install Python dependencies
    exit /b %errorlevel%
)

for /f "delims=" %%f in ('dir /s /b CMakeLists.txt ^| findstr /i /v "\\build\\" ^| findstr /i /v "\\_deps\\" ^| findstr /i /v "\\cpm_modules\\"') do (
    cmake-format --config-files .cmake-format -i "%%f"
    if !errorlevel! neq 0 (
        echo CMake formatting failed for %%f
        exit /b !errorlevel!
    )
)

for /f "delims=" %%f in ('dir /s /b *.cpp *.h ^| findstr /i /v "\\build\\" ^| findstr /i /v "\\_deps\\" ^| findstr /i /v "\\cpm_modules\\"') do (
    clang-format -i "%%f"
    if !errorlevel! neq 0 (
        echo Clang formatting failed for %%f
        exit /b !errorlevel!
    )
)

if not exist build (
    mkdir build
)

echo Building all targets...
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
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