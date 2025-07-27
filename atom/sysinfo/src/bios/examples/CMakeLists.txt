# BIOS Module Examples CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(atom_sysinfo_bios_examples VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Include parent directory for BIOS module
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Define example sources
set(EXAMPLE_SOURCES
    bios_example.cpp
)

# Create example executable
add_executable(bios_example ${EXAMPLE_SOURCES})

# Link with BIOS module
target_link_libraries(bios_example
    PRIVATE
        atom_sysinfo_bios
        spdlog::spdlog
)

# Set target properties
set_target_properties(bios_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Compiler-specific options
if(MSVC)
    target_compile_options(bios_example PRIVATE /W4)
else()
    target_compile_options(bios_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install example
install(TARGETS bios_example
    RUNTIME DESTINATION bin/examples
)
