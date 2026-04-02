cmake_minimum_required(VERSION 3.22)

project(AvionicsTests LANGUAGES CXX)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.4.12")

# ---- Create Test Executable ----

add_executable(AircraftClientTests 
    source/aircraft/AircraftUnitTests.cpp
)

target_link_libraries(AircraftClientTests PRIVATE doctest::doctest Aircraft)
target_include_directories(AircraftClientTests PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ---- Discover Tests ----

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AircraftClientTests)
