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

# Network Integration Tests
 add_executable(NetworkIntegrationTests source/common/NetworkIntegrationTests.cpp)
 target_link_libraries(NetworkIntegrationTests 
    PRIVATE 
    doctest::doctest 
    MMA             # Provides the server logic
    Aircraft        # Provides the client logic
    Common          # Provides Packet serialization
    spdlog::spdlog  # Logging dependency
 )

target_include_directories(NetworkIntegrationTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include   # your project headers
    ${ASIO_INCLUDE_DIRS}                      # Asio headers
)
# ---- Discover Tests ----

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AircraftUnitTests)
doctest_discover_tests(PacketUnitTests)
doctest_discover_tests(NetworkIntegrationTests)
