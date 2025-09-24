# CMakeLists.txt for Atom Web Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom Web Tests
# Description: Test suite for Atom web module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-web-tests VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Ensure the web module is available
if(NOT TARGET atom-web)
    message(FATAL_ERROR "atom-web target not found. Make sure ATOM_BUILD_WEB is enabled.")
endif()

# Include test infrastructure
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# Test source files
set(TEST_SOURCES
    runners/run_all_tests.cpp
    runners/test_utils.cpp
)

# Test header files
set(TEST_HEADERS
    unit/test_address.hpp
    unit/test_curl.hpp
    unit/test_downloader.hpp
    unit/test_httpparser.hpp
    unit/test_integration.hpp
    unit/test_minetype.hpp
    unit/test_time.hpp
)

# Create test executable
add_executable(atom_web_tests ${TEST_SOURCES} ${TEST_HEADERS})

# Link libraries
target_link_libraries(atom_web_tests
    PRIVATE
    atom-web
    atom-error
    atom-log
    atom-utils
    atom-io
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Find and link CURL if available
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(atom_web_tests PRIVATE CURL::libcurl)
endif()

# Set include directories
target_include_directories(atom_web_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/unit
    ${CMAKE_CURRENT_SOURCE_DIR}/runners
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# Set C++ standard
target_compile_features(atom_web_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_web_tests PRIVATE /W4)
else()
    target_compile_options(atom_web_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(atom_web_tests PRIVATE ws2_32 wsock32 wininet)
endif()

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(atom_web_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    LABELS "web"
)

# Add custom target for running only web tests
add_custom_target(test_web
    COMMAND ${CMAKE_CTEST_COMMAND} -L web --output-on-failure
    DEPENDS atom_web_tests
    COMMENT "Running web module tests"
)

# Set IDE folder
set_property(TARGET atom_web_tests PROPERTY FOLDER "Tests/Web")
if(TARGET test_web)
    set_property(TARGET test_web PROPERTY FOLDER "Tests/Web")
endif()

message(STATUS "Web tests configured")
