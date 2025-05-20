# CMakeLists.txt for Atom Project
# This project is licensed under the terms of the GPL3 license.
#
# Description: The foundational library for all elemental astro projects
# Author: Max Qian
# License: GPL3

# Require minimum CMake version
cmake_minimum_required(VERSION 3.21)

# Include Git version function before initializing project
include(cmake/GitVersion.cmake)

# Initialize project with default version
project(
  Atom
  LANGUAGES C CXX
  VERSION 0.1.0
  DESCRIPTION "Foundational library for astronomical software"
  HOMEPAGE_URL "https://github.com/ElementAstro/Atom"
)

# =============================================================================
# Package Management Configuration
# =============================================================================

option(USE_VCPKG "Use vcpkg package manager" OFF)
option(UPDATE_VCPKG_BASELINE "Update vcpkg baseline to latest" OFF)

if(USE_VCPKG)
  message(STATUS "Using vcpkg package manager.")
  include(cmake/VcpkgSetup.cmake)
else()
  message(STATUS "Not using vcpkg package manager.")
endif()

# Configure version information
include(cmake/VersionConfig.cmake)

# =============================================================================
# C++ Standard Setting
# =============================================================================

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add custom target for additional CMake files
add_custom_target(
  AtomCmakeAdditionalFiles
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_options.cmake
          ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake
)

# Pass version information as definitions to all targets
add_compile_definitions(ATOM_VERSION="${PROJECT_VERSION}"
                        ATOM_VERSION_STRING="${PROJECT_VERSION}")

# Ensure the generated version header is included in builds
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# =============================================================================
# Build Options
# =============================================================================

# Global build options
option(ATOM_BUILD_EXAMPLES "Build examples" ON)
option(ATOM_BUILD_EXAMPLES_SELECTIVE "Enable selective building of example modules" OFF)
option(ATOM_BUILD_TESTS "Build tests" ON)
option(ATOM_BUILD_TESTS_SELECTIVE "Enable selective building of test modules" OFF)
option(ATOM_BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(ATOM_BUILD_DOCS "Build documentation" OFF)

# Boost related options
option(ATOM_USE_BOOST "Enable Boost high-performance data structures" OFF)
option(ATOM_USE_BOOST_LOCKFREE "Enable Boost lock-free data structures" OFF)
option(ATOM_USE_BOOST_CONTAINER "Enable Boost container library" OFF)
option(ATOM_USE_BOOST_GRAPH "Enable Boost graph library" OFF)
option(ATOM_USE_BOOST_INTRUSIVE "Enable Boost intrusive containers" OFF)
option(ATOM_USE_PYBIND11 "Enable pybind11 support" ${ATOM_BUILD_PYTHON_BINDINGS})

# Module build options
option(ATOM_BUILD_ALL "Build all Atom modules" ON)
option(ATOM_BUILD_ALGORITHM "Build algorithm module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_ASYNC "Build async module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_COMPONENTS "Build components module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_CONNECTION "Build connection module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_CONTAINERS "Build containers module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_ERROR "Build error module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_IMAGE "Build image module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_IO "Build IO module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_LOG "Build log module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_MEMORY "Build memory module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_META "Build meta module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SEARCH "Build search module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SECRET "Build security module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SERIAL "Build serial module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SYSINFO "Build system info module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_SYSTEM "Build system module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_TYPE "Build type module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_UTILS "Build utils module" ${ATOM_BUILD_ALL})
option(ATOM_BUILD_WEB "Build Web module" ${ATOM_BUILD_ALL})

# =============================================================================
# Platform and Configuration Setup
# =============================================================================

# Setup platform-specific configurations
include(cmake/PlatformSpecifics.cmake)

# Append module paths
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

# Include compiler options
include(cmake/compiler_options.cmake)

# Add module dependency configuration file
include(cmake/module_dependencies.cmake)

# Include example build options
include(cmake/ExamplesBuildOptions.cmake)

# Include test build options
include(cmake/TestsBuildOptions.cmake)

# Process module dependencies
include(cmake/ScanModule.cmake)

# =============================================================================
# Include Directories
# =============================================================================

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extra)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(.)

# =============================================================================
# Find Necessary Packages
# =============================================================================

message(STATUS "Finding dependency packages...")
if(USE_VCPKG)
  # Ensure only vcpkg-provided packages are used Set CMake preferred Config mode
  # and disable Module mode to prioritize vcpkg packages
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
  # Disable system path lookup, force use of vcpkg
  set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
  set(CMAKE_FIND_USE_CMAKE_PATH ON) # Allow use of CMAKE_PREFIX_PATH etc.
endif()

# Find dependency packages
find_package(Asio REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
# Find Python and pybind11 only if building Python bindings
if(ATOM_BUILD_PYTHON_BINDINGS)
  find_package(
    Python
    COMPONENTS Interpreter Development
    REQUIRED)
  find_package(pybind11 CONFIG REQUIRED)
endif()
find_package(Readline REQUIRED)
find_package(ZLIB REQUIRED)
if(LINUX)
  find_package(X11 REQUIRED)
  if(X11_FOUND)
    include_directories(${X11_INCLUDE_DIR})
  else()
    message(
      FATAL_ERROR
        "X11 development files not found. Please install libx11-dev or equivalent."
    )
  endif()

  # Find libudev using pkg-config
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(UDEV REQUIRED libudev)
  if(UDEV_FOUND)
    include_directories(${UDEV_INCLUDE_DIRS})
    link_directories(${UDEV_LIBRARY_DIRS})
  else()
    message(
      FATAL_ERROR
        "libudev development files not found. Please install libudev-dev or equivalent."
    )
  endif()
endif()

# Max: In the WSL environment, directly using find_package may locate the curl
# library in Windows, leading to a series of errors
include(WSLDetection)
detect_wsl(IS_WSL)
if(IS_WSL)
  message(STATUS "Running in WSL environment")
  pkg_check_modules(CURL REQUIRED libcurl)
  if(CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    link_directories(${CURL_LIBRARY_DIRS})
  else()
    message(
      FATAL_ERROR
        "curl development files not found. Please install libcurl-dev or equivalent."
    )
  endif()
else()
  message(STATUS "Not running in WSL environment")
  find_package(CURL REQUIRED)
  if(CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    message(STATUS "Found CURL: ${CURL_VERSION} (${CURL_INCLUDE_DIRS})")
  else()
    message(
      FATAL_ERROR
        "curl development files not found. Please install libcurl-dev or equivalent."
    )
  endif()
endif()

# Include Python and pybind11 directories only if building Python bindings
if(ATOM_BUILD_PYTHON_BINDINGS)
  include_directories(${pybind11_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})
endif()

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
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h @ONLY)

# =============================================================================
# Subdirectories to Build
# =============================================================================

# Add core library subdirectory
add_subdirectory(atom)

# Add examples if enabled
if(ATOM_BUILD_EXAMPLES)
  add_subdirectory(example)
endif()

# Add extra libraries
add_subdirectory(extra)

# Add Python bindings if enabled
if(ATOM_BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()

# Add tests if enabled
if(ATOM_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# =============================================================================
# Documentation Setup
# =============================================================================

if(ATOM_BUILD_DOCS)
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found. Documentation generation enabled.")
    # Configure Doxygen options here if needed
  else()
    message(WARNING "Doxygen not found. Documentation will not be generated.")
    set(ATOM_BUILD_DOCS OFF)
  endif()
endif()

# =============================================================================
# Installation Configuration
# =============================================================================

include(GNUInstallDirs)

# Install headers
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/atom/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
  PATTERN "**/internal" EXCLUDE
  PATTERN "**/tests" EXCLUDE
  PATTERN "**/example" EXCLUDE
)

# Install version headers
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/atom_version.h
        ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom
)

message(STATUS "Atom configured successfully")

# Enable folder grouping in IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
