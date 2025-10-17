# CMakeLists.txt Template for Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Serial-Tests
# Description: Test Suite for Atom Serial Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-serial-tests VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUSB QUIET libusb-1.0)
endif()

if(NOT LIBUSB_FOUND)
    message(WARNING "libusb-1.0 not found; skipping serial tests")
    return()
endif()

# =============================================================================
# Include Test Infrastructure
# =============================================================================
# Include the standardized test infrastructure
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-serial)
    message(FATAL_ERROR "atom-serial target not found. Make sure ATOM_BUILD_SERIAL is enabled.")
endif()

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
add_executable(atom_serial_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_serial_tests
    PRIVATE
    # Test infrastructure
    atom-test-common

    # Module under test and its dependencies
    atom-serial
    atom-error

    # Additional dependencies (serial module specific)
    atom-log

    # System libraries
    Threads::Threads
)

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_serial_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_serial_tests PRIVATE /W4)
else()
    target_compile_options(atom_serial_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_serial_tests
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "serial"
#     TIMEOUT 300
# )

# Manual test registration as workaround
add_test(NAME atom_serial_tests
    COMMAND atom_serial_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(atom_serial_tests PROPERTIES
    LABELS "serial"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_serial
    COMMAND ${CMAKE_CTEST_COMMAND} -L serial --output-on-failure
    DEPENDS atom_serial_tests
    COMMENT "Running serial module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_serial_performance
    COMMAND atom_serial_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_serial_tests
    COMMENT "Running serial performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_serial_tests PROPERTY FOLDER "Tests/Serial")
set_property(TARGET test_serial PROPERTY FOLDER "Tests/Serial")
set_property(TARGET test_serial_performance PROPERTY FOLDER "Tests/Serial")
