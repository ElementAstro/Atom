# =============================================================================
# Standardized Test CMakeLists.txt Template for Atom Modules
# =============================================================================
# This template provides consistent test configuration for Atom modules. Replace
# the variables below with your module-specific values.
#
# Required variables: MODULE_NAME - Name of the module (e.g., "algorithm",
# "async") MODULE_DEPENDENCIES - List of additional module dependencies
# EXTRA_LIBRARIES - Additional libraries beyond standard dependencies
#
# Usage: 1. Set the variables below 2. Include this template at the end of your
# CMakeLists.txt 3. Customize any module-specific options as needed
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Module Configuration (EDIT THESE VARIABLES)
# =============================================================================

# Module name (must match ATOM_BUILD_<MODULE_NAME> option)
set(MODULE_NAME "YOUR_MODULE_NAME_HERE")

# Module dependencies (besides atom-error)
set(MODULE_DEPENDENCIES "")

# Additional libraries (like fmt, spdlog, OpenSSL, etc.)
set(EXTRA_LIBRARIES "")

# Test target name (defaults to atom_<module>_tests)
set(TEST_TARGET_NAME "")

# Set to ON if this is a header-only module
set(HEADER_ONLY OFF)

# Set to ON to enable GoogleMock support
set(USE_GMOCK OFF)

# Exclude specific test files (list of relative paths)
set(EXCLUDE_SOURCES "")

# Extra include directories
set(EXTRA_INCLUDES "")

# =============================================================================
# Include Standard Test Template
# =============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/StandardTestTemplate.cmake)

# =============================================================================
# Find Test Sources
# =============================================================================
if(HEADER_ONLY)
  # For header-only modules, the template will handle test source generation
  configure_standard_module_tests(
    ${MODULE_NAME}
    ""
    HEADER_ONLY
    MODULE_DEPENDENCIES
    "${MODULE_DEPENDENCIES}"
    EXTRA_LIBRARIES
    "${EXTRA_LIBRARIES}"
    EXTRA_INCLUDES
    "${EXTRA_INCLUDES}"
    TEST_TARGET_NAME
    "${TEST_TARGET_NAME}"
    EXCLUDE_SOURCES
    "${EXCLUDE_SOURCES}")
else()
  # For modules with .cpp test files
  file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

  # Remove excluded sources
  if(EXCLUDE_SOURCES)
    list(REMOVE_ITEM TEST_SOURCES ${EXCLUDE_SOURCES})
  endif()

  configure_standard_module_tests(
    ${MODULE_NAME}
    "${TEST_SOURCES}"
    USE_GMOCK
    ${USE_GMOCK}
    MODULE_DEPENDENCIES
    "${MODULE_DEPENDENCIES}"
    EXTRA_LIBRARIES
    "${EXTRA_LIBRARIES}"
    EXTRA_INCLUDES
    "${EXTRA_INCLUDES}"
    TEST_TARGET_NAME
    "${TEST_TARGET_NAME}")
endif()
