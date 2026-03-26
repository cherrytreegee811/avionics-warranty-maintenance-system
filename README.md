[![macos](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/macos.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/macos.yml)
[![windows](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/windows.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/windows.yml)
[![ubuntu](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/ubuntu.yml)
[![style](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/style.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/style.yml)
[![install](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/install.yml/badge.svg)](https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system/actions/workflows/install.yml)

# Aircraft Maintenance Management System

A distributed C++ application simulating aircraft (client) and Maintenance Management Application (MMA) (server) communication for aircraft state management, diagnostic data transfer, and maintenance tracking.

## Contributors
- Isaiah Andrews
- Jonathan Taylor
- Salah Salame

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
- **Testing Suite**: Unit, integration, and system tests using Google Test

## Technology Stack

- **Language**: C++17/20
- **Build System**: CMake 3.15+
- **Testing**: Google Test / MSTest
- **Network**: TCP/IP sockets (ports: server 8000, client 5000)
- **Code Quality**: MISRA C++ 2012 compliance checks
- **CI/CD**: GitHub Actions
- **Code Coverage**: codecov
- **Formatting**: clang-format, cmake-format

## Building the Project

### Prerequisites

- CMake 3.15 or higher
- Ninja
- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Google Test (automatically downloaded via CPM.cmake if not present)
- Optional: Doxygen for documentation generation

### Build Everything at Once

```bash
# Clone the repository
git clone https://github.com/CherryTreeGee811/avionics-warranty-maintenance-system.git
cd avionics-warranty-maintenance-system

# Configure and build
cmake -S all -B build
cmake --build build

# Run the test suite
./build/test/AircraftTests

# Run the client application
./build/client/Airplane

# Run the server application (in another terminal)
./build/server/MMA
```
### Build Client and Server Separately

#### Build the Client (Airplane)
```bash
cmake -S client -B build/client
cmake --build build/client
./build/client/Airplane
```

#### Build the Server (MMA)
```
cmake -S server -B build/server
cmake --build build/server
./build/server/MMA
```

### Build and Run Tests
```
cmake -S test -B build/test
cmake --build build/test
CTEST_OUTPUT_ON_FAILURE=1 cmake --build build/test --target tes
```

#### To enable code coverage
```
cmake -S test -B build/test -DENABLE_TEST_COVERAGE=1
cmake --build build/test --target coverage
```

### Build Documentation
```
cmake -S documentation -B build/doc
cmake --build build/doc --target GenerateDocs
# View documentation
open build/doc/doxygen/html/index.html
```

### Code Formatting
```
cmake -S test -B build/test
cmake --build build/test --target format   # View changes
cmake --build build/test --target fix-format # Apply changes
```

### MISRA Compliance
- **TODO**
