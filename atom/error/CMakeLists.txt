# CMakeLists.txt for Atom-Error This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Error Description: Atom Error Library Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-error
  VERSION 1.0.0
  LANGUAGES C CXX)

# Sources
set(SOURCES exception.cpp stacktrace.cpp)

# Headers
set(HEADERS error_code.hpp stacktrace.hpp)

# Test and example sources
set(TEST_SOURCES test_stacktrace.cpp)
set(BENCHMARK_SOURCES benchmark_stacktrace.cpp)
set(EXAMPLE_SOURCES example_stacktrace.cpp)

# Dependencies
set(LIBS loguru)

# Add meta module dependency for DemangleHelper
if(TARGET atom-meta)
  list(APPEND LIBS atom-meta)
endif()

# Optional atom module dependencies for stacktrace compression/decompression
# These are only needed if ATOM_ENABLE_STACKTRACE_COMPRESSION is defined
if(ATOM_ENABLE_STACKTRACE_COMPRESSION)
  if(TARGET atom-algorithm)
    list(APPEND LIBS atom-algorithm)
  endif()

  if(TARGET atom-io)
    list(APPEND LIBS atom-io)
  endif()

  if(TARGET atom-containers)
    list(APPEND LIBS atom-containers)
  endif()

  add_compile_definitions(ATOM_ENABLE_STACKTRACE_COMPRESSION)
endif()

if(LINUX)
  list(APPEND LIBS dl)
endif()

# Platform-specific libraries for enhanced stacktrace
if(WIN32)
  list(APPEND LIBS dbghelp psapi)
elseif(UNIX AND NOT APPLE)
  list(APPEND LIBS dl)
elseif(APPLE)
  list(APPEND LIBS dl)
endif()

# Optional dependencies
find_package(Boost QUIET COMPONENTS stacktrace)
if(Boost_FOUND)
  add_compile_definitions(ATOM_USE_BOOST)
  list(APPEND LIBS Boost::stacktrace)
  message(STATUS "Boost found - enabling Boost stacktrace support")
endif()

# Note: Using existing atom::io compression component instead of direct zlib

# Google Test for unit testing
find_package(GTest QUIET)
if(GTest_FOUND)
  enable_testing()
  message(STATUS "Google Test found - enabling unit tests")
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Shared Library
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Integration test executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_integration.cpp)
  add_executable(stacktrace_integration_test test_integration.cpp)
  target_link_libraries(stacktrace_integration_test PRIVATE ${PROJECT_NAME})
  message(STATUS "Building stacktrace integration test")
endif()

# Example executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/example_stacktrace.cpp)
  add_executable(stacktrace_example ${EXAMPLE_SOURCES})
  target_link_libraries(stacktrace_example PRIVATE ${PROJECT_NAME})
  message(STATUS "Building stacktrace example")
endif()

# Benchmark executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmark_stacktrace.cpp)
  add_executable(stacktrace_benchmark ${BENCHMARK_SOURCES})
  target_link_libraries(stacktrace_benchmark PRIVATE ${PROJECT_NAME})
  message(STATUS "Building stacktrace benchmark")
endif()

# Unit tests (if Google Test is available)
if(GTest_FOUND AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_stacktrace.cpp)
  add_executable(stacktrace_tests ${TEST_SOURCES})
  target_link_libraries(stacktrace_tests PRIVATE ${PROJECT_NAME} GTest::gtest GTest::gtest_main)

  # Add test to CTest
  add_test(NAME StackTraceUnitTests COMMAND stacktrace_tests)
  set_tests_properties(StackTraceUnitTests PROPERTIES TIMEOUT 300 LABELS "unit;stacktrace")
  message(STATUS "Building stacktrace unit tests")
endif()

# Performance test target
if(TARGET stacktrace_benchmark)
  add_custom_target(perf_test
    COMMAND stacktrace_benchmark
    DEPENDS stacktrace_benchmark
    COMMENT "Running stacktrace performance benchmarks"
  )
endif()

# Install rules
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
