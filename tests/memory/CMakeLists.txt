cmake_minimum_required(VERSION 3.20)

project(atom_memory_async_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/../../)

# Define test sources for async tests only
set(ASYNC_TEST_SOURCES
    test_async_comprehensive.cpp
    test_async_integration.cpp
    test_async_performance.cpp
    test_async_stress.cpp
    ../../atom/error/exception.cpp
    ../../atom/error/stacktrace.cpp
)

# Create executable for async tests
add_executable(async_memory_tests ${ASYNC_TEST_SOURCES})

# Link libraries
target_link_libraries(async_memory_tests
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    fmt::fmt
    rt  # For shared memory on Linux
)

# Compiler flags
target_compile_options(async_memory_tests PRIVATE
    -Wall
    -Wextra
    -O2
    -g
)

# Register tests with CTest
add_test(NAME AsyncMemoryTests COMMAND async_memory_tests)

# Enable testing
enable_testing()
