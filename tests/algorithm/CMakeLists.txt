# CMakeLists.txt Template for Atom Test Modules
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Algorithm-Tests
# Description: Test Suite for Atom Algorithm Module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-algorithm-tests VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Try to find GoogleMock (usually included with modern GTest)
# Check if GTest::gmock target exists, otherwise try to find it manually
if(NOT TARGET GTest::gmock)
    find_library(GMOCK_LIBRARY NAMES gmock HINTS ${GTEST_LIBRARY_DIR})
    find_library(GMOCK_MAIN_LIBRARY NAMES gmock_main HINTS ${GTEST_LIBRARY_DIR})
    if(GMOCK_LIBRARY)
        message(STATUS "Found GoogleMock library: ${GMOCK_LIBRARY}")
    else()
        message(WARNING "GoogleMock library not found - some tests may not link properly")
    endif()
endif()

# Try to find fmt library (for logging support)
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    find_library(FMT_LIBRARY NAMES fmt)
    if(FMT_LIBRARY)
        message(STATUS "Found fmt library: ${FMT_LIBRARY}")
    else()
        message(WARNING "fmt library not found - some tests may not link properly")
    endif()
endif()

# Try to find spdlog library (for logging support)
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    find_library(SPDLOG_LIBRARY NAMES spdlog)
    if(SPDLOG_LIBRARY)
        message(STATUS "Found spdlog library: ${SPDLOG_LIBRARY}")
    else()
        message(WARNING "spdlog library not found - some tests may not link properly")
    endif()
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
if(NOT TARGET atom-algorithm)
    message(FATAL_ERROR "atom-algorithm target not found. Make sure ATOM_BUILD_ALGORITHM is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Remove any problematic test files (customize as needed)
# list(FILTER TEST_SOURCES EXCLUDE REGEX ".*test_problematic\.cpp$")

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_algorithm_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(atom_algorithm_tests
    PRIVATE
    # Test infrastructure
    atom-test-common

    # Module under test and its dependencies
    atom-algorithm
    atom-error

    # Additional dependencies (customize based on module needs)
    # atom-log
    # atom-utils
    # atom-io

    # System libraries
    Threads::Threads
)

# Add GoogleMock support if available
if(TARGET GTest::gmock)
    target_link_libraries(atom_algorithm_tests PRIVATE GTest::gmock)
    message(STATUS "Linking GoogleMock via GTest::gmock target")
elseif(GMOCK_LIBRARY)
    target_link_libraries(atom_algorithm_tests PRIVATE ${GMOCK_LIBRARY})
    if(GMOCK_MAIN_LIBRARY)
        target_link_libraries(atom_algorithm_tests PRIVATE ${GMOCK_MAIN_LIBRARY})
    endif()
    message(STATUS "Linking GoogleMock via found libraries")
endif()

# Add fmt support if available
if(TARGET fmt::fmt)
    target_link_libraries(atom_algorithm_tests PRIVATE fmt::fmt)
    message(STATUS "Linking fmt via fmt::fmt target")
elseif(FMT_LIBRARY)
    target_link_libraries(atom_algorithm_tests PRIVATE ${FMT_LIBRARY})
    message(STATUS "Linking fmt via found library")
endif()

# Add spdlog support if available (header-only to avoid conflicts)
if(TARGET spdlog::spdlog_header_only)
    target_link_libraries(atom_algorithm_tests PRIVATE spdlog::spdlog_header_only)
    message(STATUS "Linking spdlog via spdlog::spdlog_header_only target")
elseif(TARGET spdlog::spdlog)
    # Use header-only mode to avoid linking conflicts
    target_link_libraries(atom_algorithm_tests PRIVATE spdlog::spdlog)
    target_compile_definitions(atom_algorithm_tests PRIVATE SPDLOG_HEADER_ONLY)
    message(STATUS "Linking spdlog via spdlog::spdlog target (header-only mode)")
elseif(SPDLOG_LIBRARY)
    target_link_libraries(atom_algorithm_tests PRIVATE ${SPDLOG_LIBRARY})
    message(STATUS "Linking spdlog via found library")
endif()

# =============================================================================
# Platform-Specific Libraries
# =============================================================================
if(WIN32)
    target_link_libraries(atom_algorithm_tests PRIVATE ws2_32 wsock32)
    # Add other Windows-specific libraries as needed
    # target_link_libraries(atom_algorithm_tests PRIVATE kernel32 user32)
endif()

# =============================================================================
# Include Directories
# =============================================================================
target_include_directories(atom_algorithm_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# =============================================================================
# Compiler Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_algorithm_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_algorithm_tests PRIVATE /W4)
else()
    target_compile_options(atom_algorithm_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration with CTest
# =============================================================================
# Enable testing
enable_testing()

# Use GoogleTest discovery for automatic test registration
include(GoogleTest)
# Temporarily disable test discovery due to DLL dependency issues
# gtest_discover_tests(atom_algorithm_tests
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     PROPERTIES
#     LABELS "algorithm"
#     TIMEOUT 300
#     DISCOVERY_TIMEOUT 30
# )

# Add a simple test registration as fallback
add_test(NAME algorithm_tests COMMAND atom_algorithm_tests)
set_tests_properties(algorithm_tests PROPERTIES
    LABELS "algorithm"
    TIMEOUT 300
)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(test_algorithm
    COMMAND ${CMAKE_CTEST_COMMAND} -L algorithm --output-on-failure
    DEPENDS atom_algorithm_tests
    COMMENT "Running algorithm module tests"
)

# Add performance test target (if applicable)
add_custom_target(test_algorithm_performance
    COMMAND atom_algorithm_tests --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS atom_algorithm_tests
    COMMENT "Running algorithm performance tests"
)

# =============================================================================
# IDE Organization
# =============================================================================
# Set IDE folder for better organization
set_property(TARGET atom_algorithm_tests PROPERTY FOLDER "Tests/Algorithm")
if(TARGET test_algorithm)
    set_property(TARGET test_algorithm PROPERTY FOLDER "Tests/Algorithm")
endif()
if(TARGET test_algorithm_performance)
    set_property(TARGET test_algorithm_performance PROPERTY FOLDER "Tests/Algorithm")
endif()

# =============================================================================
# Code Coverage (Optional)
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(atom_algorithm_tests PRIVATE --coverage)
    target_link_options(atom_algorithm_tests PRIVATE --coverage)

    add_custom_target(algorithm_coverage
        COMMAND lcov --directory . --capture --output-file algorithm_coverage.info
        COMMAND lcov --remove algorithm_coverage.info '/usr/*' --output-file algorithm_coverage.info
        COMMAND lcov --list algorithm_coverage.info
        COMMAND genhtml -o algorithm_coverage_html algorithm_coverage.info
        DEPENDS test_algorithm
        COMMENT "Generating code coverage report for algorithm"
    )
endif()

# =============================================================================
# Status Message
# =============================================================================
message(STATUS "Algorithm tests configured successfully")
