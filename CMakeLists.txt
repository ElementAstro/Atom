# CMakeLists.txt for Atom Project
# Licensed under GPL3
# Author: Max Qian

cmake_minimum_required(VERSION 3.21)
project(
  Atom
  LANGUAGES C CXX
  VERSION 0.1.0
  DESCRIPTION "Foundational library for astronomical software"
  HOMEPAGE_URL "https://github.com/ElementAstro/Atom"
)

# -----------------------------------------------------------------------------
# Include CMake Modules
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/GitVersion.cmake)
include(cmake/VersionConfig.cmake)
include(cmake/PlatformSpecifics.cmake)
include(cmake/compiler_options.cmake)
include(cmake/module_dependencies.cmake)
include(cmake/ExamplesBuildOptions.cmake)
include(cmake/TestsBuildOptions.cmake)
include(cmake/ScanModule.cmake)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(USE_VCPKG "Use vcpkg package manager" OFF)
option(UPDATE_VCPKG_BASELINE "Update vcpkg baseline to latest" OFF)
option(ATOM_BUILD_EXAMPLES "Build examples" ON)
option(ATOM_BUILD_EXAMPLES_SELECTIVE "Enable selective building of example modules" OFF)
option(ATOM_BUILD_TESTS "Build tests" ON)
option(ATOM_BUILD_TESTS_SELECTIVE "Enable selective building of test modules" OFF)
option(ATOM_BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(ATOM_BUILD_DOCS "Build documentation" OFF)
option(ATOM_USE_BOOST "Enable Boost high-performance data structures" OFF)
option(ATOM_USE_BOOST_LOCKFREE "Enable Boost lock-free data structures" OFF)
option(ATOM_USE_BOOST_CONTAINER "Enable Boost container library" OFF)
option(ATOM_USE_BOOST_GRAPH "Enable Boost graph library" OFF)
option(ATOM_USE_BOOST_INTRUSIVE "Enable Boost intrusive containers" OFF)
option(ATOM_USE_PYBIND11 "Enable pybind11 support" ${ATOM_BUILD_PYTHON_BINDINGS})
option(ATOM_BUILD_ALL "Build all Atom modules" ON)

# Module build options
foreach(MODULE
    ALGORITHM ASYNC COMPONENTS CONNECTION CONTAINERS ERROR IMAGE IO LOG MEMORY
    META SEARCH SECRET SERIAL SYSINFO SYSTEM TYPE UTILS WEB)
  option(ATOM_BUILD_${MODULE} "Build ${MODULE} module" ${ATOM_BUILD_ALL})
endforeach()

# -----------------------------------------------------------------------------
# C++ Standard
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Version Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(
  ATOM_VERSION="${PROJECT_VERSION}"
  ATOM_VERSION_STRING="${PROJECT_VERSION}"
)

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/extra
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  .
)

# -----------------------------------------------------------------------------
# Custom Targets
# -----------------------------------------------------------------------------
add_custom_target(
  AtomCmakeAdditionalFiles
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compiler_options.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake
)

# -----------------------------------------------------------------------------
# Package Management
# -----------------------------------------------------------------------------
if(USE_VCPKG)
  message(STATUS "Using vcpkg package manager.")
  include(cmake/VcpkgSetup.cmake)
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
  set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
  set(CMAKE_FIND_USE_CMAKE_PATH ON)
else()
  message(STATUS "Not using vcpkg package manager.")
endif()

# -----------------------------------------------------------------------------
# Dependency Packages
# -----------------------------------------------------------------------------
message(STATUS "Finding dependency packages...")

find_package(Asio REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Readline REQUIRED)
find_package(ZLIB REQUIRED)

# Python & pybind11
if(ATOM_BUILD_PYTHON_BINDINGS)
  find_package(Python COMPONENTS Interpreter Development REQUIRED)
  find_package(pybind11 CONFIG REQUIRED)
  include_directories(${pybind11_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})
endif()

# Linux/WSL/Windows platform-specific dependencies
if(LINUX)
  find_package(X11 REQUIRED)
  if(X11_FOUND)
    include_directories(${X11_INCLUDE_DIR})
  else()
    message(FATAL_ERROR "X11 development files not found. Please install libx11-dev or equivalent.")
  endif()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(UDEV REQUIRED libudev)
  if(UDEV_FOUND)
    include_directories(${UDEV_INCLUDE_DIRS})
    link_directories(${UDEV_LIBRARY_DIRS})
  else()
    message(FATAL_ERROR "libudev development files not found. Please install libudev-dev or equivalent.")
  endif()
endif()

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

# Boost
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
  # intrusive is header-only
  find_package(Boost 1.74 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
  include_directories(${Boost_INCLUDE_DIRS})
  message(STATUS "Found Boost: ${Boost_VERSION} (${Boost_INCLUDE_DIRS})")
endif()

# -----------------------------------------------------------------------------
# Version Info Header
# -----------------------------------------------------------------------------
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
  @ONLY
)

# -----------------------------------------------------------------------------
# Ninja Generator Support
# -----------------------------------------------------------------------------
if(CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_GENERATOR MATCHES "Ninja")
  message(STATUS "Ninja generator detected. Enabling Ninja-specific optimizations.")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable compile_commands.json for Ninja" FORCE)
endif()

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(atom)
if(ATOM_BUILD_EXAMPLES)
  add_subdirectory(example)
endif()
add_subdirectory(extra)
if(ATOM_BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()
if(ATOM_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Documentation
# -----------------------------------------------------------------------------
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

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/atom/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
  PATTERN "**/internal" EXCLUDE
  PATTERN "**/tests" EXCLUDE
  PATTERN "**/example" EXCLUDE
)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/atom_version.h
        ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom
)

# -----------------------------------------------------------------------------
# IDE Folders & Final Message
# -----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
message(STATUS "Atom configured successfully")
