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

# Test executable
add_executable(${PROJECT_NAME}
    test_sysinfo.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable testing
enable_testing()

# Add test cases
add_test(NAME CPUInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="CPUInfoTest.*")
add_test(NAME MemoryInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="MemoryInfoTest.*")
add_test(NAME DiskInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="DiskInfoTest.*")
add_test(NAME GPUInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="GPUInfoTest.*")
add_test(NAME BatteryInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="BatteryInfoTest.*")
add_test(NAME OSInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="OSInfoTest.*")
add_test(NAME NetworkInfoTests COMMAND ${PROJECT_NAME} --gtest_filter="NetworkInfoTest.*")
add_test(NAME SysInfoPrinterTests COMMAND ${PROJECT_NAME} --gtest_filter="SysInfoPrinterTest.*")
add_test(NAME IntegrationTests COMMAND ${PROJECT_NAME} --gtest_filter="SysInfoIntegrationTest.*")

# Custom test targets
add_custom_target(run_sysinfo_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running all system information tests"
)

add_custom_target(run_hardware_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="CPUInfoTest.*:MemoryInfoTest.*:DiskInfoTest.*:GPUInfoTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running hardware information tests"
)

add_custom_target(run_system_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="OSInfoTest.*:NetworkInfoTest.*:BatteryInfoTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running system information tests"
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
