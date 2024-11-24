# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.

# Description: The foundational library for all elemental astro projects
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(Atom LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add custom target for additional CMake files
add_custom_target(AtomCmakeAdditionalFiles
    SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_options.cmake
)

option(ATOM_BUILD_EXAMPLES "Build the examples" OFF)
option(ATOM_BUILD_TESTS "Build the tests" OFF)

# Append module paths
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

# Include compiler options
include(cmake/compiler_options.cmake)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extra)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(.)

# Find packages
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Readline REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Python COMPONENTS Interpreter REQUIRED)
include_directories(${pybind11_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})

# Add subdirectories
add_subdirectory(extra)
add_subdirectory(atom)
if (ATOM_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
if (ATOM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)