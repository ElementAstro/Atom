# CMakeLists.txt for Atom-Log Tests This project is licensed under the terms of
# the GPL3 license.
#
# Project Name: Atom-Log-Tests Description: Test Suite for Atom Log Library
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-log-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Backward-compatible aliases for legacy GTest target names
if(TARGET GTest::gtest AND NOT TARGET GTest::GTest)
  add_library(GTest::GTest ALIAS GTest::gtest)
endif()
if(TARGET GTest::gtest_main AND NOT TARGET GTest::Main)
  add_library(GTest::Main ALIAS GTest::gtest_main)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../tests)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Test sources
set(TEST_SOURCES test_logger.cpp)

# Create test executable
add_executable(atom-log-tests ${TEST_SOURCES})

target_link_libraries(atom-log-tests PRIVATE atom-log GTest::GTest GTest::Main
                                             Threads::Threads)

# Set C++ standard
target_compile_features(atom-log-tests PRIVATE cxx_std_20)

# Compiler-specific options
if(MSVC)
  target_compile_options(atom-log-tests PRIVATE /W4)
else()
  target_compile_options(atom-log-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable testing
enable_testing()

# Add test cases
add_test(NAME LogTests COMMAND atom-log-tests)

# DLL Copying (Windows)
if(WIN32)
  if(COMMAND copy_test_dlls)
    copy_test_dlls(atom-log-tests)
  endif()

  add_custom_command(
    TARGET atom-log-tests
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:atom-log-tests> $<TARGET_FILE_DIR:atom-log-tests>
    COMMAND_EXPAND_LISTS)
endif()

# Custom test targets
add_custom_target(
  run_log_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose
  DEPENDS atom-log-tests
  COMMENT "Running log tests")
