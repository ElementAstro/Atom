# CMakeLists.txt Template for Atom Test Modules This project is licensed under
# the terms of the GPL3 license.
#
# Project Name: Atom-IO-Tests Description: Test Suite for Atom IO Module Author:
# Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-io-tests
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
if(NOT TARGET atom-io)
  message(
    FATAL_ERROR "atom-io target not found. Make sure ATOM_BUILD_IO is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_file_permission.cpp$")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/test_pushd.cpp$")

# Note: test_async_io.cpp is now included after fixes

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_io_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(
  atom_io_tests
  PRIVATE # Test infrastructure
          atom-test-common
          # Module under test and its dependencies
          atom-io
          atom-error
          atom-system
          # System libraries
          Threads::Threads)

# =============================================================================
# Platform-Specific Libraries
# =============================================================================
if(WIN32)
  target_link_libraries(atom_io_tests PRIVATE ws2_32 wsock32)
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(
  atom_io_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/atom
                        ${CMAKE_SOURCE_DIR}/tests)

# =============================================================================
# Compiler Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_io_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
  target_compile_options(atom_io_tests PRIVATE /W4)
else()
  target_compile_options(atom_io_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration with CTest
# =============================================================================
# Enable testing
enable_testing()

# Use simple test registration instead of discovery to avoid DLL issues
add_test(
  NAME atom_io_tests
  COMMAND atom_io_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(atom_io_tests PROPERTIES LABELS "io" TIMEOUT 300)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(
  test_io
  COMMAND ${CMAKE_CTEST_COMMAND} -L io --output-on-failure
  DEPENDS atom_io_tests
  COMMENT "Running io module tests")

# Add performance test target (if applicable)
add_custom_target(
  test_io_performance
  COMMAND atom_io_tests --gtest_filter="*Performance*:*Benchmark*"
  DEPENDS atom_io_tests
  COMMENT "Running io performance tests")

# =============================================================================
# IDE Organization
# =============================================================================
# Set IDE folder for better organization
set_property(TARGET atom_io_tests PROPERTY FOLDER "Tests/IO")
if(TARGET test_io)
  set_property(TARGET test_io PROPERTY FOLDER "Tests/IO")
endif()
if(TARGET test_io_performance)
  set_property(TARGET test_io_performance PROPERTY FOLDER "Tests/IO")
endif()

# =============================================================================
# Code Coverage (Optional)
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(atom_io_tests PRIVATE --coverage)
  target_link_options(atom_io_tests PRIVATE --coverage)

  add_custom_target(
    io_coverage
    COMMAND lcov --directory . --capture --output-file io_coverage.info
    COMMAND lcov --remove io_coverage.info '/usr/*' --output-file
            io_coverage.info
    COMMAND lcov --list io_coverage.info
    COMMAND genhtml -o io_coverage_html io_coverage.info
    DEPENDS test_io
    COMMENT "Generating code coverage report for io")
endif()

# =============================================================================
# Status Message
# =============================================================================
message(STATUS "IO tests configured successfully")
