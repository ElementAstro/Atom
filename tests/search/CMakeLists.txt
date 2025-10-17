# CMakeLists.txt Template for Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Search-Tests
# Description: Test Suite for Atom Search Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-search-tests VERSION 1.0.0 LANGUAGES C CXX)

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
if(NOT TARGET atom-search)
    message(FATAL_ERROR "atom-search target not found. Make sure ATOM_BUILD_SEARCH is enabled.")
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
add_executable(atom_search_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_search_tests
    PRIVATE
    # Test infrastructure
    atom-test-common

    # Module under test and its dependencies
    atom-search
    atom-error

    # Additional dependencies (search module specific)
    atom-log

    # System libraries
    Threads::Threads
)

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_search_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_search_tests PRIVATE /W4)
else()
    target_compile_options(atom_search_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_search_tests
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "search"
#     TIMEOUT 300
# )

# Manual test registration as workaround
add_test(NAME atom_search_tests
    COMMAND atom_search_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(atom_search_tests PROPERTIES
    LABELS "search"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_search
    COMMAND ${CMAKE_CTEST_COMMAND} -L search --output-on-failure
    DEPENDS atom_search_tests
    COMMENT "Running search module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_search_performance
    COMMAND atom_search_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_search_tests
    COMMENT "Running search performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_search_tests PROPERTY FOLDER "Tests/Search")
set_property(TARGET test_search PROPERTY FOLDER "Tests/Search")
set_property(TARGET test_search_performance PROPERTY FOLDER "Tests/Search")
