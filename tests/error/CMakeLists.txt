# CMakeLists.txt for Atom Error Tests (Custom Configuration) This project is
# licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Error-Tests Description: Test Suite for Atom Error Module
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-error-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# =============================================================================
# Include Test Infrastructure (Optional for Custom Modules)
# =============================================================================
# Include the standardized test infrastructure if available
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests"
                   "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-error)
  message(
    FATAL_ERROR
      "atom-error target not found. Make sure ATOM_BUILD_ERROR is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# Test sources Note: test_error_formatting.cpp, test_error_integration.cpp, and
# test_error_system.cpp disabled due to missing formatter/circuit breaker
# classes
set(TEST_SOURCES test_error.cpp test_stacktrace.cpp)

# =============================================================================
# Create Test Executables (Custom Multi-Executable Configuration)
# =============================================================================
# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

  add_executable(${TEST_NAME} ${TEST_SOURCE})

  target_link_libraries(
    ${TEST_NAME}
    PRIVATE # Test infrastructure (if available)
            $<TARGET_NAME_IF_EXISTS:atom-test-common>
            # Module under test
            atom-error
            # GoogleTest
            GTest::gtest
            GTest::gtest_main
            # System libraries
            Threads::Threads)

  target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}
                                                  ${CMAKE_SOURCE_DIR}/atom)

  # Add test to CTest
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

  # Set test properties
  set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 300 LABELS "error;unit")

  # Copy DLLs on Windows
  if(WIN32)
    if(COMMAND copy_test_dlls)
      copy_test_dlls(${TEST_NAME})
    endif()

    add_custom_command(
      TARGET ${TEST_NAME}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${TEST_NAME}> $<TARGET_FILE_DIR:${TEST_NAME}>
      COMMAND_EXPAND_LISTS)
  endif()
endforeach()

# Integration tests disabled - uncomment when formatter classes are implemented
# set_tests_properties(test_error_integration PROPERTIES TIMEOUT 600 LABELS
# "error;integration") set_tests_properties(test_error_integration PROPERTIES
# LABELS "error;integration;performance")
