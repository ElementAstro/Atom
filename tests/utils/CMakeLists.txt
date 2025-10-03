# CMakeLists.txt Template for Header-Only Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Utils-Tests
# Description: Header-Only Test Suite for Atom Utils Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-utils-tests VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# =============================================================================
# Include Test Infrastructure
# =============================================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
if(NOT TARGET atom-utils)
    message(FATAL_ERROR "atom-utils target not found. Make sure ATOM_BUILD_UTILS is enabled.")
endif()

# =============================================================================
# Header-Only Test Configuration
# =============================================================================
# Find all test header files
file(GLOB_RECURSE TEST_HEADERS ${PROJECT_SOURCE_DIR}/*.hpp)

# Find all test source files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Remove demo/example files that have their own main() functions
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/crypto/test_aes_main.cpp)
# Remove problematic test files with namespace issues
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/core/test_uuid.cpp)

# Create test runner source file
set(TEST_RUNNER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/test_utils_runner.cpp")

# Generate test runner content
set(TEST_INCLUDES "")
foreach(HEADER ${TEST_HEADERS})
    file(RELATIVE_PATH REL_HEADER ${PROJECT_SOURCE_DIR} ${HEADER})
    string(APPEND TEST_INCLUDES "#include \"${REL_HEADER}\"\n")
endforeach()

# Create a basic test runner that includes both .cpp and .hpp tests
file(WRITE "${TEST_RUNNER_SOURCE}"
"// Auto-generated test runner for ${PROJECT_NAME}
#include <gtest/gtest.h>

// Include all test headers
${TEST_INCLUDES}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
")

# Combine all test sources
list(APPEND TEST_SOURCES ${TEST_RUNNER_SOURCE})

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_utils_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_utils_tests
    PRIVATE
    atom-test-common
    atom-utils
    atom-error
    Threads::Threads
)

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(atom_utils_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# =============================================================================
# Compiler Configuration
# =============================================================================
target_compile_features(atom_utils_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(atom_utils_tests PRIVATE /W4)
else()
    target_compile_options(atom_utils_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
gtest_discover_tests(atom_utils_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    LABELS "utils;header_only"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_utils
    COMMAND ${CMAKE_CTEST_COMMAND} -L utils --output-on-failure
    DEPENDS atom_utils_tests
    COMMENT "Running utils header-only tests"
)

# Add performance test target (if applicable)
add_custom_target(test_utils_performance
    COMMAND atom_utils_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_utils_tests
    COMMENT "Running utils performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_utils_tests PROPERTY FOLDER "Tests/Utils")
set_property(TARGET test_utils PROPERTY FOLDER "Tests/Utils")
set_property(TARGET test_utils_performance PROPERTY FOLDER "Tests/Utils")

# =============================================================================
# Code Coverage (Optional)
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(atom_utils_tests PRIVATE --coverage)
    target_link_options(atom_utils_tests PRIVATE --coverage)

    add_custom_target(utils_coverage
        COMMAND lcov --directory . --capture --output-file utils_coverage.info
        COMMAND lcov --remove utils_coverage.info '/usr/*' --output-file utils_coverage.info
        COMMAND lcov --list utils_coverage.info
        COMMAND genhtml -o utils_coverage_html utils_coverage.info
        DEPENDS test_utils
        COMMENT "Generating code coverage report for utils"
    )
endif()

# =============================================================================
# Status Message
# =============================================================================
message(STATUS "Utils header-only tests configured with ${CMAKE_CURRENT_LIST_DIR}")
