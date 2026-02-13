# CMakeLists.txt for Atom Test Infrastructure This project is licensed under the
# terms of the GPL3 license.
#
# Project Name: Atom-Test-Infrastructure Description: Common test infrastructure
# and utilities for Atom Test Suite Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
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
if(TARGET GTest::gmock AND NOT TARGET GTest::GMock)
  add_library(GTest::GMock ALIAS GTest::gmock)
endif()
if(TARGET GTest::gmock_main AND NOT TARGET GTest::GMockMain)
  add_library(GTest::GMockMain ALIAS GTest::gmock_main)
endif()

# Create interface library for test infrastructure (with guard)
if(NOT TARGET atom-test-common)
  add_library(atom-test-common INTERFACE)
endif()

# Set include directories
target_include_directories(
  atom-test-common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                             ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Link required libraries (include GMock for mocking support)
target_link_libraries(
  atom-test-common INTERFACE GTest::GTest GTest::Main GTest::gmock
                             GTest::gmock_main Threads::Threads)

# Set C++ standard
target_compile_features(atom-test-common INTERFACE cxx_std_20)

# Compiler-specific options
if(MSVC)
  target_compile_options(atom-test-common INTERFACE /W4)
else()
  target_compile_options(atom-test-common INTERFACE -Wall -Wextra -Wpedantic)
endif()
# Note: Do NOT define SPDLOG_USE_STD_FORMAT - system spdlog uses fmt
target_compile_definitions(atom-test-common INTERFACE SPDLOG_COMPILED_LIB
                                                      SPDLOG_FMT_EXTERNAL)

# Test runner executable for infrastructure validation (with guard)
if(NOT TARGET test-infrastructure-validation)
  add_executable(test-infrastructure-validation
                 test_infrastructure_validation.cpp)
endif()

target_link_libraries(test-infrastructure-validation PRIVATE atom-test-common)

# ============================================================================
# Core Test Files
# ============================================================================
set(CORE_TEST_SOURCES core/test_assertions.cpp core/test_test_runner.cpp
                      core/test_test_cli.cpp core/test_test_registry.cpp)

# ============================================================================
# Assertions Test Files
# ============================================================================
set(ASSERTIONS_TEST_SOURCES
    assertions/test_matchers.cpp assertions/test_death_test.cpp
    assertions/test_string_matchers.cpp)

# ============================================================================
# Fixtures Test Files
# ============================================================================
set(FIXTURES_TEST_SOURCES
    fixtures/test_test_fixture.cpp fixtures/test_parameterized.cpp
    fixtures/test_typed_test.cpp)

# ============================================================================
# Mocking Test Files
# ============================================================================
# Note: test_mock.cpp and test_mock_matchers.cpp temporarily disabled due to API
# mismatch
set(MOCKING_TEST_SOURCES)

# ============================================================================
# Utilities Test Files
# ============================================================================
# Note: All utilities tests temporarily disabled due to API mismatch
set(UTILITIES_TEST_SOURCES)

# ============================================================================
# Reporters Test Files
# ============================================================================
# Note: test_reporters.cpp temporarily disabled due to API mismatch
set(REPORTERS_TEST_SOURCES)

# ============================================================================
# Performance Test Files
# ============================================================================
# Note: test_perf.cpp temporarily disabled due to API mismatch
set(PERFORMANCE_TEST_SOURCES performance/test_benchmark.cpp
                             performance/test_fuzz.cpp)

# ============================================================================
# Create Test Executables by Category
# ============================================================================

# Core tests
if(NOT TARGET test-atom-tests-core)
  add_executable(test-atom-tests-core ${CORE_TEST_SOURCES})
  target_link_libraries(test-atom-tests-core PRIVATE atom-test-common
                                                     atom-tests)
  add_test(NAME AtomTestsCore COMMAND test-atom-tests-core)
  set_tests_properties(AtomTestsCore PROPERTIES LABELS "atom_tests;core")
endif()

# Assertions tests
if(NOT TARGET test-atom-tests-assertions)
  add_executable(test-atom-tests-assertions ${ASSERTIONS_TEST_SOURCES})
  target_link_libraries(test-atom-tests-assertions PRIVATE atom-test-common
                                                           atom-tests)
  add_test(NAME AtomTestsAssertions COMMAND test-atom-tests-assertions)
  set_tests_properties(AtomTestsAssertions PROPERTIES LABELS
                                                      "atom_tests;assertions")
endif()

# Fixtures tests
if(NOT TARGET test-atom-tests-fixtures)
  add_executable(test-atom-tests-fixtures ${FIXTURES_TEST_SOURCES})
  target_link_libraries(test-atom-tests-fixtures PRIVATE atom-test-common
                                                         atom-tests)
  add_test(NAME AtomTestsFixtures COMMAND test-atom-tests-fixtures)
  set_tests_properties(AtomTestsFixtures PROPERTIES LABELS
                                                    "atom_tests;fixtures")
endif()

# Mocking tests (temporarily disabled due to API mismatch) if(NOT TARGET
# test-atom-tests-mocking AND MOCKING_TEST_SOURCES)
# add_executable(test-atom-tests-mocking ${MOCKING_TEST_SOURCES})
# target_link_libraries(test-atom-tests-mocking PRIVATE atom-test-common
# atom-tests) add_test(NAME AtomTestsMocking COMMAND test-atom-tests-mocking)
# set_tests_properties(AtomTestsMocking PROPERTIES LABELS "atom_tests;mocking")
# endif()

# Utilities tests (temporarily disabled due to API mismatch) if(NOT TARGET
# test-atom-tests-utilities AND UTILITIES_TEST_SOURCES)
# add_executable(test-atom-tests-utilities ${UTILITIES_TEST_SOURCES})
# target_link_libraries(test-atom-tests-utilities PRIVATE atom-test-common
# atom-tests) add_test(NAME AtomTestsUtilities COMMAND
# test-atom-tests-utilities) set_tests_properties(AtomTestsUtilities PROPERTIES
# LABELS "atom_tests;utilities") endif()

# Reporters tests (temporarily disabled due to API mismatch) if(NOT TARGET
# test-atom-tests-reporters AND REPORTERS_TEST_SOURCES)
# add_executable(test-atom-tests-reporters ${REPORTERS_TEST_SOURCES})
# target_link_libraries(test-atom-tests-reporters PRIVATE atom-test-common
# atom-tests) add_test(NAME AtomTestsReporters COMMAND
# test-atom-tests-reporters) set_tests_properties(AtomTestsReporters PROPERTIES
# LABELS "atom_tests;reporters") endif()

# Performance tests
if(NOT TARGET test-atom-tests-performance)
  add_executable(test-atom-tests-performance ${PERFORMANCE_TEST_SOURCES})
  target_link_libraries(test-atom-tests-performance PRIVATE atom-test-common
                                                            atom-tests)
  add_test(NAME AtomTestsPerformance COMMAND test-atom-tests-performance)
  set_tests_properties(AtomTestsPerformance
                       PROPERTIES LABELS "atom_tests;performance" TIMEOUT 600)
endif()

# ============================================================================
# Combined Test Executable (all tests in one)
# ============================================================================
set(ALL_TEST_SOURCES ${CORE_TEST_SOURCES} ${ASSERTIONS_TEST_SOURCES}
                     ${FIXTURES_TEST_SOURCES} ${PERFORMANCE_TEST_SOURCES})

if(NOT TARGET test-atom-tests-all)
  add_executable(test-atom-tests-all ${ALL_TEST_SOURCES})
  target_link_libraries(test-atom-tests-all PRIVATE atom-test-common atom-tests)
  add_test(NAME AtomTestsAll COMMAND test-atom-tests-all)
  set_tests_properties(AtomTestsAll PROPERTIES LABELS "atom_tests;all" TIMEOUT
                                               900)
endif()

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
