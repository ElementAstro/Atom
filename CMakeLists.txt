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

option(USE_VCPKG "Use vcpkg package manager" OFF)
option(UPDATE_VCPKG_BASELINE "Update vcpkg baseline to latest" OFF)

if (USE_VCPKG)
    message(STATUS "Using vcpkg package manager.")
    include(cmake/VcpkgSetup.cmake)
else()
    message(STATUS "Not using vcpkg package manager.")
endif()
include(cmake/VersionConfig.cmake)

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
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

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

include(cmake/PlatformSpecifics.cmake) # Uses variables from VcpkgSetup.cmake

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
if(USE_VCPKG)
    # Ensure only vcpkg-provided packages are used
    # Set CMake preferred Config mode and disable Module mode to prioritize vcpkg packages
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
    # Disable system path lookup, force use of vcpkg
    set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
    set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
    set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
    set(CMAKE_FIND_USE_CMAKE_PATH ON)  # Allow use of CMAKE_PREFIX_PATH etc.
endif()

# Find dependency packages
find_package(Asio REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
find_package(Readline REQUIRED)
find_package(ZLIB REQUIRED)
if (LINUX)
    find_package(X11 REQUIRED)
    if(X11_FOUND)
        include_directories(${X11_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "X11 development files not found. Please install libx11-dev or equivalent.")
    endif()

    # Find libudev using pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UDEV REQUIRED libudev)
    if(UDEV_FOUND)
        include_directories(${UDEV_INCLUDE_DIRS})
     link_directories(${UDEV_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "libudev development files not found. Please install libudev-dev or equivalent.")
    endif()
endif()

# Max: In the WSL environment, directly using find_package may locate the curl library in Windows, leading to a series of errors
include(WSLDetection)
detect_wsl(IS_WSL)
if(IS_WSL)
    message(STATUS "Running in WSL environment")
    pkg_check_modules(CURL REQUIRED libcurl)
    if(CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIRS})
        link_directories(${CURL_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "curl development files not found. Please install libcurl-dev or equivalent.")
    endif()
else()
    message(STATUS "Not running in WSL environment")
    find_package(CURL REQUIRED)
    if(CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIRS})
        message(STATUS "Found CURL: ${CURL_VERSION} (${CURL_INCLUDE_DIRS})")
    else()
        message(FATAL_ERROR "curl development files not found. Please install libcurl-dev or equivalent.")
    endif()
endif()


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
