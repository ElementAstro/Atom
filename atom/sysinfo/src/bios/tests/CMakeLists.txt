# BIOS Module Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(atom_sysinfo_bios_tests VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Include parent directory for BIOS module
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Define test sources
set(TEST_SOURCES
    test_bios_basic.cpp
)

# Create test executable
add_executable(test_bios_basic ${TEST_SOURCES})

# Link with BIOS module
target_link_libraries(test_bios_basic
    PRIVATE
        atom_sysinfo_bios
        spdlog::spdlog
)

# Set target properties
set_target_properties(test_bios_basic PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Compiler-specific options
if(MSVC)
    target_compile_options(test_bios_basic PRIVATE /W4)
else()
    target_compile_options(test_bios_basic PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add test to CTest
enable_testing()
add_test(NAME BiosBasicTest COMMAND test_bios_basic)

# Set test properties
set_tests_properties(BiosBasicTest PROPERTIES
    TIMEOUT 30
    LABELS "bios;basic"
)
