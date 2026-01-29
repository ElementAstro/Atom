# =============================================================================
# Standardized Test Configuration for Atom Utils Module
# =============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Utils-Tests Description: Header-Only Test Suite for Atom
# Utils Module Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-utils-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# =============================================================================
# Module Configuration
# =============================================================================
set(MODULE_NAME "utils")
set(MODULE_DEPENDENCIES "")
set(EXTRA_LIBRARIES "")
set(TEST_TARGET_NAME "")
set(HEADER_ONLY ON)
set(USE_GMOCK OFF)

# Exclude problematic test files
set(EXCLUDE_SOURCES
    "crypto/test_aes_main\\.cpp"
    "crypto/test_aes\\.hpp" # Requires OpenSSL
    "core/test_uuid\\.cpp"
    "string/test_cstring\\.hpp"
    "string/test_to_string\\.hpp"
    "core/test_anyutils\\.hpp"
    "format/test_xml\\.hpp"
    "math/test_random\\.hpp"
    "debug/test_print\\.hpp"
    "format/test_difflib\\.hpp"
    "core/test_bit\\.hpp"
    "core/test_linq\\.hpp"
    "math/test_uuid\\.hpp")

# =============================================================================
# Include Standard Test Template
# =============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/StandardTestTemplate.cmake)

# =============================================================================
# Find Test Sources and Configure
# =============================================================================
# For header-only modules, the template will handle test source generation
configure_standard_module_tests(
  ${MODULE_NAME}
  ""
  HEADER_ONLY
  MODULE_DEPENDENCIES
  "${MODULE_DEPENDENCIES}"
  EXTRA_LIBRARIES
  "${EXTRA_LIBRARIES}"
  TEST_TARGET_NAME
  "${TEST_TARGET_NAME}"
  EXCLUDE_SOURCES
  "${EXCLUDE_SOURCES}")

# Copy DLLs to test directory
if(TARGET atom_utils_tests)
  if(COMMAND copy_test_dlls)
    copy_test_dlls(atom_utils_tests)
  endif()
  # Increase timeout for computationally intensive tests
  if(TEST atom_utils_tests)
    set_tests_properties(atom_utils_tests PROPERTIES TIMEOUT 600)
  endif()

  # Fix for MinGW: "too many sections" error when compiling large test files
  if(MINGW)
    target_compile_options(atom_utils_tests PRIVATE -Wa,-mbig-obj)
  endif()
endif()
