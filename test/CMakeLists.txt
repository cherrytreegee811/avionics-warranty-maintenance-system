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

# ---- Normalize doctest target name (fixes missing ALIAS target) ----
if(TARGET doctest::doctest)
  set(DOCTEST_LIB_TARGET doctest::doctest)
elseif(TARGET doctest)
  set(DOCTEST_LIB_TARGET doctest)
else()
  message(FATAL_ERROR "doctest target not found (expected doctest::doctest or doctest)")
endif()

# ---- Helper library (shared test utilities) ----
add_library(TestHelpers STATIC source/helpers/TestHelpers.cpp)
target_include_directories(TestHelpers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(TestHelpers PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ---- Create Test Executables ----
add_executable(AircraftUnitTests source/aircraft/AircraftUnitTests.cpp)
target_link_libraries(AircraftUnitTests PRIVATE ${DOCTEST_LIB_TARGET} Aircraft TestHelpers)
target_include_directories(AircraftUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(AircraftUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(PacketUnitTests source/common/PacketUnitTests.cpp)
target_link_libraries(PacketUnitTests PRIVATE ${DOCTEST_LIB_TARGET} Common)
target_include_directories(PacketUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(PacketUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(NetworkIntegrationTests source/common/NetworkIntegrationTests.cpp)
target_link_libraries(NetworkIntegrationTests PRIVATE ${DOCTEST_LIB_TARGET} Common MMA Aircraft)
target_include_directories(NetworkIntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(WIN32)
  target_link_libraries(NetworkIntegrationTests PRIVATE ws2_32 mswsock)
endif()
set_target_properties(NetworkIntegrationTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

add_executable(MMAUnitTests source/mma/MMAUnitTests.cpp)
target_link_libraries(MMAUnitTests PRIVATE ${DOCTEST_LIB_TARGET} MMA)
target_include_directories(MMAUnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(MMAUnitTests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# ---- Discover Tests ----
enable_testing()

# This include path is fine *if* doctest_SOURCE_DIR is set by CPM; keep it but fail early if not.
if(NOT DEFINED doctest_SOURCE_DIR)
  message(FATAL_ERROR "doctest_SOURCE_DIR not set; CPMAddPackage(doctest) did not populate it")
endif()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(AircraftUnitTests)
doctest_discover_tests(PacketUnitTests)
doctest_discover_tests(NetworkIntegrationTests)
doctest_discover_tests(MMAUnitTests)
