# CMakeLists.txt for Atom Components Tests (Custom Configuration)
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Components-Tests
# Description: Test Suite for Atom Components Module (Custom Build)
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-components-tests VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest QUIET)
find_package(Threads REQUIRED)

if(NOT GTEST_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)
else()
  include(GoogleTest)
endif()

# =============================================================================
# Include Test Infrastructure (Optional for Custom Modules)
# =============================================================================
# Include the standardized test infrastructure if available
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Custom Dependency Management (Preserved)
# =============================================================================
# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../)

# Bring in required Atom modules so targets resolve locally
# Only add subdirectories if the targets do not already exist (to avoid duplicates when included from the top-level)
# Build error first (no external loguru dependency)
if(NOT TARGET atom-error)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/error ${CMAKE_CURRENT_BINARY_DIR}/atom-error)
endif()
# Build log module which provides loguru target wrapper
# Ensure ZLIB is available for atom-log and provide a stub for atom-web to satisfy link interface
find_package(ZLIB REQUIRED)
if(NOT TARGET atom-web)
  add_library(atom-web INTERFACE)
endif()
if(NOT TARGET loguru)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/log ${CMAKE_CURRENT_BINARY_DIR}/atom-log)
endif()
# Build utils next (relies on ModuleDependencies.cmake from top-level; if missing, fall back to minimal)
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)
  if(NOT TARGET atom-utils)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/utils ${CMAKE_CURRENT_BINARY_DIR}/atom-utils)
  endif()
else()
  message(WARNING "ModuleDependencies.cmake not found; building a minimal atom-utils stub.")
  if(NOT TARGET atom-utils)
    add_library(atom-utils INTERFACE)
  endif()
endif()
# Finally, build components
if(NOT TARGET atom-component)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/components ${CMAKE_CURRENT_BINARY_DIR}/atom-components)
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# Create comprehensive component tests
set(TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/simple_test.cpp
    ${PROJECT_SOURCE_DIR}/comprehensive_component_test.cpp
)

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_components_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
# Link with atom-component library and its dependencies
target_link_libraries(atom_components_tests
    PRIVATE
    # Test infrastructure (if available)
    $<TARGET_NAME_IF_EXISTS:atom-test-common>

    # Module under test and its dependencies
    atom-component
    atom-error
    atom-utils

    # GoogleTest
    gtest
    gtest_main

    # System libraries
    Threads::Threads
)

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++20 standard
target_compile_features(atom_components_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_components_tests PRIVATE /W4)
else()
    target_compile_options(atom_components_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Pass macro definitions for optional features (Custom Configuration)
if(ATOM_ENABLE_LUA)
    target_compile_definitions(atom_components_tests PRIVATE ATOM_ENABLE_LUA=1)
endif()

if(ATOM_ENABLE_PYTHON)
    target_compile_definitions(atom_components_tests PRIVATE ATOM_ENABLE_PYTHON=1)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
gtest_discover_tests(atom_components_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    LABELS "components"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_components
    COMMAND ${CMAKE_CTEST_COMMAND} -L components --output-on-failure
    DEPENDS atom_components_tests
    COMMENT "Running components module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_components_performance
    COMMAND atom_components_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_components_tests
    COMMENT "Running components performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_components_tests PROPERTY FOLDER "Tests/Components")
set_property(TARGET test_components PROPERTY FOLDER "Tests/Components")
set_property(TARGET test_components_performance PROPERTY FOLDER "Tests/Components")
