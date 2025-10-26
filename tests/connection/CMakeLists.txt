# CMakeLists.txt Template for Atom Test Modules This project is licensed under
# the terms of the GPL3 license.
#
# Project Name: Atom-Connection-Tests Description: Test Suite for Atom
# Connection Module Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-connection-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

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
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests"
                   "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-connection)
  message(
    FATAL_ERROR
      "atom-connection target not found. Make sure ATOM_BUILD_CONNECTION is enabled."
  )
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# For modules with .cpp test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Remove problematic test files (preserve existing exclusions)
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/build/.*")
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/udpclient.cpp)
# Temporarily disable tests with API mismatches
list(REMOVE_ITEM TEST_SOURCES
     ${PROJECT_SOURCE_DIR}/fifo/test_async_fifoserver.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/ssh/test_sshserver.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/ssh/test_sshclient.cpp)
list(REMOVE_ITEM TEST_SOURCES
     ${PROJECT_SOURCE_DIR}/socket/test_async_sockethub.cpp)

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_connection_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(
  atom_connection_tests
  PRIVATE # Test infrastructure
          atom-test-common
          # Module under test and its dependencies
          atom-connection
          atom-error
          # Additional dependencies (connection module specific)
          atom-log
          # System libraries
          Threads::Threads)

# Add OpenSSL libraries for SSL/TLS support
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  target_link_libraries(atom_connection_tests PRIVATE OpenSSL::SSL
                                                      OpenSSL::Crypto)
endif()

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_connection_tests PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
  target_compile_options(atom_connection_tests PRIVATE /W4)
else()
  target_compile_options(atom_connection_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add compiler definitions for Windows
if(WIN32)
  target_compile_definitions(
    atom_connection_tests PRIVATE _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN
                                  NOMINMAX)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

# Use simple test registration instead of discovery to avoid DLL issues
add_test(
  NAME atom_connection_tests
  COMMAND atom_connection_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(atom_connection_tests PROPERTIES LABELS "connection"
                                                      TIMEOUT 300)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(
  test_connection
  COMMAND ${CMAKE_CTEST_COMMAND} -L connection --output-on-failure
  DEPENDS atom_connection_tests
  COMMENT "Running connection module tests")

# Add performance test target (if applicable)
add_custom_target(
  test_connection_performance
  COMMAND atom_connection_tests --gtest_filter="*Performance*:*Benchmark*"
  DEPENDS atom_connection_tests
  COMMENT "Running connection performance tests")

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_connection_tests PROPERTY FOLDER "Tests/Connection")
set_property(TARGET test_connection PROPERTY FOLDER "Tests/Connection")
set_property(TARGET test_connection_performance PROPERTY FOLDER
                                                         "Tests/Connection")
