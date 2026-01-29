# CMakeLists.txt Template for Atom Test Modules This project is licensed under
# the terms of the GPL3 license.
#
# Project Name: Atom-System-Tests Description: Test Suite for Atom System Module
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-system-tests
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
if(NOT TARGET atom-system)
  message(
    FATAL_ERROR
      "atom-system target not found. Make sure ATOM_BUILD_SYSTEM is enabled.")
endif()

# Tests are now enabled after API alignment work

# =============================================================================
# Test Source Configuration
# =============================================================================
# Include subdirectories that contain organized tests
add_subdirectory(clipboard)
add_subdirectory(core)
add_subdirectory(debug)
add_subdirectory(hardware)
add_subdirectory(info)
add_subdirectory(network)
add_subdirectory(power)
add_subdirectory(process)
add_subdirectory(registry)
add_subdirectory(scheduling)
add_subdirectory(shortcut)
add_subdirectory(signals)
add_subdirectory(storage)

# Collect all test sources from subdirectories
set(TEST_SOURCES
    # Root level tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_header_only.cpp
    # Subdirectory tests
    ${SYSTEM_CLIPBOARD_TEST_SOURCES}
    ${SYSTEM_CORE_TEST_SOURCES}
    ${SYSTEM_DEBUG_TEST_SOURCES}
    ${SYSTEM_HARDWARE_TEST_SOURCES}
    ${SYSTEM_INFO_TEST_SOURCES}
    ${SYSTEM_NETWORK_TEST_SOURCES}
    ${SYSTEM_POWER_TEST_SOURCES}
    ${SYSTEM_PROCESS_TEST_SOURCES}
    ${SYSTEM_REGISTRY_TEST_SOURCES}
    ${SYSTEM_SCHEDULING_TEST_SOURCES}
    ${SYSTEM_SHORTCUT_TEST_SOURCES}
    ${SYSTEM_SIGNALS_TEST_SOURCES}
    ${SYSTEM_STORAGE_TEST_SOURCES})

# Remove any empty entries
list(REMOVE_ITEM TEST_SOURCES "")

# =============================================================================
# Create Test Executable
# =============================================================================
# Use unique name to avoid file lock issues during development
add_executable(atom_sys_test ${TEST_SOURCES})

# =============================================================================
# Source Grouping for IDE Organization
# =============================================================================
source_group("Tests/System/Root"
             FILES ${CMAKE_CURRENT_SOURCE_DIR}/test_header_only.cpp)
source_group("Tests/System/Clipboard" FILES ${SYSTEM_CLIPBOARD_TEST_SOURCES})
source_group("Tests/System/Core" FILES ${SYSTEM_CORE_TEST_SOURCES})
source_group("Tests/System/Debug" FILES ${SYSTEM_DEBUG_TEST_SOURCES})
source_group("Tests/System/Hardware" FILES ${SYSTEM_HARDWARE_TEST_SOURCES})
source_group("Tests/System/Info" FILES ${SYSTEM_INFO_TEST_SOURCES})
source_group("Tests/System/Network" FILES ${SYSTEM_NETWORK_TEST_SOURCES})
source_group("Tests/System/Power" FILES ${SYSTEM_POWER_TEST_SOURCES})
source_group("Tests/System/Process" FILES ${SYSTEM_PROCESS_TEST_SOURCES})
source_group("Tests/System/Registry" FILES ${SYSTEM_REGISTRY_TEST_SOURCES})
source_group("Tests/System/Scheduling" FILES ${SYSTEM_SCHEDULING_TEST_SOURCES})
source_group("Tests/System/Shortcut" FILES ${SYSTEM_SHORTCUT_TEST_SOURCES})
source_group("Tests/System/Signals" FILES ${SYSTEM_SIGNALS_TEST_SOURCES})
source_group("Tests/System/Storage" FILES ${SYSTEM_STORAGE_TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(
  atom_sys_test
  PRIVATE # Test infrastructure
          atom-test-common
          # Module under test and its dependencies
          atom-system
          atom-error
          # Additional dependencies (system module specific)
          atom-log
          # System libraries
          Threads::Threads)

# Platform-specific libraries
if(WIN32)
  target_link_libraries(atom_sys_test PRIVATE ws2_32 wsock32 bthprops wtsapi32
                                              netapi32)
  # Link shortcut detector for shortcut tests
  if(TARGET shortcut_detector_static)
    target_link_libraries(atom_sys_test PRIVATE shortcut_detector_static)
  endif()
endif()

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_sys_test PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
  target_compile_options(atom_sys_test PRIVATE /W4)
else()
  target_compile_options(atom_sys_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

include(GoogleTest)
# Temporarily disabled due to DLL dependency issues during test discovery
# gtest_discover_tests(atom_sys_test WORKING_DIRECTORY
# ${CMAKE_CURRENT_SOURCE_DIR} PROPERTIES LABELS "system" TIMEOUT 300 )

# Manual test registration as workaround
add_test(
  NAME atom_sys_test
  COMMAND atom_sys_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(atom_sys_test PROPERTIES LABELS "system" TIMEOUT 300)

# =============================================================================
# Custom Test Targets
# =============================================================================
# Add custom target for running only this module's tests
add_custom_target(
  test_system
  COMMAND ${CMAKE_CTEST_COMMAND} -L system --output-on-failure
  DEPENDS atom_sys_test
  COMMENT "Running system module tests")

# Add performance test target (if applicable)
add_custom_target(
  test_system_performance
  COMMAND atom_sys_test --gtest_filter="*Performance*:*Benchmark*"
  DEPENDS atom_sys_test
  COMMENT "Running system performance tests")

# =============================================================================
# IDE Organization
# =============================================================================
set_property(TARGET atom_sys_test PROPERTY FOLDER "Tests/System")
set_property(TARGET test_system PROPERTY FOLDER "Tests/System")
set_property(TARGET test_system_performance PROPERTY FOLDER "Tests/System")
