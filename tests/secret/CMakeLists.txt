# CMakeLists.txt for Atom-Secret Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Secret-Tests
# Description: Test Suite for Atom Secret Management Library
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-secret-tests VERSION 1.0.0 LANGUAGES C CXX)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include parent directory for secret headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Test executable
add_executable(${PROJECT_NAME}
    test_secret.cpp
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
add_test(NAME EncryptionTests COMMAND ${PROJECT_NAME} --gtest_filter="EncryptionTest.*")
add_test(NAME PasswordEntryTests COMMAND ${PROJECT_NAME} --gtest_filter="PasswordEntryTest.*")
add_test(NAME SecureStorageTests COMMAND ${PROJECT_NAME} --gtest_filter="SecureStorageTest.*")
add_test(NAME ResultTests COMMAND ${PROJECT_NAME} --gtest_filter="ResultTest.*")
add_test(NAME CommonUtilitiesTests COMMAND ${PROJECT_NAME} --gtest_filter="CommonUtilitiesTest.*")
add_test(NAME IntegrationTests COMMAND ${PROJECT_NAME} --gtest_filter="SecretIntegrationTest.*")
add_test(NAME SecurityTests COMMAND ${PROJECT_NAME} --gtest_filter="SecurityTest.*")

# Custom test targets
add_custom_target(run_secret_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running all secret management tests"
)

add_custom_target(run_encryption_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="EncryptionTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running encryption tests"
)

add_custom_target(run_security_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="SecurityTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running security tests"
)

# Code coverage (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
    target_link_options(${PROJECT_NAME} PRIVATE --coverage)

    add_custom_target(secret_coverage
        COMMAND lcov --directory . --capture --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml -o coverage_html coverage.info
        DEPENDS run_secret_tests
        COMMENT "Generating code coverage report for secret management"
    )
endif()
