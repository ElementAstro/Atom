# CMakeLists.txt for Atom Extra Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom Extra Tests
# Description: Comprehensive test suite for Atom extra components
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-extra-tests VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/extra)

# Function to create test executable for a module
function(create_extra_test_executable MODULE_NAME)
    set(TEST_TARGET atom-extra-${MODULE_NAME}-test)

    # Collect all test source files for the module
    file(GLOB_RECURSE TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}/*.hpp"
    )

    if(TEST_SOURCES)
        # Create test executable
        add_executable(${TEST_TARGET} ${TEST_SOURCES})

        # Link libraries
        target_link_libraries(${TEST_TARGET}
            PRIVATE
            GTest::GTest
            GTest::Main
            GTest::gmock
            GTest::gmock_main
            Threads::Threads
        )

        # Link to the corresponding extra module if it exists
        if(TARGET atom-extra-${MODULE_NAME})
            target_link_libraries(${TEST_TARGET} PRIVATE atom-extra-${MODULE_NAME})
        endif()

        # Add Windows socket libraries for network-related modules
        if(WIN32 AND (MODULE_NAME STREQUAL "beast" OR MODULE_NAME STREQUAL "asio"))
            target_link_libraries(${TEST_TARGET} PRIVATE ws2_32 wsock32 mswsock)
        endif()

        # Add OpenSSL libraries for ASIO module (needed for SSL support)
        if(MODULE_NAME STREQUAL "asio")
            find_package(OpenSSL REQUIRED)
            target_link_libraries(${TEST_TARGET} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        endif()

        # Add TBB libraries for inicpp module (needed for parallel algorithms)
        if(MODULE_NAME STREQUAL "inicpp")
            find_package(TBB REQUIRED)
            target_link_libraries(${TEST_TARGET} PRIVATE TBB::tbb atom-error)
        endif()

        # Set compiler features
        target_compile_features(${TEST_TARGET} PRIVATE cxx_std_20)

        # Compiler flags
        if(MSVC)
            target_compile_options(${TEST_TARGET} PRIVATE /W4)
        else()
            target_compile_options(${TEST_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
        endif()

        # Enable testing
        enable_testing()

        # Add test
        add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})

        # Set test properties
        set_tests_properties(${TEST_TARGET} PROPERTIES
            TIMEOUT 300
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )

        message(STATUS "Created test executable: ${TEST_TARGET}")
    else()
        message(STATUS "No test sources found for module: ${MODULE_NAME}")
    endif()
endfunction()

# List of extra modules to test
set(EXTRA_MODULES
    asio
    beast
    boost
    curl
    dotenv
    iconv
    inicpp
    injection
    pugixml
    spdlog
    uv
)

# Create test executables for each module
foreach(MODULE ${EXTRA_MODULES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}")
        create_extra_test_executable(${MODULE})
    endif()
endforeach()

# Custom test targets
add_custom_target(run_extra_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    COMMENT "Running all extra component tests"
)

add_custom_target(run_extra_tests_parallel
    COMMAND ${CMAKE_CTEST_COMMAND} --parallel 4 --verbose
    COMMENT "Running extra component tests in parallel"
)

# Code coverage (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    add_custom_target(extra_coverage
        COMMAND lcov --directory . --capture --output-file extra_coverage.info
        COMMAND lcov --remove extra_coverage.info '/usr/*' --output-file extra_coverage.info
        COMMAND lcov --remove extra_coverage.info '*/gtest/*' --output-file extra_coverage.info
        COMMAND lcov --list extra_coverage.info
        COMMAND genhtml -o extra_coverage_html extra_coverage.info
        DEPENDS run_extra_tests
        COMMENT "Generating code coverage report for extra components"
    )
endif()

message(STATUS "Atom Extra Tests configuration completed")
