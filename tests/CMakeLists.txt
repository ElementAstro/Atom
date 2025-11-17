# CMakeLists.txt for Atom Test Suite This project is licensed under the terms of
# the GPL3 license.
#
# Project Name: Atom Test Suite Description: Comprehensive test suite for Atom
# framework Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-test-suite
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include test build options if not already included
if(NOT DEFINED ATOM_TEST_BUILD_ALL)
  include(${CMAKE_SOURCE_DIR}/cmake/TestsBuildOptions.cmake)
endif()

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Function to copy required DLLs to test executable directory
function(copy_test_dlls TARGET_NAME)
  if(WIN32)
    # List of all DLL targets that might be needed by tests
    set(DLL_TARGETS atom-error atom-log atom-component)

    # Copy each DLL if it exists as a target
    foreach(DLL_TARGET ${DLL_TARGETS})
      if(TARGET ${DLL_TARGET})
        add_custom_command(
          TARGET ${TARGET_NAME}
          POST_BUILD
          COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${DLL_TARGET}>"
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
          COMMENT "Copying ${DLL_TARGET} DLL to ${TARGET_NAME} directory"
          VERBATIM)
      endif()
    endforeach()
  endif()
endfunction()

# Make the function available globally
macro(copy_test_dlls_macro TARGET_NAME)
  copy_test_dlls(${TARGET_NAME})
endmacro()

# Add a global post-build step to copy DLLs to all test directories
if(WIN32)
  # This custom target will copy DLLs after all tests are built
  add_custom_target(
    copy_all_test_dlls ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Copying DLLs to all test directories..."
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/atom/error/libatom-error.dll"
      "${CMAKE_CURRENT_BINARY_DIR}/"
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/atom/log/libatom-log.dll"
      "${CMAKE_CURRENT_BINARY_DIR}/"
    DEPENDS atom-error atom-log
    COMMENT "Copying DLLs to test root directory")
endif()

# Include test infrastructure first
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()

# Conditionally add test subdirectories based on build options

# Algorithm tests
if(ATOM_TEST_BUILD_ALGORITHM
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/algorithm/CMakeLists.txt")
  add_subdirectory(algorithm)
endif()

# Async tests
if(ATOM_TEST_BUILD_ASYNC AND EXISTS
                             "${CMAKE_CURRENT_SOURCE_DIR}/async/CMakeLists.txt")
  add_subdirectory(async)
endif()

# Components tests
if(ATOM_TEST_BUILD_COMPONENTS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components/CMakeLists.txt")
  add_subdirectory(components)
endif()

# Connection tests
if(ATOM_TEST_BUILD_CONNECTION
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/connection/CMakeLists.txt")
  add_subdirectory(connection)
endif()

# Error tests
if(ATOM_TEST_BUILD_ERROR AND EXISTS
                             "${CMAKE_CURRENT_SOURCE_DIR}/error/CMakeLists.txt")
  add_subdirectory(error)
endif()

# Extra tests
if(ATOM_TEST_BUILD_EXTRA AND EXISTS
                             "${CMAKE_CURRENT_SOURCE_DIR}/extra/CMakeLists.txt")
  add_subdirectory(extra)
endif()

# Image tests
if(ATOM_TEST_BUILD_IMAGE AND EXISTS
                             "${CMAKE_CURRENT_SOURCE_DIR}/image/CMakeLists.txt")
  add_subdirectory(image)
endif()

# IO tests
if(ATOM_TEST_BUILD_IO AND EXISTS
                          "${CMAKE_CURRENT_SOURCE_DIR}/io/CMakeLists.txt")
  add_subdirectory(io)
endif()

# Memory tests
if(ATOM_TEST_BUILD_MEMORY
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/memory/CMakeLists.txt")
  add_subdirectory(memory)
endif()

# Meta tests
if(ATOM_TEST_BUILD_META AND EXISTS
                            "${CMAKE_CURRENT_SOURCE_DIR}/meta/CMakeLists.txt")
  add_subdirectory(meta)
endif()

# Search tests
if(ATOM_TEST_BUILD_SEARCH
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/search/CMakeLists.txt")
  add_subdirectory(search)
endif()

# Secret tests
if(ATOM_TEST_BUILD_SECRET
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/secret/CMakeLists.txt")
  add_subdirectory(secret)
endif()

# Serial tests
if(ATOM_TEST_BUILD_SERIAL
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/serial/CMakeLists.txt")
  add_subdirectory(serial)
endif()

# Sysinfo tests
if(ATOM_TEST_BUILD_SYSINFO
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sysinfo/CMakeLists.txt")
  add_subdirectory(sysinfo)
endif()

# System tests
if(ATOM_TEST_BUILD_SYSTEM
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/system/CMakeLists.txt")
  add_subdirectory(system)
endif()

# Type tests
if(ATOM_TEST_BUILD_TYPE AND EXISTS
                            "${CMAKE_CURRENT_SOURCE_DIR}/type/CMakeLists.txt")
  add_subdirectory(type)
endif()

# Utils tests
if(ATOM_TEST_BUILD_UTILS AND EXISTS
                             "${CMAKE_CURRENT_SOURCE_DIR}/utils/CMakeLists.txt")
  add_subdirectory(utils)
endif()

# Web tests
if(ATOM_TEST_BUILD_WEB AND EXISTS
                           "${CMAKE_CURRENT_SOURCE_DIR}/web/CMakeLists.txt")
  add_subdirectory(web)
endif()

# Containers tests
if(ATOM_TEST_BUILD_CONTAINERS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/containers/CMakeLists.txt")
  add_subdirectory(containers)
endif()

# Log tests
if(ATOM_TEST_BUILD_LOG AND EXISTS
                           "${CMAKE_CURRENT_SOURCE_DIR}/log/CMakeLists.txt")
  add_subdirectory(log)
endif()

# Set test targets grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(TEST_GROUP_NAME "Tests")

# Recursively set all test targets to the test group
get_property(
  TARGETS
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY BUILDSYSTEM_TARGETS)
foreach(TARGET ${TARGETS})
  set_property(TARGET ${TARGET} PROPERTY FOLDER ${TEST_GROUP_NAME})
endforeach()

# Add custom targets for running tests by category
add_custom_target(
  test_all_modules
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMENT "Running all module tests")

# Add custom target for running tests with specific labels
add_custom_target(
  test_core_modules
  COMMAND ${CMAKE_CTEST_COMMAND} -L "error|log|meta|type|utils"
          --output-on-failure
  COMMENT "Running core module tests")

add_custom_target(
  test_io_modules
  COMMAND ${CMAKE_CTEST_COMMAND} -L "io|image|serial" --output-on-failure
  COMMENT "Running I/O module tests")

add_custom_target(
  test_system_modules
  COMMAND ${CMAKE_CTEST_COMMAND} -L "system|sysinfo" --output-on-failure
  COMMENT "Running system module tests")

add_custom_target(
  test_network_modules
  COMMAND ${CMAKE_CTEST_COMMAND} -L "web|connection" --output-on-failure
  COMMENT "Running network module tests")

# Set IDE folders for custom test targets
set_property(TARGET test_all_modules PROPERTY FOLDER "Tests/Custom")
set_property(TARGET test_core_modules PROPERTY FOLDER "Tests/Custom")
set_property(TARGET test_io_modules PROPERTY FOLDER "Tests/Custom")
set_property(TARGET test_system_modules PROPERTY FOLDER "Tests/Custom")
set_property(TARGET test_network_modules PROPERTY FOLDER "Tests/Custom")

# =============================================================================
# Unified Test Runner
# =============================================================================
# Build the unified test runner executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/run_all_tests.cpp")
  add_executable(run_all_tests run_all_tests.cpp)

  target_link_libraries(
    run_all_tests
    PRIVATE # Atom test framework
            atom-test-common
            # Core modules needed for testing
            atom-error
            atom-utils
            # GoogleTest
            GTest::gtest
            GTest::gtest_main
            # System libraries
            Threads::Threads)

  target_include_directories(
    run_all_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/atom
                          ${CMAKE_SOURCE_DIR}/tests)

  target_compile_features(run_all_tests PRIVATE cxx_std_20)

  # Add compiler flags
  if(MSVC)
    target_compile_options(run_all_tests PRIVATE /W4)
  else()
    target_compile_options(run_all_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  # Platform-specific libraries
  if(WIN32)
    target_link_libraries(run_all_tests PRIVATE ws2_32 wsock32)
  endif()

  # Add test to CTest
  add_test(NAME unified_test_runner COMMAND run_all_tests --list)
  set_tests_properties(unified_test_runner
                       PROPERTIES LABELS "unified;infrastructure" TIMEOUT 60)

  # Set IDE folder
  set_property(TARGET run_all_tests PROPERTY FOLDER "Tests/Infrastructure")

  # Custom target for running tests through unified runner
  add_custom_target(
    test_unified
    COMMAND run_all_tests --verbose
    DEPENDS run_all_tests
    COMMENT "Running all tests through unified test runner")

  set_property(TARGET test_unified PROPERTY FOLDER "Tests/Infrastructure")

  message(STATUS "Unified test runner configured")
endif()

# =============================================================================
# Copy DLLs to All Test Directories (Windows)
# =============================================================================
if(WIN32)
  # Get all test executables
  set(TEST_SUBDIRS
      algorithm
      async
      components
      connection
      containers
      error
      extra
      image
      io
      log
      memory
      meta
      search
      secret
      serial
      sysinfo
      type
      utils
      web)

  # Copy DLLs to each test subdirectory
  foreach(SUBDIR ${TEST_SUBDIRS})
    if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}")
      add_custom_command(
        TARGET copy_all_test_dlls
        POST_BUILD
        COMMAND
          ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_BINARY_DIR}/atom/error/libatom-error.dll"
          "${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}/"
        COMMAND
          ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_BINARY_DIR}/atom/log/libatom-log.dll"
          "${CMAKE_CURRENT_BINARY_DIR}/${SUBDIR}/"
        COMMENT "Copying DLLs to ${SUBDIR} test directory"
        VERBATIM)
    endif()
  endforeach()

  message(STATUS "Configured DLL copying for all test directories")
endif()
