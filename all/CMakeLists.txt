# this script adds all subprojects to a single build to allow IDEs understand the full project
# structure.

cmake_minimum_required(VERSION 3.14...3.22)

project(BuildAll LANGUAGES CXX)

include(../cmake/tools.cmake)

# needed to generate test target
enable_testing()

# Add main project which builds Aircraft and MMA libraries
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_BINARY_DIR}/src)

# Add test subproject
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../test ${CMAKE_BINARY_DIR}/test)

# Add documentation subproject
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../documentation ${CMAKE_BINARY_DIR}/documentation)
