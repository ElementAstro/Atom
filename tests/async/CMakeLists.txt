# CMakeLists.txt Template for Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Async-Tests
# Description: Test Suite for Atom Async Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-async-tests VERSION 1.0.0 LANGUAGES C CXX)

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
if(NOT TARGET atom-async)
    message(FATAL_ERROR "atom-async target not found. Make sure ATOM_BUILD_ASYNC is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Remove problematic test files (preserve existing exclusions)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_async_main.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/sync/test_limiter.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/sync/test_slot.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/threading/test_threadlocal.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/threading/test_lock.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/threading/test_thread_wrapper.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/utils/test_lodash.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/utils/test_timer.cpp)

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_async_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_async_tests
    PRIVATE
    # Test infrastructure
    atom-test-common

    # Module under test and its dependencies
    atom-async
    atom-error

    # Dependencies
    fmt

    # System libraries
    Threads::Threads
)

# =============================================================================
# Platform-Specific Libraries
# =============================================================================
if(WIN32)
    target_link_libraries(atom_async_tests PRIVATE ws2_32 wsock32)
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(atom_async_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# =============================================================================
# Compiler Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_async_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_async_tests PRIVATE /W4)
else()
    target_compile_options(atom_async_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration with CTest
# =============================================================================
# Enable testing
enable_testing()

# Use GoogleTest discovery for automatic test registration
include(GoogleTest)
gtest_discover_tests(atom_async_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    LABELS "async"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_async
    COMMAND ${CMAKE_CTEST_COMMAND} -L async --output-on-failure
    DEPENDS atom_async_tests
    COMMENT "Running async module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_async_performance
    COMMAND atom_async_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_async_tests
    COMMENT "Running async performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
# Set IDE folder for better organization
set_property(TARGET atom_async_tests PROPERTY FOLDER "Tests/Async")
if(TARGET test_async)
    set_property(TARGET test_async PROPERTY FOLDER "Tests/Async")
endif()
if(TARGET test_async_performance)
    set_property(TARGET test_async_performance PROPERTY FOLDER "Tests/Async")
endif()

# =============================================================================
# Code Coverage (Optional)
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(atom_async_tests PRIVATE --coverage)
    target_link_options(atom_async_tests PRIVATE --coverage)

    add_custom_target(async_coverage
        COMMAND lcov --directory . --capture --output-file async_coverage.info
        COMMAND lcov --remove async_coverage.info '/usr/*' --output-file async_coverage.info
        COMMAND lcov --list async_coverage.info
        COMMAND genhtml -o async_coverage_html async_coverage.info
        DEPENDS test_async
        COMMENT "Generating code coverage report for async"
    )
endif()

# =============================================================================
# Status Message
# =============================================================================
message(STATUS "Async tests configured successfully")
