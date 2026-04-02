cmake_minimum_required(VERSION 3.22)

project(AvionicsTests LANGUAGES CXX)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.5.0")
CPMAddPackage("gh:TheLartians/Format.cmake@1.8.3")

# ---- Create Test Executables ----

# Aircraft Unit Tests
add_executable(AircraftUnitTests source/aircraft/AircraftUnitTests.cpp)
target_link_libraries(AircraftUnitTests PRIVATE doctest::doctest Aircraft)
target_include_directories(AircraftUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Packet Unit Tests
add_executable(PacketUnitTests source/common/PacketUnitTests.cpp)
target_link_libraries(PacketUnitTests PRIVATE doctest::doctest Common)
target_include_directories(PacketUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# MMA Unit Tests
add_executable(MMAUnitTests source/mma/MMAUnitTests.cpp)
target_link_libraries(MMAUnitTests PRIVATE doctest::doctest MMA)
target_include_directories(MMAUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ---- Discover Tests ----

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AircraftUnitTests)
doctest_discover_tests(PacketUnitTests)
doctest_discover_tests(MMAUnitTests)
