# =============================================================================
# Standardized Test Configuration for Atom Async Module
# =============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Async-Tests Description: Test Suite for Atom Async Module
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-async-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# =============================================================================
# Module Configuration
# =============================================================================
set(MODULE_NAME "async")
set(MODULE_DEPENDENCIES "")
set(EXTRA_LIBRARIES "fmt")
set(TEST_TARGET_NAME "")
set(HEADER_ONLY OFF)
set(USE_GMOCK OFF)

# Exclude problematic test files
set(EXCLUDE_SOURCES
    "${PROJECT_SOURCE_DIR}/test_async_main.cpp"
    "${PROJECT_SOURCE_DIR}/sync/test_limiter.cpp"
    "${PROJECT_SOURCE_DIR}/threading/test_threadlocal.cpp"
    "${PROJECT_SOURCE_DIR}/threading/test_lock.cpp"
    "${PROJECT_SOURCE_DIR}/threading/test_thread_wrapper.cpp"
    "${PROJECT_SOURCE_DIR}/utils/test_lodash.cpp"
    "${PROJECT_SOURCE_DIR}/utils/test_timer.cpp")

# =============================================================================
# Include Standard Test Template
# =============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/StandardTestTemplate.cmake)

# =============================================================================
# Find Test Sources and Configure
# =============================================================================
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

configure_standard_module_tests(
  ${MODULE_NAME}
  "${TEST_SOURCES}"
  USE_GMOCK
  ${USE_GMOCK}
  MODULE_DEPENDENCIES
  "${MODULE_DEPENDENCIES}"
  EXTRA_LIBRARIES
  "${EXTRA_LIBRARIES}"
  TEST_TARGET_NAME
  "${TEST_TARGET_NAME}"
  EXCLUDE_SOURCES
  "${EXCLUDE_SOURCES}")

# Copy DLLs to test directory
if(TARGET atom_async_tests)
  if(COMMAND copy_test_dlls)
    copy_test_dlls(atom_async_tests)
  endif()
  # Increase timeout for async tests (thread pool tests take time)
  if(TEST atom_async_tests)
    set_tests_properties(atom_async_tests PROPERTIES TIMEOUT 600)
  endif()
endif()
