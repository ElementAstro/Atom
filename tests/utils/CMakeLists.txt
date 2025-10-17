# =============================================================================
# Standardized Test Configuration for Atom Utils Module
# =============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Utils-Tests
# Description: Header-Only Test Suite for Atom Utils Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-utils-tests VERSION 1.0.0 LANGUAGES C CXX)

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
    "${PROJECT_SOURCE_DIR}/crypto/test_aes_main.cpp"
    "${PROJECT_SOURCE_DIR}/core/test_uuid.cpp"
)

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
    MODULE_DEPENDENCIES "${MODULE_DEPENDENCIES}"
    EXTRA_LIBRARIES "${EXTRA_LIBRARIES}"
    TEST_TARGET_NAME "${TEST_TARGET_NAME}"
    EXCLUDE_SOURCES "${EXCLUDE_SOURCES}"
)
