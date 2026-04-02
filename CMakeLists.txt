cmake_minimum_required(VERSION 3.22)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  AvionicsWarrantyMaintenanceSystem
  VERSION 1.0
  LANGUAGES CXX
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.13.0")

CPMAddPackage(
  NAME spdlog
  GIT_TAG v1.17.0
  GITHUB_REPOSITORY gabime/spdlog
  OPTIONS "SPDLOG_BUILD_SHARED OFF" "SPDLOG_BUILD_EXAMPLE OFF" "SPDLOG_BUILD_TESTS OFF"
)

# ---- Create Aircraft Library ----

file(GLOB AIRCRAFT_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/aircraft/*.h")
file(GLOB AIRCRAFT_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/source/aircraft/*.cpp")
list(FILTER AIRCRAFT_SOURCES EXCLUDE REGEX ".*/aircraft_main\\.cpp$")

add_library(Aircraft ${AIRCRAFT_HEADERS} ${AIRCRAFT_SOURCES})

set_target_properties(Aircraft PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_compile_options(Aircraft PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_link_libraries(Aircraft PRIVATE spdlog::spdlog)
target_include_directories(
  Aircraft PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                  $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Create MMA Library ----

file(GLOB MMA_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/include/mma/*.h")
file(GLOB MMA_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/source/mma/*.cpp")
list(FILTER MMA_SOURCES EXCLUDE REGEX ".*/mma_main\\.cpp$")

add_library(MMA ${MMA_HEADERS} ${MMA_SOURCES})
set_target_properties(MMA PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_compile_options(MMA PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_link_libraries(MMA PRIVATE spdlog::spdlog)
target_include_directories(
  MMA PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
             $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Create Aircraft Executable ----

add_executable(AircraftApp ${PROJECT_SOURCE_DIR}/source/aircraft/aircraft_main.cpp)
set_target_properties(AircraftApp PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_link_libraries(AircraftApp PRIVATE Aircraft)

# ---- Create MMA Executable ----

add_executable(MMAApp ${PROJECT_SOURCE_DIR}/source/mma/mma_main.cpp)
set_target_properties(MMAApp PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_link_libraries(MMAApp PRIVATE MMA)

# Include test subdirectory
enable_testing()
add_subdirectory(test)
