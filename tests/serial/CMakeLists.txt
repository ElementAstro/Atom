# CMakeLists.txt for Atom Serial Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom Serial Tests
# Description: Test suite for Atom serial module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-serial-tests VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Ensure the serial module is available
if(NOT TARGET atom-serial)
    message(FATAL_ERROR "atom-serial target not found. Make sure ATOM_BUILD_SERIAL is enabled.")
endif()

# Include test infrastructure
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# Test source files
set(TEST_SOURCES
    test_scanner.cpp
    test_serial.cpp
    test_usb.cpp
)

# Test header files
set(TEST_HEADERS
    test_bluetooth_serial.hpp
    test_serial_port.hpp
)

# Create test executable
add_executable(atom_serial_tests ${TEST_SOURCES} ${TEST_HEADERS})

# Link libraries
target_link_libraries(atom_serial_tests
    PRIVATE
    atom-serial
    atom-error
    atom-log
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Set include directories
target_include_directories(atom_serial_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# Set C++ standard
target_compile_features(atom_serial_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_serial_tests PRIVATE /W4)
else()
    target_compile_options(atom_serial_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(atom_serial_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    LABELS "serial"
)

# Add custom target for running only serial tests
add_custom_target(test_serial
    COMMAND ${CMAKE_CTEST_COMMAND} -L serial --output-on-failure
    DEPENDS atom_serial_tests
    COMMENT "Running serial module tests"
)

# Set IDE folder
set_property(TARGET atom_serial_tests PROPERTY FOLDER "Tests/Serial")
if(TARGET test_serial)
    set_property(TARGET test_serial PROPERTY FOLDER "Tests/Serial")
endif()

message(STATUS "Serial tests configured")
