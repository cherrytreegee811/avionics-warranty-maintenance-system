cmake_minimum_required(VERSION 3.22)

project(AvionicsTests LANGUAGES CXX)

include(../cmake/tools.cmake)

include(../cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.5.0")
CPMAddPackage(
  NAME Format.cmake
  VERSION 1.8.3
  GITHUB_REPOSITORY TheLartians/Format.cmake
  OPTIONS "CMAKE_FORMAT_EXCLUDE (^|/)cmake/.*"
)

if(TARGET doctest::doctest)
  set(DOCTEST_LIB_TARGET doctest::doctest)
elseif(TARGET doctest)
  set(DOCTEST_LIB_TARGET doctest)
else()
  message(FATAL_ERROR "doctest target not found (expected doctest::doctest or doctest)")
endif()

# ---- Helper library (shared test utilities) ----
add_library(
  TestHelpers STATIC source/helpers/TestHelpers.cpp source/helpers/MockMMA.cpp
                     source/helpers/MockAircraft.cpp
)
target_include_directories(
  TestHelpers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(TestHelpers PUBLIC ${asio_SOURCE_DIR}/include)
target_link_libraries(TestHelpers PRIVATE Common MMA)
set_target_properties(TestHelpers PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ---- Create Test Executables ----
add_executable(AircraftUnitTests source/aircraft/AircraftUnitTests.cpp)
target_link_libraries(AircraftUnitTests PRIVATE ${DOCTEST_LIB_TARGET} Aircraft TestHelpers)
target_include_directories(AircraftUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(AircraftUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(CliInterfaceUnitTests source/aircraft/CliInterfaceUnitTests.cpp)
target_link_libraries(CliInterfaceUnitTests PRIVATE ${DOCTEST_LIB_TARGET} Aircraft)
target_include_directories(CliInterfaceUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(CliInterfaceUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(CliInterfaceUsabilityTests source/aircraft/CliInterfaceUsabilityTests.cpp)
target_link_libraries(CliInterfaceUsabilityTests PRIVATE ${DOCTEST_LIB_TARGET} Aircraft)
target_include_directories(CliInterfaceUsabilityTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(
  CliInterfaceUsabilityTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON
)

add_executable(StateManagerUnitTests source/aircraft/StateManagerUnitTests.cpp)
target_link_libraries(StateManagerUnitTests PRIVATE ${DOCTEST_LIB_TARGET} Aircraft)
target_include_directories(StateManagerUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(StateManagerUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(PacketUnitTests source/common/PacketUnitTests.cpp)
target_link_libraries(PacketUnitTests PRIVATE ${DOCTEST_LIB_TARGET} Common)
target_include_directories(PacketUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(PacketUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(NetworkIntegrationTests source/common/NetworkIntegrationTests.cpp)
target_link_libraries(
  NetworkIntegrationTests PRIVATE ${DOCTEST_LIB_TARGET} Common MMA Aircraft TestHelpers
)
target_include_directories(NetworkIntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(NetworkIntegrationTests PRIVATE ws2_32 mswsock)
endif()
set_target_properties(NetworkIntegrationTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(SystemTests source/common/SystemTests.cpp)
target_link_libraries(SystemTests PRIVATE ${DOCTEST_LIB_TARGET} Common MMA Aircraft TestHelpers)
target_include_directories(SystemTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(SystemTests PRIVATE ws2_32 mswsock)
endif()
set_target_properties(SystemTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(AircraftIntegrationTests source/aircraft/AircraftIntegrationTests.cpp)
target_link_libraries(
  AircraftIntegrationTests PRIVATE ${DOCTEST_LIB_TARGET} Common MMA Aircraft TestHelpers
)
target_include_directories(AircraftIntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(AircraftIntegrationTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(MMAUnitTests source/mma/MMAUnitTests.cpp)
target_link_libraries(MMAUnitTests PRIVATE ${DOCTEST_LIB_TARGET} MMA)
target_include_directories(MMAUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(MMAUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(MMAIntegrationTests source/mma/MMAIntegrationTests.cpp)
target_link_libraries(MMAIntegrationTests PRIVATE ${DOCTEST_LIB_TARGET} Common MMA TestHelpers)
target_include_directories(MMAIntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(MMAIntegrationTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ---- Discover Tests ----
enable_testing()

if(NOT DEFINED doctest_SOURCE_DIR)
  message(FATAL_ERROR "doctest_SOURCE_DIR not set; CPMAddPackage(doctest) did not populate it")
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AircraftUnitTests)
doctest_discover_tests(CliInterfaceUnitTests)
doctest_discover_tests(CliInterfaceUsabilityTests)
doctest_discover_tests(StateManagerUnitTests)
doctest_discover_tests(PacketUnitTests)
doctest_discover_tests(NetworkIntegrationTests)
doctest_discover_tests(SystemTests)
doctest_discover_tests(MMAUnitTests)
doctest_discover_tests(AircraftIntegrationTests)
doctest_discover_tests(MMAIntegrationTests)
