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
option(ATOM_USE_BOOST "Enable Boost high-performance data structures" OFF)
option(ATOM_USE_BOOST_LOCKFREE "Enable Boost lockfree data structures" OFF)
option(ATOM_USE_BOOST_CONTAINER "Enable Boost container library" OFF)
option(ATOM_USE_BOOST_GRAPH "Enable Boost graph library" OFF)
option(ATOM_USE_BOOST_INTRUSIVE "Enable Boost intrusive containers" OFF)

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
find_package(Python COMPONENTS Interpreter REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
include_directories(${pybind11_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})

# Boost support
if(ATOM_USE_BOOST)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(BOOST_COMPONENTS)
    
    if(ATOM_USE_BOOST_CONTAINER)
        list(APPEND BOOST_COMPONENTS container)
    endif()
    
    if(ATOM_USE_BOOST_LOCKFREE)
        list(APPEND BOOST_COMPONENTS atomic thread)
    endif()
    
    if(ATOM_USE_BOOST_GRAPH)
        list(APPEND BOOST_COMPONENTS graph)
    endif()
    
    if(ATOM_USE_BOOST_INTRUSIVE)
        # intrusive is header-only
    endif()
    
    find_package(Boost 1.74 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Found Boost: ${Boost_VERSION} (${Boost_INCLUDE_DIRS})")
endif()

# Add subdirectories
add_subdirectory(extra)
add_subdirectory(atom)
add_subdirectory(python)
if (ATOM_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
if (ATOM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)