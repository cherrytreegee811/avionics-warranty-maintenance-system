cmake_minimum_required(VERSION 3.22)

project(AvionicsTests LANGUAGES CXX)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.5.0")
CPMAddPackage("gh:TheLartians/Format.cmake@1.8.3")

# ---- Helper library (shared test utilities) ----
add_library(TestHelpers STATIC source/helpers/TestHelpers.cpp)
target_include_directories(TestHelpers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(TestHelpers PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ---- Create Test Executables ----

# Aircraft Unit Tests
add_executable(AircraftUnitTests source/aircraft/AircraftUnitTests.cpp)
target_link_libraries(
  AircraftUnitTests PRIVATE doctest::doctest Aircraft spdlog::spdlog TestHelpers
)
target_include_directories(AircraftUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(AircraftUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# Packet Unit Tests
add_executable(PacketUnitTests source/common/PacketUnitTests.cpp)
target_link_libraries(PacketUnitTests PRIVATE doctest::doctest Common)
target_include_directories(PacketUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(PacketUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# Network Integration Tests
add_executable(NetworkIntegrationTests source/common/NetworkIntegrationTests.cpp)
target_link_libraries(NetworkIntegrationTests PRIVATE doctest::doctest Common MMA Aircraft)
target_include_directories(NetworkIntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(NetworkIntegrationTests PRIVATE ws2_32 mswsock)
endif()
set_target_properties(NetworkIntegrationTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# MMA Unit Tests
add_executable(MMAUnitTests source/mma/MMAUnitTests.cpp)
target_link_libraries(MMAUnitTests PRIVATE doctest::doctest MMA)
target_include_directories(MMAUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(MMAUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)


# ---- Discover Tests ----

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AircraftUnitTests)
doctest_discover_tests(PacketUnitTests)
doctest_discover_tests(NetworkIntegrationTests)
doctest_discover_tests(MMAUnitTests)
