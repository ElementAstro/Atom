# CMakeLists.txt for Atom-SysInfo Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-SysInfo-Tests
# Description: Test Suite for Atom System Information Library
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-sysinfo-tests VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include parent directory for sysinfo headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Add sysinfo library as subdirectory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../atom/sysinfo ${CMAKE_CURRENT_BINARY_DIR}/atom-sysinfo)

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
    wm.cpp
)

# Test executable
add_executable(${PROJECT_NAME}
    ${TEST_SOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    atom-sysinfo
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Test configuration
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable testing
enable_testing()

# Add test cases
add_test(NAME CPUInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="CPUInfoTest.*:CpuTest.*")
add_test(NAME MemoryInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="MemoryInfoTest.*:MemoryTest.*")
add_test(NAME DiskInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="DiskInfoTest.*:DiskTest.*")
add_test(NAME GPUInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="GPUInfoTest.*:GpuTest.*")
add_test(NAME BatteryInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="BatteryInfoTest.*:BatteryTest.*")
add_test(NAME OSInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="OSInfoTest.*:OsTest.*")
add_test(NAME BIOSInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="BiosTest.*")
add_test(NAME LocaleInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="LocaleTest.*")
add_test(NAME SerialNumberTests COMMAND ${PROJECT_NAME} --gtest_filter="SnTest.*")
add_test(NAME VirtualEnvTests COMMAND ${PROJECT_NAME} --gtest_filter="VirtualTest.*")
add_test(NAME WindowManagerTests COMMAND ${PROJECT_NAME} --gtest_filter="WmTest.*")
add_test(NAME NetworkInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="NetworkInfoTest.*:WifiTest.*")
add_test(NAME SysInfoPrinterTests COMMAND ${PROJECT_NAME} --gtest_filter="SysInfoPrinterTest.*:SysInfoPrinterTest.*")
add_test(NAME IntegrationTests COMMAND ${PROJECT_NAME} --gtest_filter="SysInfoIntegrationTest.*")

# Set test properties for better organization and timeout handling
set_tests_properties(CPUInfoTests PROPERTIES
    LABELS "hardware;cpu;unit"
    TIMEOUT 30
)

set_tests_properties(MemoryInfoTests PROPERTIES
    LABELS "hardware;memory;unit"
    TIMEOUT 30
)

set_tests_properties(DiskInfoTests PROPERTIES
    LABELS "hardware;disk;unit"
    TIMEOUT 60
)

set_tests_properties(GPUInfoTests PROPERTIES
    LABELS "hardware;gpu;unit"
    TIMEOUT 30
)

set_tests_properties(BatteryInfoTests PROPERTIES
    LABELS "hardware;battery;unit"
    TIMEOUT 30
)

set_tests_properties(OSInfoTests PROPERTIES
    LABELS "system;os;unit"
    TIMEOUT 30
)

set_tests_properties(BIOSInfoTests PROPERTIES
    LABELS "hardware;bios;security;unit"
    TIMEOUT 30
)

set_tests_properties(LocaleInfoTests PROPERTIES
    LABELS "system;locale;unit"
    TIMEOUT 15
)

set_tests_properties(SerialNumberTests PROPERTIES
    LABELS "hardware;serial;unit"
    TIMEOUT 30
)

set_tests_properties(VirtualEnvTests PROPERTIES
    LABELS "system;virtual;unit"
    TIMEOUT 30
)

set_tests_properties(WindowManagerTests PROPERTIES
    LABELS "system;wm;unit"
    TIMEOUT 30
)

set_tests_properties(NetworkInfoTests PROPERTIES
    LABELS "network;wifi;unit"
    TIMEOUT 60
)

set_tests_properties(SysInfoPrinterTests PROPERTIES
    LABELS "utility;printer;unit"
    TIMEOUT 30
)

set_tests_properties(IntegrationTests PROPERTIES
    LABELS "integration;full"
    TIMEOUT 120
)

# Custom test targets
add_custom_target(run_sysinfo_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running all system information tests"
)

add_custom_target(run_hardware_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="CPUInfoTest.*:CpuTest.*:MemoryInfoTest.*:MemoryTest.*:DiskInfoTest.*:DiskTest.*:GPUInfoTest.*:GpuTest.*:BatteryInfoTest.*:BatteryTest.*:BiosTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running hardware information tests"
)

add_custom_target(run_system_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="OSInfoTest.*:OsTest.*:LocaleTest.*:SnTest.*:VirtualTest.*:WmTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running system information tests"
)

add_custom_target(run_network_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="NetworkInfoTest.*:WifiTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running network information tests"
)

add_custom_target(run_utility_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="SysInfoPrinterTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running utility tests"
)

# Test category targets for better organization
add_custom_target(run_unit_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*Test.*:-*IntegrationTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running unit tests only"
)

add_custom_target(run_integration_tests_only
    COMMAND ${PROJECT_NAME} --gtest_filter="*IntegrationTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running integration tests only"
)

add_custom_target(run_performance_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*Performance*:*Monitoring*:*Load*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running performance-related tests"
)

add_custom_target(run_security_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*Security*:*Secure*:BiosTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running security-related tests"
)

add_custom_target(run_quick_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*Basic*:*Info*:-*Performance*:-*Monitoring*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running quick tests (excluding performance tests)"
)

# Code coverage (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
    target_link_options(${PROJECT_NAME} PRIVATE --coverage)

    add_custom_target(sysinfo_coverage
        COMMAND lcov --directory . --capture --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml -o coverage_html coverage.info
        DEPENDS run_sysinfo_tests
        COMMENT "Generating code coverage report for system information"
    )
endif()
