[![macos](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/macos.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/macos.yml)
[![windows](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/windows.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/windows.yml)
[![ubuntu](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/ubuntu.yml)
[![style](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/style.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/style.yml)
[![install](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/install.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/install.yml)
[![codecov](https://codecov.io/gh/CherryTreeGee811/avionics-warranty-maintenance-system/branch/master/graph/badge.svg)](https://codecov.io/gh/CherryTreeGee811/avionics-warranty-maintenance-system)

# Avionics Warranty & Maintenance Management System

A distributed C++ application simulating aircraft (client) and Maintenance Management Application (MMA) (server) communication for aircraft state management, diagnostic data transfer, and maintenance tracking.

## Contributors
- Isaiah Andrews (<i>SemiDoge</i>)
- Jonathan Taylor (<i>CherryTreeGee811</i>)
- Salah Salame (<i>TechNerd-019</i>)

## Project Overview

This project implements a client-server system where:
- **Airplane (Client)**: Simulates aircraft systems, maintains operational state, and communicates with MMA
- **MMA (Server)**: Manages aircraft states, logs events, and coordinates maintenance activities

The system fulfills requirements for state machine management, diagnostic data transfer (including 1MB schematic images), packet logging, and connection verification.

## Key Features

- **Distributed Architecture**: Separate client and server applications communicating over TCP/IP
- **State Machine**: Aircraft operational states (STANDBY, DIAGNOSTIC, MAINTENANCE, FAULT) with validated transitions
- **CLI/TUI Interface**: Pilot-facing interface for viewing state, fault codes, warranty status, and maintenance history
- **Large Data Transfer**: 1MB schematic image and diagnostic information transfer during DIAGNOSTIC state
- **Comprehensive Logging**: All transmitted/received packets logged to files on both client and server
- **Connection Verification**: Secure handshake protocol before command acceptance
- **MISRA Compliance**: Code adheres to MISRA C++ 2012 guidelines
- **Testing Suite**: Unit, integration, and system tests using Doctest

## Technology Stack

- **Language**: C++23
- **Build System**: CMake 3.22+
- **Testing**: Doctest
- **Network**: TCP/IP sockets (ports: server 8000, client 5000)
- **Code Quality**: MISRA C++ 2012 compliance checks
- **CI/CD**: GitHub Actions
- **Code Coverage**: codecov
- **Formatting**: clang-format, cmake-format

## Getting Started

### Prerequisites

- CMake 3.22 or higher
- Ninja
- A C++23-capable compiler (GCC 11+, Clang 14+, MSVC 19.29+, or Strawberry GCC on Windows)
- Python 3 with `pip` if you want to use the provided build scripts
- Optional: Doxygen for documentation generation

### Build

The quickest path is to use the provided platform script from the repository root:

```bash
# Linux/macOS
./build.sh
```

```bat
:: Windows
build.bat
```

Those scripts create a local Python virtual environment, install the formatting tools, format the source tree, configure CMake, and build the project into `build/`.

If you prefer to build manually, run:

```bash
cmake -S . -B build
cmake --build build
```

### Run the applications

The build produces two executables in `build/`:

- `MMAApp` / `MMAApp.exe`
- `AircraftApp` / `AircraftApp.exe`

Start the MMA server first, then launch the aircraft client in a second terminal:

```bash
# Terminal 1
./build/MMAApp
```

```bash
# Terminal 2
./build/AircraftApp
```

On Windows PowerShell, use:

```powershell
# Terminal 1
.\build\MMAApp.exe
```

```powershell
# Terminal 2
.\build\AircraftApp.exe
```

The MMA server listens on `127.0.0.1:8000`, and the aircraft client connects to that address automatically.

### Run tests

After building, run the test suite from the `build/` directory:

```bash
cd build
ctest --output-on-failure
```

On Windows PowerShell:

```powershell
Set-Location build
ctest --output-on-failure
```

### Logs and output

- Runtime logs are written to `logs/`
- The applications print status information to the console while they run
- Test output is available through `ctest --output-on-failure`
