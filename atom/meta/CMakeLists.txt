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

# Logging dependency (spdlog preferred) - must be found before creating targets
find_package(spdlog CONFIG REQUIRED)

# Create library using object library pattern for better build performance
add_library(atom-meta_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET atom-meta_object PROPERTY POSITION_INDEPENDENT_CODE 1)
# Use header-only spdlog to avoid linking conflicts with MinGW
if(TARGET spdlog::spdlog_header_only)
  target_link_libraries(atom-meta_object PUBLIC spdlog::spdlog_header_only)
else()
  target_link_libraries(atom-meta_object PUBLIC spdlog::spdlog)
  target_compile_definitions(atom-meta_object PUBLIC SPDLOG_HEADER_ONLY)
endif()

# Create static library from object library
add_library(atom-meta STATIC $<TARGET_OBJECTS:atom-meta_object>)

# Configure module with standardized settings
atom_configure_module(atom-meta)

# Use header-only spdlog to avoid linking conflicts with MinGW
if(TARGET spdlog::spdlog_header_only)
  target_link_libraries(atom-meta PUBLIC spdlog::spdlog_header_only)
else()
  target_link_libraries(atom-meta PUBLIC spdlog::spdlog)
  target_compile_definitions(atom-meta PUBLIC SPDLOG_HEADER_ONLY)
endif()
