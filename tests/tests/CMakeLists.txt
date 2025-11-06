# CMakeLists.txt for Atom Test Infrastructure This project is licensed under the
# terms of the GPL3 license.
#
# Project Name: Atom-Test-Infrastructure Description: Common test infrastructure
# and utilities for Atom Test Suite Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-test-infrastructure
  VERSION 1.0.0
  LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Backward-compatible aliases for legacy GTest target names
if(TARGET GTest::gtest AND NOT TARGET GTest::GTest)
  add_library(GTest::GTest ALIAS GTest::gtest)
endif()
if(TARGET GTest::gtest_main AND NOT TARGET GTest::Main)
  add_library(GTest::Main ALIAS GTest::gtest_main)
endif()

# Create interface library for test infrastructure (with guard)
if(NOT TARGET atom-test-common)
  add_library(atom-test-common INTERFACE)
endif()

# Set include directories
target_include_directories(
  atom-test-common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Link required libraries
target_link_libraries(atom-test-common INTERFACE GTest::GTest GTest::Main
                                                 Threads::Threads)

# Set C++ standard
target_compile_features(atom-test-common INTERFACE cxx_std_20)

# Compiler-specific options
if(MSVC)
  target_compile_options(atom-test-common INTERFACE /W4)
else()
  target_compile_options(atom-test-common INTERFACE -Wall -Wextra -Wpedantic)
endif()
target_compile_definitions(atom-test-common INTERFACE SPDLOG_HEADER_ONLY
                                                      SPDLOG_USE_STD_FORMAT=1)

# Test runner executable for infrastructure validation (with guard)
if(NOT TARGET test-infrastructure-validation)
  add_executable(test-infrastructure-validation
                 test_infrastructure_validation.cpp)
endif()

target_link_libraries(test-infrastructure-validation PRIVATE atom-test-common)

# Enable testing
enable_testing()

# Add infrastructure validation test
add_test(NAME InfrastructureValidation COMMAND test-infrastructure-validation)

# Custom targets for test infrastructure (with guard)
if(NOT TARGET validate_test_infrastructure)
  add_custom_target(
    validate_test_infrastructure
    COMMAND test-infrastructure-validation
    DEPENDS test-infrastructure-validation
    COMMENT "Validating test infrastructure")
endif()

# Function to create standardized test targets
function(add_atom_test test_name test_sources)
  add_executable(${test_name} ${test_sources})

  target_link_libraries(${test_name} PRIVATE atom-test-common)

  # Add the test to CTest
  add_test(NAME ${test_name} COMMAND ${test_name})

  # Set test properties
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 300 # 5 minutes timeout
                                               LABELS "atom_tests")
endfunction()

# Function to create performance test targets
function(add_atom_performance_test test_name test_sources)
  add_executable(${test_name} ${test_sources})

  target_link_libraries(${test_name} PRIVATE atom-test-common)

  # Add the test to CTest with performance label
  add_test(NAME ${test_name} COMMAND ${test_name})

  # Set test properties for performance tests
  set_tests_properties(
    ${test_name}
    PROPERTIES TIMEOUT 600 # 10 minutes timeout for performance tests
               LABELS "atom_performance_tests")
endfunction()

# Function to create stress test targets
function(add_atom_stress_test test_name test_sources)
  add_executable(${test_name} ${test_sources})

  target_link_libraries(${test_name} PRIVATE atom-test-common)

  # Add the test to CTest with stress label
  add_test(NAME ${test_name} COMMAND ${test_name})

  # Set test properties for stress tests
  set_tests_properties(
    ${test_name} PROPERTIES TIMEOUT 1800 # 30 minutes timeout for stress tests
                            LABELS "atom_stress_tests")
endfunction()

# Global test targets (with guards)
if(NOT TARGET run_all_atom_tests)
  add_custom_target(
    run_all_atom_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --label-regex "atom_tests"
    COMMENT "Running all Atom tests")
endif()

if(NOT TARGET run_performance_tests)
  add_custom_target(
    run_performance_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --label-regex
            "atom_performance_tests"
    COMMENT "Running Atom performance tests")
endif()

if(NOT TARGET run_stress_tests)
  add_custom_target(
    run_stress_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose --label-regex "atom_stress_tests"
    COMMENT "Running Atom stress tests")
endif()

# Code coverage target (if available, with guard)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
  if(NOT TARGET atom_coverage)
    add_custom_target(
      atom_coverage
      COMMAND lcov --directory . --capture --output-file atom_coverage.info
      COMMAND lcov --remove atom_coverage.info '/usr/*' --output-file
              atom_coverage.info
      COMMAND lcov --list atom_coverage.info
      COMMAND genhtml -o atom_coverage_html atom_coverage.info
      DEPENDS run_all_atom_tests
      COMMENT "Generating code coverage report for Atom")
  endif()
endif()

# Documentation for test infrastructure
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/README.md.in
               ${CMAKE_CURRENT_BINARY_DIR}/README.md @ONLY)
