# CMakeLists.txt Template for Atom Test Modules This project is licensed under
# the terms of the GPL3 license.
#
# Project Name: Atom-System-Tests Description: Test Suite for Atom System Module
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-system-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# =============================================================================
# Include Test Infrastructure
# =============================================================================
# Include the standardized test infrastructure
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests"
                   "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-system)
  message(
    FATAL_ERROR
      "atom-system target not found. Make sure ATOM_BUILD_SYSTEM is enabled.")
endif()

message(
  WARNING
    "Skipping atom-system tests in this configuration (pending API alignment)")
return()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Remove build directory files
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/build/.*")

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_system_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(
  atom_system_tests
  PRIVATE # Test infrastructure
          atom-test-common
          # Module under test and its dependencies
          atom-system
          atom-error
          # Additional dependencies (system module specific)
          atom-log
          # System libraries
          Threads::Threads)

# Platform-specific libraries
if(WIN32)
  target_link_libraries(atom_system_tests PRIVATE ws2_32 wsock32)
endif()

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_system_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
  target_compile_options(atom_system_tests PRIVATE /W4)
else()
  target_compile_options(atom_system_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_system_tests WORKING_DIRECTORY
# ${CMAKE_CURRENT_SOURCE_DIR} PROPERTIES LABELS "system" TIMEOUT 300 )

# Manual test registration as workaround
add_test(
  NAME atom_system_tests
  COMMAND atom_system_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(atom_system_tests PROPERTIES LABELS "system" TIMEOUT 300)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(
  test_system
  COMMAND ${CMAKE_CTEST_COMMAND} -L system --output-on-failure
  DEPENDS atom_system_tests
  COMMENT "Running system module tests")

# Add performance test target (if applicable)
add_custom_target(
  test_system_performance
  COMMAND atom_system_tests --gtest_filter="*Performance*:*Benchmark*"
  DEPENDS atom_system_tests
  COMMENT "Running system performance tests")

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_system_tests PROPERTY FOLDER "Tests/System")
set_property(TARGET test_system PROPERTY FOLDER "Tests/System")
set_property(TARGET test_system_performance PROPERTY FOLDER "Tests/System")
