# CMakeLists.txt for atom-meta - OPTIMIZED VERSION
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: atom-meta
# Description: High-performance meta programming library for C++ with optimizations
# Author: Max Qian
# License: GPL3
# Optimized: 2025-01-22 - Performance optimizations and feature enhancements

cmake_minimum_required(VERSION 3.20)
project(
  atom-meta
  VERSION 2.0.0  # Bumped version for optimized release
  LANGUAGES C CXX)

# C++ Standard Requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for performance
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(${PROJECT_NAME}_object PRIVATE /O2 /Ob2 /DNDEBUG)
    else()
        target_compile_options(${PROJECT_NAME}_object PRIVATE -O3 -march=native -DNDEBUG)
    endif()
endif()

# Sources (implementation files)
set(SOURCES
    global_ptr.cpp
    # Add other .cpp files here if needed
)

# Headers (all optimized header files)
set(HEADERS
    any.hpp              # BoxedValue system (optimized)
    global_ptr.hpp       # GlobalSharedPtrManager (optimized)
    type_info.hpp        # TypeInfo system (optimized)
    refl.hpp            # Reflection system (optimized)
    refl_json.hpp       # JSON reflection (enhanced)
    refl_yaml.hpp       # YAML reflection
    invoke.hpp          # Function invocation (optimized)
    concept.hpp         # Concepts and traits (optimized)
    # Add other header files
)

# Dependencies
set(LIBS)

# Optional dependencies
find_package(Boost QUIET)
if(Boost_FOUND)
    list(APPEND LIBS Boost::boost)
    add_compile_definitions(ATOM_USE_BOOST)
endif()

find_package(yaml-cpp QUIET)
if(yaml-cpp_FOUND)
    list(APPEND LIBS yaml-cpp)
    add_compile_definitions(ATOM_USE_YAML_CPP)
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Compiler-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(${PROJECT_NAME}_object PRIVATE /O2 /Ob2 /DNDEBUG)
    else()
        target_compile_options(${PROJECT_NAME}_object PRIVATE -O3 -march=native -DNDEBUG)
    endif()
endif()

# Enable all warnings for better code quality
if(MSVC)
    target_compile_options(${PROJECT_NAME}_object PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME}_object PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Testing configuration
option(ATOM_META_BUILD_TESTS "Build atom-meta tests" ON)
if(ATOM_META_BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        add_subdirectory(tests)
    else()
        message(STATUS "GTest not found or tests directory missing, tests will not be built")
    endif()
endif()

# Benchmarking configuration
option(ATOM_META_BUILD_BENCHMARKS "Build atom-meta benchmarks" OFF)
if(ATOM_META_BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(benchmark_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
        add_subdirectory(benchmarks)
    else()
        message(STATUS "Google Benchmark not found or benchmarks directory missing, benchmarks will not be built")
    endif()
endif()

# Documentation configuration
option(ATOM_META_BUILD_DOCS "Build atom-meta documentation" OFF)
if(ATOM_META_BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(Doxygen_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
        set(DOXYGEN_PROJECT_NAME "Atom Meta Library")
        set(DOXYGEN_PROJECT_BRIEF "High-performance meta programming library for C++")
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_XML YES)

        doxygen_add_docs(
            ${PROJECT_NAME}_docs
            ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating API documentation with Doxygen"
        )
    else()
        message(STATUS "Doxygen not found, documentation will not be built")
    endif()
endif()

# Install rules
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/meta)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Only configure package config if template exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()

# Print configuration summary
message(STATUS "=== Atom Meta Library Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${ATOM_META_BUILD_TESTS}")
message(STATUS "Build benchmarks: ${ATOM_META_BUILD_BENCHMARKS}")
message(STATUS "Build documentation: ${ATOM_META_BUILD_DOCS}")
if(Boost_FOUND)
    message(STATUS "Boost support: ENABLED")
else()
    message(STATUS "Boost support: DISABLED")
endif()
message(STATUS "========================================")
