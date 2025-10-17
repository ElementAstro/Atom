# CMakeLists.txt Template for Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Memory-Tests
# Description: Test Suite for Atom Memory Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-memory-tests VERSION 1.0.0 LANGUAGES C CXX)

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
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-memory)
    message(FATAL_ERROR "atom-memory target not found. Make sure ATOM_BUILD_MEMORY is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Exclude CMake-generated helper directories
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/build/.*")

# Remove problematic test files (preserve existing exclusions)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_memory_main.cpp)

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_memory_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_memory_tests
    PRIVATE
    # Test infrastructure
    atom-test-common

    # Module under test and its dependencies
    atom-memory
    atom-error

    # GoogleTest libraries (explicit linking required)
    GTest::gtest
    GTest::gtest_main

    # System libraries
    Threads::Threads
)

# =============================================================================
# Platform-Specific Libraries
# =============================================================================
if(WIN32)
    target_link_libraries(atom_memory_tests PRIVATE psapi pdh iphlpapi kernel32 user32 ws2_32 wsock32)
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(atom_memory_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# =============================================================================
# Compiler Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_memory_tests PRIVATE cxx_std_20)

target_compile_definitions(atom_memory_tests
    PRIVATE
        SPDLOG_USE_STD_FORMAT=1
)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_memory_tests PRIVATE /W4)
else()
    target_compile_options(atom_memory_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration with CTest
# =============================================================================
# Enable testing
enable_testing()

# Use GoogleTest discovery for automatic test registration
include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_memory_tests
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "memory"
#     TIMEOUT 300
# )

# Manual test registration as workaround
add_test(NAME atom_memory_tests
    COMMAND atom_memory_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(atom_memory_tests PROPERTIES
    LABELS "memory"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_memory
    COMMAND ${CMAKE_CTEST_COMMAND} -L memory --output-on-failure
    DEPENDS atom_memory_tests
    COMMENT "Running memory module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_memory_performance
    COMMAND atom_memory_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_memory_tests
    COMMENT "Running memory performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
# Set IDE folder for better organization
set_property(TARGET atom_memory_tests PROPERTY FOLDER "Tests/Memory")
if(TARGET test_memory)
    set_property(TARGET test_memory PROPERTY FOLDER "Tests/Memory")
endif()
if(TARGET test_memory_performance)
    set_property(TARGET test_memory_performance PROPERTY FOLDER "Tests/Memory")
endif()

# =============================================================================
# Code Coverage (Optional)
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(atom_memory_tests PRIVATE --coverage)
    target_link_options(atom_memory_tests PRIVATE --coverage)

    add_custom_target(memory_coverage
        COMMAND lcov --directory . --capture --output-file memory_coverage.info
        COMMAND lcov --remove memory_coverage.info '/usr/*' --output-file memory_coverage.info
        COMMAND lcov --list memory_coverage.info
        COMMAND genhtml -o memory_coverage_html memory_coverage.info
        DEPENDS test_memory
        COMMENT "Generating code coverage report for memory"
    )
endif()

# =============================================================================
# Status Message
# =============================================================================
message(STATUS "Memory tests configured successfully")
