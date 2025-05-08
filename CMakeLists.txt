# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.

# Description: The foundational library for all elemental astro projects
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)

# Include Git version function before initializing project
include(cmake/GitVersion.cmake)

# Initialize project with default version
project(Atom LANGUAGES C CXX VERSION 0.1.0)

# Configure version from Git
configure_version_from_git(
    OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/atom_version.h"
    VERSION_VARIABLE ATOM_VERSION
    PREFIX "ATOM"
)

# Update project version with Git version
if(DEFINED ATOM_VERSION)
    set(PROJECT_VERSION ${ATOM_VERSION})
    message(STATUS "Using Git-derived version: ${PROJECT_VERSION}")
endif()

# Configure vcpkg support
option(USE_VCPKG "Use vcpkg package manager" ON)
set(VCPKG_INSTALL_OPTIONS "--no-print-usage" CACHE STRING "Additional vcpkg install options")

# First, detect MSYS2 environment
if(DEFINED ENV{MSYSTEM})
    message(STATUS "MSYS2 environment detected: $ENV{MSYSTEM}")
    set(MSYS2_ENV TRUE)
    
    # Automatically enable vcpkg option
    set(USE_VCPKG ON)
    
    # Add MSYS2 specific compile definitions
    add_compile_definitions(MSYS2_BUILD)
endif()

# Explicitly define possible vcpkg paths
if(DEFINED ENV{VCPKG_ROOT})
    set(POTENTIAL_VCPKG_PATH "$ENV{VCPKG_ROOT}")
    message(STATUS "Found vcpkg from VCPKG_ROOT environment variable: ${POTENTIAL_VCPKG_PATH}")
else()
    # Check common paths
    if(MSYS2_ENV)
        if(EXISTS "$ENV{USERPROFILE}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "$ENV{USERPROFILE}/vcpkg")
        elseif(EXISTS "$ENV{HOME}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "$ENV{HOME}/vcpkg")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
        elseif(EXISTS "/mingw64/vcpkg")
            set(POTENTIAL_VCPKG_PATH "/mingw64/vcpkg")
        endif()
    elseif(WIN32)
        if(EXISTS "C:/vcpkg")
            set(POTENTIAL_VCPKG_PATH "C:/vcpkg")
        elseif(EXISTS "$ENV{USERPROFILE}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "$ENV{USERPROFILE}/vcpkg")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
        endif()
    else()
        if(EXISTS "/usr/local/vcpkg")
            set(POTENTIAL_VCPKG_PATH "/usr/local/vcpkg")
        elseif(EXISTS "$ENV{HOME}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "$ENV{HOME}/vcpkg")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
            set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
        endif()
    endif()
endif()

# Check and set vcpkg toolchain file
if(USE_VCPKG AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED POTENTIAL_VCPKG_PATH AND EXISTS "${POTENTIAL_VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${POTENTIAL_VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
        message(STATUS "Set vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
        
        # Set VCPKG_ROOT to ensure vcpkg works correctly
        if(NOT DEFINED ENV{VCPKG_ROOT})
            set(ENV{VCPKG_ROOT} "${POTENTIAL_VCPKG_PATH}")
            message(STATUS "Set VCPKG_ROOT environment variable: ${POTENTIAL_VCPKG_PATH}")
        endif()
    else()
        message(WARNING "USE_VCPKG is enabled but vcpkg toolchain file was not found.")
        message(STATUS "Please install vcpkg or set the VCPKG_ROOT environment variable to point to the vcpkg installation directory.")
        message(STATUS "You can install vcpkg using the following commands:")
        if(WIN32)
            message(STATUS "   git clone https://github.com/microsoft/vcpkg.git")
            message(STATUS "   .\\vcpkg\\bootstrap-vcpkg.bat")
        else()
            message(STATUS "   git clone https://github.com/microsoft/vcpkg.git")
            message(STATUS "   ./vcpkg/bootstrap-vcpkg.sh")
        endif()
        message(FATAL_ERROR "Missing vcpkg, build aborted. Please install vcpkg and try again.")
    endif()
endif()

# MSYS2/MinGW environment support
if(MINGW OR MSYS2_ENV)
    message(STATUS "MinGW/MSYS2 environment detected")
    
    # Add special settings for MSYS2/MinGW environment
    add_compile_definitions(MINGW_BUILD)
    
    # Set MSYS2 path conversion
    if(USE_VCPKG)
        # Path handling for MinGW with vcpkg integration
        if(NOT DEFINED VCPKG_TARGET_TRIPLET)
            # Set default triplet for MinGW based on actual architecture
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                # For MSYS2/MinGW environment, first try to use x64-mingw-dynamic, if this triplet is not found, use x64-mingw-static
                if(EXISTS "${POTENTIAL_VCPKG_PATH}/triplets/community/x64-mingw-dynamic.cmake")
                    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
                elseif(EXISTS "${POTENTIAL_VCPKG_PATH}/triplets/community/x64-mingw-static.cmake")
                    set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "")
                else()
                    # If community triplet does not exist, try using x64-mingw
                    if(EXISTS "${POTENTIAL_VCPKG_PATH}/triplets/x64-mingw.cmake")
                        set(VCPKG_TARGET_TRIPLET "x64-mingw" CACHE STRING "")
                    else()
                        # Finally, try to automatically create the required triplet
                        message(STATUS "Creating x64-mingw-dynamic triplet")
                        file(MAKE_DIRECTORY "${POTENTIAL_VCPKG_PATH}/triplets/community")
                        file(WRITE "${POTENTIAL_VCPKG_PATH}/triplets/community/x64-mingw-dynamic.cmake" 
                            "set(VCPKG_TARGET_ARCHITECTURE x64)\nset(VCPKG_CRT_LINKAGE dynamic)\nset(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_CMAKE_SYSTEM_NAME MinGW)")
                        set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "")
                    endif()
                endif()
            else()
                # Perform similar checks for 32-bit builds
                if(EXISTS "${POTENTIAL_VCPKG_PATH}/triplets/community/x86-mingw-dynamic.cmake")
                    set(VCPKG_TARGET_TRIPLET "x86-mingw-dynamic" CACHE STRING "")
                elseif(EXISTS "${POTENTIAL_VCPKG_PATH}/triplets/community/x86-mingw-static.cmake")
                    set(VCPKG_TARGET_TRIPLET "x86-mingw-static" CACHE STRING "")
                else()
                    # If community triplet does not exist, try using x86-mingw
                    if(EXISTS("${POTENTIAL_VCPKG_PATH}/triplets/x86-mingw.cmake"))
                        set(VCPKG_TARGET_TRIPLET "x86-mingw" CACHE STRING "")
                    else()
                        # Finally, try to automatically create the required triplet
                        message(STATUS "Creating x86-mingw-dynamic triplet")
                        file(MAKE_DIRECTORY "${POTENTIAL_VCPKG_PATH}/triplets/community")
                        file(WRITE "${POTENTIAL_VCPKG_PATH}/triplets/community/x86-mingw-dynamic.cmake" 
                            "set(VCPKG_TARGET_ARCHITECTURE x86)\nset(VCPKG_CRT_LINKAGE dynamic)\nset(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_CMAKE_SYSTEM_NAME MinGW)")
                        set(VCPKG_TARGET_TRIPLET "x86-mingw-dynamic" CACHE STRING "")
                    endif()
                endif()
            endif()
            message(STATUS "Set default VCPKG_TARGET_TRIPLET for MinGW: ${VCPKG_TARGET_TRIPLET}")
        endif()
    endif()
endif()

# Ensure vcpkg dependencies are installed
if(USE_VCPKG AND EXISTS "${POTENTIAL_VCPKG_PATH}/vcpkg${CMAKE_EXECUTABLE_SUFFIX}")
    message(STATUS "Checking vcpkg dependencies...")
    
    # Install vcpkg dependencies before build
    if(MSYS2_ENV AND WIN32)
        execute_process(
            COMMAND cmd.exe /c "${POTENTIAL_VCPKG_PATH}/vcpkg.exe" install --triplet=${VCPKG_TARGET_TRIPLET} ${VCPKG_INSTALL_OPTIONS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE VCPKG_INSTALL_RESULT
        )
    else()
        execute_process(
            COMMAND "${POTENTIAL_VCPKG_PATH}/vcpkg${CMAKE_EXECUTABLE_SUFFIX}" install --triplet=${VCPKG_TARGET_TRIPLET} ${VCPKG_INSTALL_OPTIONS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE VCPKG_INSTALL_RESULT
        )
    endif()
    
    if(NOT VCPKG_INSTALL_RESULT EQUAL 0)
        message(WARNING "vcpkg dependency installation failed, return code: ${VCPKG_INSTALL_RESULT}")
        message(STATUS "You can try to install dependencies manually:")
        if(MSYS2_ENV AND WIN32)
            message(STATUS "   cmd.exe /c \"${POTENTIAL_VCPKG_PATH}/vcpkg.exe\" install --triplet=${VCPKG_TARGET_TRIPLET}")
        else()
            message(STATUS "   \"${POTENTIAL_VCPKG_PATH}/vcpkg${CMAKE_EXECUTABLE_SUFFIX}\" install --triplet=${VCPKG_TARGET_TRIPLET}")
        endif()
    else()
        message(STATUS "vcpkg dependencies installed successfully")
    endif()
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add custom target for additional CMake files
add_custom_target(AtomCmakeAdditionalFiles
    SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_options.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake
)

# 将版本信息作为定义传递给所有目标
add_compile_definitions(
    ATOM_VERSION="${PROJECT_VERSION}"
    ATOM_VERSION_STRING="${PROJECT_VERSION}"
)

# Ensure the generated version header is included in builds
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Global build options
option(ATOM_BUILD_EXAMPLES "Build examples" OFF)
option(ATOM_BUILD_TESTS "Build tests" OFF)

# Boost related options
option(ATOM_USE_BOOST "Enable Boost high-performance data structures" OFF)
option(ATOM_USE_BOOST_LOCKFREE "Enable Boost lock-free data structures" OFF)
option(ATOM_USE_BOOST_CONTAINER "Enable Boost container library" OFF)
option(ATOM_USE_BOOST_GRAPH "Enable Boost graph library" OFF)
option(ATOM_USE_BOOST_INTRUSIVE "Enable Boost intrusive containers" OFF)
option(ATOM_USE_PYBIND11 "Enable pybind11 support" ON)

# Module build options
option(ATOM_BUILD_ALL "Build all Atom modules" ON)
option(ATOM_BUILD_ALGORITHM "Build algorithm module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_ASYNC "Build async module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_COMPONENTS "Build components module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_CONNECTION "Build connection module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_ERROR "Build error module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_IO "Build IO module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_LOG "Build log module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_META "Build meta module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SEARCH "Build search module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SECRET "Build security module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SYSINFO "Build system info module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SYSTEM "Build system module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_UTILS "Build utils module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_WEB "Build Web module" ${ATOM_BUILD_ALL})

# Append module paths
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

# Include compiler options
include(cmake/compiler_options.cmake)

# Add module dependency configuration file
include(cmake/module_dependencies.cmake)

# Process module dependencies
include(cmake/ScanModule.cmake)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extra)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(.)

# Find necessary packages
message(STATUS "Finding dependency packages...")

# Ensure only vcpkg-provided packages are used
# Set CMake preferred Config mode and disable Module mode to prioritize vcpkg packages
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
# Disable system path lookup, force use of vcpkg
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
set(CMAKE_FIND_USE_CMAKE_PATH ON)  # Allow use of CMAKE_PREFIX_PATH etc.

# Find dependency packages
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Readline REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${pybind11_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})

# If Boost support is enabled
if(ATOM_USE_BOOST)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(BOOST_COMPONENTS)
    
    if(ATOM_USE_BOOST_CONTAINER)
        list(APPEND BOOST_COMPONENTS container)
    endif()
    
    if(ATOM_USE_BOOST_LOCKFREE)
        list(APPEND BOOST_COMPONENTS atomic thread)
    endif()
    
    if(ATOM_USE_BOOST_GRAPH)
        list(APPEND BOOST_COMPONENTS graph)
    endif()
    
    if(ATOM_USE_BOOST_INTRUSIVE)
        # intrusive is header-only
    endif()
    
    find_package(Boost 1.74 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Found Boost: ${Boost_VERSION} (${Boost_INCLUDE_DIRS})")
endif()

# Generate a version information file for runtime access
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
    @ONLY
)

# Add subdirectories
add_subdirectory(extra)
add_subdirectory(atom)
add_subdirectory(python)
if(ATOM_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()
if(ATOM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Enable folder grouping in IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 打印最终确定的版本号
message(STATUS "Atom version: ${PROJECT_VERSION}")