cmake_minimum_required(VERSION 3.22)

project(
  AvionicsWarrantyMaintenanceSystem
  VERSION 1.0
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed.")
endif()

include(cmake/CPM.cmake)

CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.13.0")

CPMAddPackage(
  NAME spdlog
  GIT_TAG v1.17.0
  GITHUB_REPOSITORY gabime/spdlog
  OPTIONS "SPDLOG_BUILD_SHARED OFF" "SPDLOG_BUILD_EXAMPLE OFF" "SPDLOG_BUILD_TESTS OFF"
)

# Standalone Asio (no Boost)
CPMAddPackage(
  NAME asio
  GITHUB_REPOSITORY chriskohlhoff/asio
  GIT_TAG asio-1-38-0
)

# ---- Common Library ----
file(GLOB COMMON_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/common/*.h")
file(GLOB COMMON_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/source/common/*.cpp")
add_library(Common ${COMMON_HEADERS} ${COMMON_SOURCES})
set_target_properties(Common PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

target_include_directories(
  Common
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}> ${asio_SOURCE_DIR}/include
)

target_link_libraries(Common PUBLIC spdlog::spdlog)

# ---- Aircraft Library ----
file(GLOB AIRCRAFT_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/aircraft/*.h")
file(GLOB AIRCRAFT_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/source/aircraft/*.cpp")
list(FILTER AIRCRAFT_SOURCES EXCLUDE REGEX ".*/aircraft_main\\.cpp$")
add_library(Aircraft ${AIRCRAFT_HEADERS} ${AIRCRAFT_SOURCES})
set_target_properties(Aircraft PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_link_libraries(Aircraft PUBLIC Common)
target_link_libraries(Aircraft PUBLIC spdlog::spdlog)
if(WIN32)
  target_link_libraries(Aircraft PUBLIC ws2_32 mswsock)
endif()

# ---- MMA Library ----
file(GLOB MMA_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/mma/*.h")
file(GLOB MMA_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/source/mma/*.cpp")
list(FILTER MMA_SOURCES EXCLUDE REGEX ".*/mma_main\\.cpp$")
add_library(MMA ${MMA_HEADERS} ${MMA_SOURCES})
set_target_properties(MMA PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_link_libraries(MMA PUBLIC Common)
target_link_libraries(MMA PUBLIC spdlog::spdlog)
if(WIN32)
  target_link_libraries(MMA PUBLIC ws2_32 mswsock)
endif()

# ---- Executables ----
add_executable(AircraftApp ${PROJECT_SOURCE_DIR}/source/aircraft/aircraft_main.cpp)
target_link_libraries(AircraftApp PRIVATE Aircraft)

add_executable(MMAApp ${PROJECT_SOURCE_DIR}/source/mma/mma_main.cpp)
target_link_libraries(MMAApp PRIVATE MMA)

# ---- Tests ----
enable_testing()
add_subdirectory(test)
