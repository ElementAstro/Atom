# CMakeLists.txt for atom-meta This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: atom-meta Description: a library for meta programming in C++
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-meta
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES global_ptr.cpp)

# Headers
set(HEADERS global_ptr.hpp)

# Create library using object library pattern for better build performance
add_library(atom-meta_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET atom-meta_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Create static library from object library
add_library(atom-meta STATIC $<TARGET_OBJECTS:atom-meta_object>)

# Configure module with standardized settings
atom_configure_module(atom-meta)

# Logging dependency (spdlog preferred)
atom_setup_logging_deps(atom-meta)
