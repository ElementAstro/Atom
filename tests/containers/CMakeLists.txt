# CMakeLists.txt for Atom-Containers Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Containers-Tests
# Description: Test Suite for Atom Container Library
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-containers-tests VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include parent directory for container headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Test executable
add_executable(${PROJECT_NAME}
    test_containers.cpp
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
add_test(NAME BoostContainersTests COMMAND ${PROJECT_NAME} --gtest_filter="BoostContainersTest.*")
add_test(NAME GraphTests COMMAND ${PROJECT_NAME} --gtest_filter="GraphTest.*")
add_test(NAME HighPerformanceTests COMMAND ${PROJECT_NAME} --gtest_filter="HighPerformanceTest.*")
add_test(NAME IntrusiveTests COMMAND ${PROJECT_NAME} --gtest_filter="IntrusiveTest.*")
add_test(NAME LockFreeTests COMMAND ${PROJECT_NAME} --gtest_filter="LockFreeTest.*")
add_test(NAME IntegrationTests COMMAND ${PROJECT_NAME} --gtest_filter="ContainerIntegrationTest.*")
add_test(NAME ErrorHandlingTests COMMAND ${PROJECT_NAME} --gtest_filter="ContainerErrorTest.*")

# Custom test targets
add_custom_target(run_container_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running all container tests"
)

add_custom_target(run_performance_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*PerformanceTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running performance tests"
)

add_custom_target(run_thread_safety_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*ThreadSafety*:*Concurrent*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running thread safety tests"
)

# Code coverage (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
    target_link_options(${PROJECT_NAME} PRIVATE --coverage)
    
    add_custom_target(container_coverage
        COMMAND lcov --directory . --capture --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml -o coverage_html coverage.info
        DEPENDS run_container_tests
        COMMENT "Generating code coverage report for containers"
    )
endif()
