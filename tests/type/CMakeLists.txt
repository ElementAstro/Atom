# CMakeLists.txt Template for Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Type-Tests
# Description: Test Suite for Atom Type Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-type-tests VERSION 1.0.0 LANGUAGES C CXX)

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
if(NOT TARGET atom-type)
    message(FATAL_ERROR "atom-type target not found. Make sure ATOM_BUILD_TYPE is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Exclude files with their own main() function
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_args_main\\.cpp$")
# Exclude test_auto_table.cpp due to GMock linking issues with MinGW
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_auto_table\\.cpp$")

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_type_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_type_tests
    PRIVATE
    # Test infrastructure
    atom-test-common
    GTest::gmock

    # Module under test and its dependencies
    atom-type
    atom-error

    # System libraries
    Threads::Threads
)

# =============================================================================
# Platform-Specific Libraries
# =============================================================================
if(WIN32)
    target_link_libraries(atom_type_tests PRIVATE ws2_32 wsock32)
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(atom_type_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# =============================================================================
# Compiler Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_type_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_type_tests PRIVATE /W4)
else()
    target_compile_options(atom_type_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration with CTest
# =============================================================================
# Enable testing
enable_testing()

# Use GoogleTest discovery for automatic test registration
include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_type_tests
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "type"
#     TIMEOUT 300
# )

# Manual test registration as workaround
add_test(NAME atom_type_tests
    COMMAND atom_type_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(atom_type_tests PROPERTIES
    LABELS "type"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_type
    COMMAND ${CMAKE_CTEST_COMMAND} -L type --output-on-failure
    DEPENDS atom_type_tests
    COMMENT "Running type module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_type_performance
    COMMAND atom_type_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_type_tests
    COMMENT "Running type performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
# Set IDE folder for better organization
set_property(TARGET atom_type_tests PROPERTY FOLDER "Tests/Type")
if(TARGET test_type)
    set_property(TARGET test_type PROPERTY FOLDER "Tests/Type")
endif()
if(TARGET test_type_performance)
    set_property(TARGET test_type_performance PROPERTY FOLDER "Tests/Type")
endif()

# =============================================================================
# Code Coverage (Optional)
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(atom_type_tests PRIVATE --coverage)
    target_link_options(atom_type_tests PRIVATE --coverage)

    add_custom_target(type_coverage
        COMMAND lcov --directory . --capture --output-file type_coverage.info
        COMMAND lcov --remove type_coverage.info '/usr/*' --output-file type_coverage.info
        COMMAND lcov --list type_coverage.info
        COMMAND genhtml -o type_coverage_html type_coverage.info
        DEPENDS test_type
        COMMENT "Generating code coverage report for type"
    )
endif()

# =============================================================================
# Status Message
# =============================================================================
message(STATUS "Type tests configured successfully")
