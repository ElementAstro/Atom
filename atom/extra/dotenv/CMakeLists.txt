cmake_minimum_required(VERSION 3.20)
project(dotenv-cpp VERSION 1.0.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Include directories
include_directories(include)

# Source files
set(SOURCES
    dotenv.cpp
    parser.cpp
    validator.cpp
    loader.cpp
)

# Create library
add_library(dotenv-cpp ${SOURCES})

# Header files for installation
set(HEADERS
    dotenv.hpp
    parser.hpp
    validator.hpp
    loader.hpp
    exceptions.hpp
)

# Set target properties
target_include_directories(dotenv-cpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Find required packages
find_package(Threads REQUIRED)
target_link_libraries(dotenv-cpp Threads::Threads)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(dotenv-cpp ws2_32)
endif()

# Testing
enable_testing()
add_subdirectory(tests)

# Examples
add_subdirectory(examples)

# Installation
install(TARGETS dotenv-cpp
    EXPORT dotenv-cpp-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${HEADERS} DESTINATION include/dotenv)

# Export configuration
install(EXPORT dotenv-cpp-targets
    FILE dotenv-cpp-config.cmake
    DESTINATION lib/cmake/dotenv-cpp
)