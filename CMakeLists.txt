# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.

# Description: The foundational library for all elemental astro projects
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(Atom LANGUAGES C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")
include(cmake/compiler_options.cmake)

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(BUILD_SHARED_LIBS ON)

include_directories(.)

# Add subdirectories
add_subdirectory(example)
add_subdirectory(atom)
add_subdirectory(tests)

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
