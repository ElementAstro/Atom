# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom
# Description: Atom Library for all of the Element Astro Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom VERSION 1.0.0 LANGUAGES C CXX)

# Python Support
option(ATOM_BUILD_PYTHON "Build Atom with Python support" OFF)
if(ATOM_BUILD_PYTHON)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)
    if(PYTHON_FOUND)
        message(STATUS "Found Python ${PYTHON_VERSION_STRING}: ${PYTHON_EXECUTABLE}")
        find_package(pybind11 QUIET)
        if(pybind11_FOUND)
            message(STATUS "Found pybind11: ${pybind11_INCLUDE_DIRS}")
        else()
            message(FATAL_ERROR "pybind11 not found")
        endif()
    else()
        message(FATAL_ERROR "Python not found")
    endif()
endif()

# Subdirectories
set(SUBDIRECTORIES
    algorithm
    async
    components
    connection
    error
    function
    io
    log
    search
    secret
    sysinfo
    system
    tests
    utils
    web
)

foreach(subdir ${SUBDIRECTORIES})
    add_subdirectory(${subdir})
endforeach()

# Sources and Headers
set(ATOM_SOURCES
    log/atomlog.cpp
    log/logger.cpp
)

set(ATOM_HEADERS
    log/atomlog.hpp
    log/logger.hpp
)

# Libraries
set(ATOM_LIBS
    loguru
    atom-function
    atom-algorithm
    atom-async
    atom-io
    atom-component
    atom-utils
    atom-search
    atom-web
    atom-system
    atom-sysinfo
)

# Object Library
add_library(atom_object OBJECT ${ATOM_SOURCES} ${ATOM_HEADERS})

if(WIN32)
    target_link_libraries(atom_object setupapi wsock32 ws2_32 shlwapi iphlpapi)
endif()

target_link_libraries(atom_object ${ATOM_LIBS})

# Static Library
add_library(atom STATIC)
set_target_properties(atom PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}atom${CMAKE_STATIC_LIBRARY_SUFFIX}"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries(atom atom_object ${CMAKE_THREAD_LIBS_INIT} ${ATOM_LIBS})

# Install
install(TARGETS atom
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT library
)