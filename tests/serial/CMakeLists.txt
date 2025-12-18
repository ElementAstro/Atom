# CMakeLists.txt Template for Atom Test Modules This project is licensed under
# the terms of the GPL3 license.
#
# Project Name: Atom-Serial-Tests Description: Test Suite for Atom Serial Module
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-serial-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Check for libusb (USB tests require it)
set(LIBUSB_FOUND FALSE)
if(WIN32)
  # On Windows, check for libusb via vcpkg or manual installation
  find_path(LIBUSB_INCLUDE_DIR libusb-1.0/libusb.h)
  find_library(LIBUSB_LIBRARY NAMES libusb-1.0 usb-1.0)
  if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARY)
    set(LIBUSB_FOUND TRUE)
    set(LIBUSB_INCLUDE_DIRS ${LIBUSB_INCLUDE_DIR})
    set(LIBUSB_LIBRARIES ${LIBUSB_LIBRARY})
  endif()
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBUSB QUIET libusb-1.0)
  endif()
endif()

# =============================================================================
# Include Test Infrastructure
# =============================================================================
# Include the standardized test infrastructure
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests"
                   "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-serial)
  message(
    FATAL_ERROR
      "atom-serial target not found. Make sure ATOM_BUILD_SERIAL is enabled.")
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# Core test files - organized by subdirectory matching implementation structure
set(TEST_SOURCES
    # Core module tests
    ${PROJECT_SOURCE_DIR}/core/test_scanner.cpp
    ${PROJECT_SOURCE_DIR}/core/test_serial_port.cpp
    # Bluetooth module tests
    ${PROJECT_SOURCE_DIR}/bluetooth/test_bluetooth_serial.cpp
    # Platform-specific tests
    ${PROJECT_SOURCE_DIR}/platform/test_serial_port_platform.cpp)

# Add USB tests only if libusb is available
if(LIBUSB_FOUND)
  list(APPEND TEST_SOURCES ${PROJECT_SOURCE_DIR}/usb/test_usb.cpp)
  message(STATUS "USB tests enabled (libusb found)")
else()
  message(STATUS "USB tests disabled (libusb not found)")
endif()

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_serial_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(
  atom_serial_tests
  PRIVATE # Test infrastructure
          atom-test-common
          # Module under test and its dependencies
          atom-serial
          atom-error
          # Additional dependencies (serial module specific)
          atom-log
          # Google Test and Mock
          GTest::gtest
          GTest::gtest_main
          GTest::gmock
          GTest::gmock_main
          # System libraries
          Threads::Threads)

# Link libusb only if available
if(LIBUSB_FOUND)
  target_link_libraries(atom_serial_tests PRIVATE ${LIBUSB_LIBRARIES})
  target_compile_definitions(atom_serial_tests PRIVATE ATOM_SERIAL_HAS_USB)
endif()

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_serial_tests PRIVATE cxx_std_20)

# Add include directories for libusb
if(LIBUSB_INCLUDE_DIRS)
  target_include_directories(atom_serial_tests PRIVATE ${LIBUSB_INCLUDE_DIRS})
endif()

# Add compiler flags
if(MSVC)
  target_compile_options(atom_serial_tests PRIVATE /W4)
else()
  target_compile_options(atom_serial_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_serial_tests WORKING_DIRECTORY
# ${CMAKE_CURRENT_SOURCE_DIR} PROPERTIES LABELS "serial" TIMEOUT 300 )

# Manual test registration as workaround
add_test(
  NAME atom_serial_tests
  COMMAND atom_serial_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(atom_serial_tests PROPERTIES LABELS "serial" TIMEOUT 300)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(
  test_serial
  COMMAND ${CMAKE_CTEST_COMMAND} -L serial --output-on-failure
  DEPENDS atom_serial_tests
  COMMENT "Running serial module tests")

# Add performance test target (if applicable)
add_custom_target(
  test_serial_performance
  COMMAND atom_serial_tests --gtest_filter="*Performance*:*Benchmark*"
  DEPENDS atom_serial_tests
  COMMENT "Running serial performance tests")

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_serial_tests PROPERTY FOLDER "Tests/Serial")
set_property(TARGET test_serial PROPERTY FOLDER "Tests/Serial")
set_property(TARGET test_serial_performance PROPERTY FOLDER "Tests/Serial")

# =============================================================================
# DLL Copying (Windows)
# =============================================================================
if(WIN32)
  if(COMMAND copy_test_dlls)
    copy_test_dlls(atom_serial_tests)
  endif()

  add_custom_command(
    TARGET atom_serial_tests
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:atom_serial_tests>
      $<TARGET_FILE_DIR:atom_serial_tests>
    COMMAND_EXPAND_LISTS)
endif()
