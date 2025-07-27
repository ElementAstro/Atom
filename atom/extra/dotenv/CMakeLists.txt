cmake_minimum_required(VERSION 3.20)
project(dotenv-cpp VERSION 1.0.0 LANGUAGES CXX)

# C++23 standard for cutting-edge features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Advanced compiler flags for performance and concurrency
if(MSVC)
    add_compile_options(/W4 /WX /O2 /Oi /Ot /GL /arch:AVX2)
    add_compile_definitions(_WIN32_WINNT=0x0A00)  # Windows 10+
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3 -march=native
                       -mtune=native -flto -ffast-math -funroll-loops
                       -fomit-frame-pointer -finline-functions)
    # Enable advanced concurrency features
    add_compile_options(-pthread -fcoroutines)
endif()

# Enable advanced concurrency and performance features
add_compile_definitions(
    DOTENV_ENABLE_ADVANCED_CONCURRENCY=1
    DOTENV_ENABLE_LOCK_FREE=1
    DOTENV_ENABLE_PERFORMANCE_MONITORING=1
    ATOM_HAS_SPDLOG=1
)

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
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# Link libraries
target_link_libraries(dotenv-cpp
    PUBLIC
        Threads::Threads
        spdlog::spdlog
        fmt::fmt
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(dotenv-cpp ws2_32)
endif()

# Testing
enable_testing()

# Add concurrency test executable
add_executable(test_concurrency test_concurrency.cpp)
target_link_libraries(test_concurrency dotenv-cpp)
add_test(NAME ConcurrencyTest COMMAND test_concurrency)

# Add advanced example executable
add_executable(advanced_example advanced_example.cpp)
target_link_libraries(advanced_example dotenv-cpp)

# Add performance benchmark
add_executable(benchmark_dotenv benchmark_dotenv.cpp)
target_link_libraries(benchmark_dotenv dotenv-cpp)

# Traditional tests (if they exist)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

# Examples (if they exist)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    add_subdirectory(examples)
endif()

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
