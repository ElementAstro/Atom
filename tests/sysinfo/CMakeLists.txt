# CMakeLists.txt for Atom SysInfo Tests This project is licensed under the terms
# of the GPL3 license.
#
# Project Name: Atom-SysInfo-Tests Description: Comprehensive Test Suite for
# Atom System Information Library Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-sysinfo-tests
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
# Custom Dependency Management (Preserved)
# =============================================================================

# Include parent directory for sysinfo headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Bring in required Atom modules as subdirectories so targets are available
# locally Avoid duplicate target definitions if building from the repository
# top-level
if(NOT TARGET atom-error)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/error
                   ${CMAKE_CURRENT_BINARY_DIR}/atom-error)
endif()
if(NOT TARGET atom-sysinfo)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/sysinfo
                   ${CMAKE_CURRENT_BINARY_DIR}/atom-sysinfo)
endif()

# =============================================================================
# Test Source Configuration
# =============================================================================
# Test source files
set(TEST_SOURCES
    test_sysinfo.cpp
    battery.cpp
    bios.cpp
    cpu.cpp
    disk.cpp
    gpu.cpp
    locale.cpp
    memory.cpp
    os.cpp
    sn.cpp
    sysinfo_printer.cpp
    virtual.cpp
    wifi.cpp
    wm.cpp)

# =============================================================================
# Create Test Executable
# =============================================================================
add_executable(atom_sysinfo_tests ${TEST_SOURCES})

# =============================================================================
# Link Libraries
# =============================================================================
target_link_libraries(
  atom_sysinfo_tests
  PRIVATE # Test infrastructure (if available)
          $<TARGET_NAME_IF_EXISTS:atom-test-common>
          # Module under test
          atom-sysinfo
          # GoogleTest
          GTest::GTest
          GTest::Main
          # System libraries
          Threads::Threads)

# =============================================================================
# Target Configuration
# =============================================================================
# Set C++ standard
target_compile_features(atom_sysinfo_tests PRIVATE cxx_std_20)

# Test configuration
set_target_properties(atom_sysinfo_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                    ${CMAKE_BINARY_DIR}/bin)

# Compiler-specific options
if(MSVC)
  target_compile_options(atom_sysinfo_tests PRIVATE /W4)
else()
  target_compile_options(atom_sysinfo_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Test Registration
# =============================================================================
enable_testing()

# Include GoogleTest for standardized test discovery
include(GoogleTest)
# Temporarily disabled due to timeout issues during test discovery
# gtest_discover_tests(atom_sysinfo_tests WORKING_DIRECTORY
# ${CMAKE_CURRENT_SOURCE_DIR} PROPERTIES LABELS "sysinfo;unit" TIMEOUT 300 )

# Manual test registration as workaround
add_test(
  NAME atom_sysinfo_tests
  COMMAND atom_sysinfo_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(atom_sysinfo_tests PROPERTIES LABELS "sysinfo;unit"
                                                   TIMEOUT 300)

# =============================================================================
# Custom Test Cases (Preserved)
# =============================================================================
# Add test cases
add_test(NAME CPUInfoTests COMMAND ${PROJECT_NAME}
                                   --gtest_filter="CPUInfoTest.*:CpuTest.*")
add_test(NAME MemoryInfoTests
         COMMAND ${PROJECT_NAME} --gtest_filter="MemoryInfoTest.*:MemoryTest.*")
add_test(NAME DiskInfoTests COMMAND ${PROJECT_NAME}
                                    --gtest_filter="DiskInfoTest.*:DiskTest.*")
add_test(NAME GPUInfoTests COMMAND ${PROJECT_NAME}
                                   --gtest_filter="GPUInfoTest.*:GpuTest.*")
add_test(NAME BatteryInfoTests
         COMMAND ${PROJECT_NAME}
                 --gtest_filter="BatteryInfoTest.*:BatteryTest.*")
add_test(NAME OSInfoTests COMMAND ${PROJECT_NAME}
                                  --gtest_filter="OSInfoTest.*:OsTest.*")
add_test(NAME BIOSInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="BiosTest.*")
add_test(NAME LocaleInfoTests COMMAND ${PROJECT_NAME}
                                      --gtest_filter="LocaleTest.*")
add_test(NAME SerialNumberTests COMMAND ${PROJECT_NAME}
                                        --gtest_filter="SnTest.*")
add_test(NAME VirtualEnvTests COMMAND ${PROJECT_NAME}
                                      --gtest_filter="VirtualTest.*")
add_test(NAME WindowManagerTests COMMAND ${PROJECT_NAME}
                                         --gtest_filter="WmTest.*")
add_test(NAME NetworkInfoTests
         COMMAND ${PROJECT_NAME} --gtest_filter="NetworkInfoTest.*:WifiTest.*")
add_test(NAME SysInfoPrinterTests
         COMMAND ${PROJECT_NAME}
                 --gtest_filter="SysInfoPrinterTest.*:SysInfoPrinterTest.*")
add_test(NAME IntegrationTests
         COMMAND ${PROJECT_NAME} --gtest_filter="SysInfoIntegrationTest.*")

# Set test properties for better organization and timeout handling
set_tests_properties(CPUInfoTests PROPERTIES LABELS "hardware;cpu;unit" TIMEOUT
                                             30)

set_tests_properties(MemoryInfoTests PROPERTIES LABELS "hardware;memory;unit"
                                                TIMEOUT 30)

set_tests_properties(DiskInfoTests PROPERTIES LABELS "hardware;disk;unit"
                                              TIMEOUT 60)

set_tests_properties(GPUInfoTests PROPERTIES LABELS "hardware;gpu;unit" TIMEOUT
                                             30)

set_tests_properties(BatteryInfoTests PROPERTIES LABELS "hardware;battery;unit"
                                                 TIMEOUT 30)

set_tests_properties(OSInfoTests PROPERTIES LABELS "system;os;unit" TIMEOUT 30)

set_tests_properties(BIOSInfoTests
                     PROPERTIES LABELS "hardware;bios;security;unit" TIMEOUT 30)

set_tests_properties(LocaleInfoTests PROPERTIES LABELS "system;locale;unit"
                                                TIMEOUT 15)

set_tests_properties(SerialNumberTests PROPERTIES LABELS "hardware;serial;unit"
                                                  TIMEOUT 30)

set_tests_properties(VirtualEnvTests PROPERTIES LABELS "system;virtual;unit"
                                                TIMEOUT 30)

set_tests_properties(WindowManagerTests PROPERTIES LABELS "system;wm;unit"
                                                   TIMEOUT 30)

set_tests_properties(NetworkInfoTests PROPERTIES LABELS "network;wifi;unit"
                                                 TIMEOUT 60)

set_tests_properties(SysInfoPrinterTests
                     PROPERTIES LABELS "utility;printer;unit" TIMEOUT 30)

set_tests_properties(IntegrationTests PROPERTIES LABELS "integration;full"
                                                 TIMEOUT 120)

# Custom test targets for better organization
add_custom_target(
  run_sysinfo_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "sysinfo"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running all system information tests")

add_custom_target(
  run_hardware_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "hardware"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running hardware information tests")

add_custom_target(
  run_system_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "system"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running system information tests")

add_custom_target(
  run_network_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "network"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running network information tests")

add_custom_target(
  run_utility_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "utility"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running utility tests")

# Test category targets for better organization
add_custom_target(
  run_unit_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "unit"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running unit tests only")

add_custom_target(
  run_integration_tests_only
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "integration"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running integration tests only")

add_custom_target(
  run_sysinfo_performance_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "performance"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running sysinfo performance-related tests")

add_custom_target(
  run_sysinfo_security_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "security"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running sysinfo security-related tests")

add_custom_target(
  run_quick_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --verbose -L "unit" -E
          "performance|load|stress"
  DEPENDS atom_sysinfo_tests
  COMMENT "Running quick tests (excluding performance tests)")

# =============================================================================
# Code Coverage Support
# =============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(atom_sysinfo_tests PRIVATE --coverage)
  target_link_options(atom_sysinfo_tests PRIVATE --coverage)

  add_custom_target(
    sysinfo_coverage
    COMMAND lcov --directory . --capture --output-file coverage.info
    COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
    COMMAND lcov --list coverage.info
    COMMAND genhtml -o coverage_html coverage.info
    DEPENDS run_sysinfo_tests
    COMMENT "Generating code coverage report for system information")
endif()

# Set IDE folder organization
set_property(TARGET atom_sysinfo_tests PROPERTY FOLDER "Tests/SysInfo")
set_property(TARGET run_sysinfo_tests PROPERTY FOLDER "Tests/SysInfo/Custom")
set_property(TARGET run_hardware_tests PROPERTY FOLDER "Tests/SysInfo/Custom")
set_property(TARGET run_system_tests PROPERTY FOLDER "Tests/SysInfo/Custom")
set_property(TARGET run_network_tests PROPERTY FOLDER "Tests/SysInfo/Custom")
set_property(TARGET run_utility_tests PROPERTY FOLDER "Tests/SysInfo/Custom")
