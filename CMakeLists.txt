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
  NAME fmt
  GIT_TAG 10.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS "FMT_INSTALL YES" # create an installable target
)

# ---- Create Aircraft Library ----

add_library(
  Aircraft ${PROJECT_SOURCE_DIR}/include/aircraft/aircraft.h
           ${PROJECT_SOURCE_DIR}/source/aircraft.cpp
)
set_target_properties(Aircraft PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_compile_options(Aircraft PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_link_libraries(Aircraft PRIVATE fmt::fmt)
target_include_directories(
  Aircraft PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                  $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Create MMA Library ----

add_library(MMA ${PROJECT_SOURCE_DIR}/include/mma/mma.h ${PROJECT_SOURCE_DIR}/source/mma.cpp)
set_target_properties(MMA PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_compile_options(MMA PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")
target_link_libraries(MMA PRIVATE fmt::fmt)
target_include_directories(
  MMA PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
             $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Create Aircraft Executable ----

add_executable(AircraftApp ${PROJECT_SOURCE_DIR}/source/aircraft_main.cpp)
set_target_properties(AircraftApp PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_link_libraries(AircraftApp PRIVATE Aircraft)

# ---- Create MMA Executable ----

add_executable(MMAApp ${PROJECT_SOURCE_DIR}/source/mma_main.cpp)
set_target_properties(MMAApp PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
target_link_libraries(MMAApp PRIVATE MMA)
