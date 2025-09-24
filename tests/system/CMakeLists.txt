# CMakeLists.txt for Atom System Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom System Tests
# Description: Test suite for Atom system module
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-system-tests VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Ensure the system module is available
if(NOT TARGET atom-system)
    message(FATAL_ERROR "atom-system target not found. Make sure ATOM_BUILD_SYSTEM is enabled.")
endif()

# Include test infrastructure
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests" "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# Test source files
set(TEST_SOURCES
    test_command.cpp
    test_crash_quotes.cpp
    test_device.cpp
    test_gpio.cpp
    test_network_manager.cpp
    test_power.cpp
    test_priority.cpp
    test_process_manager.cpp
    test_stat.cpp
    test_storage.cpp
    test_voltage.cpp
    test_wregistry.cpp
)

# Test header files
set(TEST_HEADERS
    test_crontab.hpp
    test_env.hpp
    test_gpio.hpp
    test_lregistry.hpp
    test_network_manager.hpp
    test_pidwatcher.hpp
    test_signal.hpp
    test_stat.hpp
    test_user.hpp
)

# Create test executable
add_executable(atom_system_tests ${TEST_SOURCES} ${TEST_HEADERS})

# Link libraries
target_link_libraries(atom_system_tests
    PRIVATE
    atom-system
    atom-error
    atom-log
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Set include directories
target_include_directories(atom_system_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/atom
    ${CMAKE_SOURCE_DIR}/tests
)

# Set C++ standard
target_compile_features(atom_system_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(atom_system_tests PRIVATE /W4)
else()
    target_compile_options(atom_system_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(atom_system_tests PRIVATE ws2_32 wsock32)
endif()

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(atom_system_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
    LABELS "system"
)

# Add custom target for running only system tests
add_custom_target(test_system
    COMMAND ${CMAKE_CTEST_COMMAND} -L system --output-on-failure
    DEPENDS atom_system_tests
    COMMENT "Running system module tests"
)

# Set IDE folder
set_property(TARGET atom_system_tests PROPERTY FOLDER "Tests/System")
if(TARGET test_system)
    set_property(TARGET test_system PROPERTY FOLDER "Tests/System")
endif()

message(STATUS "System tests configured")
