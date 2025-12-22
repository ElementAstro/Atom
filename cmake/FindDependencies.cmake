# FindDependencies.cmake - Standardized dependency finding for the Atom project
# This module provides consistent dependency finding across all modules
#
# Main functions: atom_find_dependency()         - Find a dependency with
# multiple fallback methods atom_setup_dependency_target() - Setup an imported
# target for a dependency atom_find_vcpkg_package()      - Find a package via
# vcpkg paths atom_print_dependency_summary() - Print summary of found
# dependencies
#
# Dependencies found: Core: OpenSSL, ZLIB, SQLite3, fmt, spdlog, Asio, CURL
# Optional: TBB, OpenCV, minizip-ng, libuv, Boost, Python, pybind11, GTest
#
# vcpkg Integration: When ATOM_VCPKG_AVAILABLE is TRUE, vcpkg paths are
# automatically added to the search paths for all dependencies.
#
# Author: Max Qian License: GPL3
# =============================================================================

include_guard(GLOBAL)
include(FindPackageHandleStandardArgs)

# Set policy for consistent behavior
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# =============================================================================
# vcpkg Integration
# =============================================================================

# Set up vcpkg paths if available
if(ATOM_VCPKG_AVAILABLE AND DEFINED ATOM_VCPKG_INSTALLED_DIR)
  # Add vcpkg to CMAKE_PREFIX_PATH for find_package
  list(APPEND CMAKE_PREFIX_PATH "${ATOM_VCPKG_INSTALLED_DIR}")
  list(APPEND CMAKE_PREFIX_PATH "${ATOM_VCPKG_INSTALLED_DIR}/share")

  # Set vcpkg-specific search hints
  set(ATOM_VCPKG_INCLUDE_DIR "${ATOM_VCPKG_INSTALLED_DIR}/include")
  set(ATOM_VCPKG_LIB_DIR "${ATOM_VCPKG_INSTALLED_DIR}/lib")
  set(ATOM_VCPKG_BIN_DIR "${ATOM_VCPKG_INSTALLED_DIR}/bin")
  set(ATOM_VCPKG_SHARE_DIR "${ATOM_VCPKG_INSTALLED_DIR}/share")

  message(
    STATUS "vcpkg paths added to dependency search: ${ATOM_VCPKG_INSTALLED_DIR}"
  )
endif()

# =============================================================================
# Utility Functions
# =============================================================================

# Function to find a package via vcpkg paths specifically
function(atom_find_vcpkg_package PACKAGE_NAME)
  set(options REQUIRED QUIET)
  set(oneValueArgs VERSION)
  set(multiValueArgs COMPONENTS)
  cmake_parse_arguments(AFVP "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(NOT ATOM_VCPKG_AVAILABLE)
    if(AFVP_REQUIRED)
      message(
        FATAL_ERROR
          "vcpkg is not available but ${PACKAGE_NAME} is required via vcpkg")
    endif()
    return()
  endif()

  # Try to find via vcpkg share directory (CONFIG mode)
  set(_config_dir "${ATOM_VCPKG_SHARE_DIR}/${PACKAGE_NAME}")
  if(EXISTS "${_config_dir}")
    if(AFVP_COMPONENTS)
      find_package(${PACKAGE_NAME} ${AFVP_VERSION} CONFIG PATHS "${_config_dir}"
                   NO_DEFAULT_PATH COMPONENTS ${AFVP_COMPONENTS})
    else()
      find_package(${PACKAGE_NAME} ${AFVP_VERSION} CONFIG PATHS
                   "${_config_dir}" NO_DEFAULT_PATH)
    endif()
  endif()

  string(TOUPPER ${PACKAGE_NAME} PKG_UPPER)
  if(${PACKAGE_NAME}_FOUND OR ${PKG_UPPER}_FOUND)
    if(NOT AFVP_QUIET)
      message(STATUS "Found ${PACKAGE_NAME} via vcpkg")
    endif()
  elseif(AFVP_REQUIRED)
    message(FATAL_ERROR "${PACKAGE_NAME} not found in vcpkg installation")
  endif()
endfunction()

# Function to find a dependency with multiple fallback methods
function(atom_find_dependency dep_name)
  set(options REQUIRED QUIET)
  set(oneValueArgs VERSION COMPONENT)
  set(multiValueArgs COMPONENTS PATHS PKG_CONFIG_NAME HINTS)
  cmake_parse_arguments(AFD "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  string(TOUPPER ${dep_name} DEP_UPPER)
  set(found_var "${DEP_UPPER}_FOUND")

  # Skip if already found
  if(${found_var})
    return()
  endif()

  # Method 1: Try find_package first
  if(AFD_COMPONENTS)
    if(AFD_QUIET)
      find_package(${dep_name} ${AFD_VERSION} QUIET
                   COMPONENTS ${AFD_COMPONENTS})
    else()
      find_package(${dep_name} ${AFD_VERSION} COMPONENTS ${AFD_COMPONENTS})
    endif()
  else()
    if(AFD_QUIET)
      find_package(${dep_name} ${AFD_VERSION} QUIET)
    else()
      find_package(${dep_name} ${AFD_VERSION})
    endif()
  endif()

  # Method 2: Try pkg-config if find_package failed
  if(NOT ${found_var} AND AFD_PKG_CONFIG_NAME)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      if(AFD_VERSION)
        pkg_check_modules(${DEP_UPPER} QUIET
                          ${AFD_PKG_CONFIG_NAME}>=${AFD_VERSION})
      else()
        pkg_check_modules(${DEP_UPPER} QUIET ${AFD_PKG_CONFIG_NAME})
      endif()
    endif()
  endif()

  # Method 3: Manual search for header-only libraries
  if(NOT ${found_var} AND AFD_PATHS)
    find_path(
      ${DEP_UPPER}_INCLUDE_DIR
      NAMES ${AFD_PATHS}
      PATHS /usr/include /usr/local/include /mingw64/include
            ${CMAKE_PREFIX_PATH}/include ${AFD_HINTS})
    if(${DEP_UPPER}_INCLUDE_DIR)
      set(${DEP_UPPER}_INCLUDE_DIR
          ${${DEP_UPPER}_INCLUDE_DIR}
          PARENT_SCOPE)
      set(${found_var}
          TRUE
          PARENT_SCOPE)
      message(
        STATUS "Found ${dep_name} headers at: ${${DEP_UPPER}_INCLUDE_DIR}")
    endif()
  endif()

  # Handle results
  if(${found_var})
    if(NOT AFD_QUIET)
      if(${dep_name}_VERSION)
        message(STATUS "Found ${dep_name}: ${${dep_name}_VERSION}")
      elseif(${DEP_UPPER}_VERSION)
        message(STATUS "Found ${dep_name}: ${${DEP_UPPER}_VERSION}")
      else()
        message(STATUS "Found ${dep_name}")
      endif()
    endif()
  else()
    if(AFD_REQUIRED)
      message(FATAL_ERROR "${dep_name} is required but was not found")
    else()
      message(WARNING "${dep_name} not found - related features may be limited")
    endif()
  endif()
endfunction()

# Function to setup a dependency target
function(atom_setup_dependency_target dep_name target_name)
  string(TOUPPER ${dep_name} DEP_UPPER)

  if(NOT TARGET ${target_name})
    # Create imported target if it doesn't exist
    if(${DEP_UPPER}_FOUND)
      add_library(${target_name} INTERFACE IMPORTED)

      # Set include directories
      if(${DEP_UPPER}_INCLUDE_DIRS)
        target_include_directories(${target_name}
                                   INTERFACE ${${DEP_UPPER}_INCLUDE_DIRS})
      elseif(${DEP_UPPER}_INCLUDE_DIR)
        target_include_directories(${target_name}
                                   INTERFACE ${${DEP_UPPER}_INCLUDE_DIR})
      endif()

      # Set libraries
      if(${DEP_UPPER}_LIBRARIES)
        target_link_libraries(${target_name}
                              INTERFACE ${${DEP_UPPER}_LIBRARIES})
      elseif(${dep_name}_LIBRARIES)
        target_link_libraries(${target_name} INTERFACE ${${dep_name}_LIBRARIES})
      endif()

      # Set compile flags
      if(${DEP_UPPER}_CFLAGS_OTHER)
        target_compile_options(${target_name}
                               INTERFACE ${${DEP_UPPER}_CFLAGS_OTHER})
      endif()
    endif()
  endif()
endfunction()

# =============================================================================
# Core Dependencies
# =============================================================================

# OpenSSL - Required for non-MSVC, optional for MSVC
if(MSVC)
  atom_find_dependency(OpenSSL QUIET)
  if(NOT OpenSSL_FOUND)
    message(
      STATUS
        "OpenSSL not found for MSVC - some cryptographic features will be disabled"
    )
  endif()
else()
  # Try manual lookup first for MinGW/MSYS2
  find_path(
    OPENSSL_INCLUDE_DIR
    NAMES openssl/ssl.h
    PATHS /mingw64/include /usr/include /usr/local/include
          D:/msys64/mingw64/include ${CMAKE_PREFIX_PATH}/include)
  find_library(
    OPENSSL_SSL_LIBRARY
    NAMES ssl
    PATHS /mingw64/lib /usr/lib /usr/local/lib D:/msys64/mingw64/lib
          ${CMAKE_PREFIX_PATH}/lib)
  find_library(
    OPENSSL_CRYPTO_LIBRARY
    NAMES crypto
    PATHS /mingw64/lib /usr/lib /usr/local/lib D:/msys64/mingw64/lib
          ${CMAKE_PREFIX_PATH}/lib)
  if(OPENSSL_INCLUDE_DIR
     AND OPENSSL_SSL_LIBRARY
     AND OPENSSL_CRYPTO_LIBRARY)
    if(NOT TARGET OpenSSL::SSL)
      add_library(OpenSSL::SSL UNKNOWN IMPORTED GLOBAL)
      set_target_properties(
        OpenSSL::SSL
        PROPERTIES IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY}
                   INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
    endif()
    if(NOT TARGET OpenSSL::Crypto)
      add_library(OpenSSL::Crypto UNKNOWN IMPORTED GLOBAL)
      set_target_properties(
        OpenSSL::Crypto
        PROPERTIES IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY}
                   INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
    endif()
    set(OpenSSL_FOUND
        TRUE
        CACHE BOOL "OpenSSL found" FORCE)
    set(OPENSSL_FOUND
        TRUE
        CACHE BOOL "OpenSSL found" FORCE)
    message(STATUS "OpenSSL found via manual lookup: ${OPENSSL_SSL_LIBRARY}")
  else()
    atom_find_dependency(OpenSSL QUIET)
    if(NOT OpenSSL_FOUND)
      message(
        STATUS
          "OpenSSL not found - some cryptographic features will be disabled")
    endif()
  endif()
endif()

# ZLIB - Required for non-MSVC, optional for MSVC
if(MSVC)
  atom_find_dependency(ZLIB QUIET)
  if(NOT ZLIB_FOUND)
    message(
      STATUS
        "ZLIB not found for MSVC - some compression features will be disabled")
  endif()
else()
  # Try manual lookup first (common on MinGW/Unix)
  find_path(
    ZLIB_INCLUDE_DIR
    NAMES zlib.h
    PATHS /mingw64/include /usr/include /usr/local/include
          D:/msys64/mingw64/include ${CMAKE_PREFIX_PATH}/include)
  find_library(
    ZLIB_LIBRARY
    NAMES z zlib
    PATHS /mingw64/lib /usr/lib /usr/local/lib D:/msys64/mingw64/lib
          ${CMAKE_PREFIX_PATH}/lib)
  if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
    if(NOT TARGET ZLIB::ZLIB)
      add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
      set_target_properties(
        ZLIB::ZLIB PROPERTIES IMPORTED_LOCATION ${ZLIB_LIBRARY}
                              INTERFACE_INCLUDE_DIRECTORIES ${ZLIB_INCLUDE_DIR})
    endif()
    set(ZLIB_FOUND TRUE)
    message(STATUS "ZLIB found via manual lookup: ${ZLIB_LIBRARY}")
  else()
    # Fallback to standard search and fail hard if still missing
    atom_find_dependency(ZLIB REQUIRED)
  endif()
endif()

# SQLite3 - Core database functionality
atom_find_dependency(SQLite3 QUIET)

# fmt - String formatting (required by spdlog)
find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND)
  atom_find_dependency(fmt QUIET PKG_CONFIG_NAME fmt)
endif()
if(fmt_FOUND)
  message(STATUS "fmt found: ${fmt_VERSION}")
endif()

# spdlog - Logging library (use compiled version to avoid ODR violations)
find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
  # Try pkg-config fallback
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SPDLOG QUIET spdlog)
    if(SPDLOG_FOUND AND NOT TARGET spdlog::spdlog)
      add_library(spdlog::spdlog INTERFACE IMPORTED)
      target_include_directories(spdlog::spdlog
                                 INTERFACE ${SPDLOG_INCLUDE_DIRS})
      target_link_libraries(spdlog::spdlog INTERFACE ${SPDLOG_LIBRARIES})
      set(spdlog_FOUND TRUE)
    endif()
  endif()
endif()
if(spdlog_FOUND)
  message(STATUS "spdlog found: ${spdlog_VERSION}")
endif()

# CURL - HTTP client library (optional, for atom-web)
find_package(CURL QUIET)
if(CURL_FOUND)
  message(STATUS "CURL found: ${CURL_VERSION_STRING}")
else()
  # Try pkg-config fallback
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(CURL QUIET libcurl)
    if(CURL_FOUND)
      message(STATUS "CURL found via pkg-config: ${CURL_VERSION}")
    endif()
  endif()
endif()

# =============================================================================
# Header-only Dependencies
# =============================================================================

# Asio - Networking (header-only, standalone) Try manual lookup first for
# MinGW/MSYS2
find_path(
  ASIO_INCLUDE_DIR
  NAMES asio.hpp
  PATHS /mingw64/include /usr/include /usr/local/include
        D:/msys64/mingw64/include ${CMAKE_PREFIX_PATH}/include)
if(ASIO_INCLUDE_DIR)
  set(ASIO_FOUND TRUE)
  add_definitions(-DASIO_STANDALONE)
  if(NOT TARGET asio::asio)
    add_library(asio::asio INTERFACE IMPORTED)
    set_target_properties(asio::asio PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                ${ASIO_INCLUDE_DIR})
  endif()
  message(STATUS "Asio found via manual lookup: ${ASIO_INCLUDE_DIR}")
else()
  atom_find_dependency(
    asio
    QUIET
    PATHS
    asio.hpp
    HINTS
    /mingw64/include
    /usr/include
    /usr/local/include)
  if(ASIO_FOUND)
    add_definitions(-DASIO_STANDALONE)
    atom_setup_dependency_target(asio asio::asio)
  endif()
endif()

# =============================================================================
# Optional Dependencies
# =============================================================================

# SSH support
if(ATOM_USE_SSH)
  atom_find_dependency(libssh REQUIRED PKG_CONFIG_NAME libssh)
  if(LIBSSH_FOUND)
    add_definitions(-DENABLE_SSH -DENABLE_LIBSSH)
    atom_setup_dependency_target(libssh libssh::libssh)
  endif()
endif()

# Python bindings
if(ATOM_BUILD_PYTHON_BINDINGS)
  atom_find_dependency(Python REQUIRED COMPONENTS Interpreter Development)
  # Try to find pybind11 directly first
  find_package(pybind11 CONFIG QUIET)
  if(NOT pybind11_FOUND)
    atom_find_dependency(pybind11 REQUIRED)
  endif()
endif()

# Testing framework
if(ATOM_BUILD_TESTS)
  include(cmake/FindGTestFixed.cmake)
endif()

# =============================================================================
# Boost Dependencies (Optional)
# =============================================================================

if(ATOM_USE_BOOST)
  set(BOOST_COMPONENTS)

  if(ATOM_USE_BOOST_LOCKFREE)
    list(APPEND BOOST_COMPONENTS atomic thread)
  endif()

  if(ATOM_USE_BOOST_GRAPH)
    list(APPEND BOOST_COMPONENTS graph)
  endif()

  if(ATOM_USE_BOOST_CONTAINER)
    list(APPEND BOOST_COMPONENTS container)
  endif()

  if(BOOST_COMPONENTS)
    atom_find_dependency(Boost QUIET VERSION 1.74 COMPONENTS
                         ${BOOST_COMPONENTS})
  else()
    atom_find_dependency(Boost QUIET VERSION 1.74)
  endif()
endif()

# =============================================================================
# Optional Feature Dependencies (only loaded when features are enabled)
# =============================================================================

# Image processing (OpenCV) - only when ATOM_BUILD_IMAGE is ON
if(ATOM_BUILD_IMAGE AND ATOM_USE_OPENCV)
  find_package(OpenCV QUIET)
  if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    set(ATOM_HAS_OPENCV
        TRUE
        CACHE BOOL "OpenCV available" FORCE)
  else()
    message(STATUS "OpenCV not found - image features will be limited")
    set(ATOM_HAS_OPENCV
        FALSE
        CACHE BOOL "OpenCV not available" FORCE)
  endif()
endif()

# Parallel algorithms (TBB) - only when explicitly requested
if(ATOM_USE_TBB)
  find_package(TBB QUIET)
  if(TBB_FOUND)
    message(STATUS "TBB found for parallel algorithms")
    set(ATOM_HAS_TBB
        TRUE
        CACHE BOOL "TBB available" FORCE)
  else()
    message(STATUS "TBB not found - parallel algorithms will use std::thread")
    set(ATOM_HAS_TBB
        FALSE
        CACHE BOOL "TBB not available" FORCE)
  endif()
endif()

# Advanced compression (minizip-ng) - only when explicitly requested
if(ATOM_USE_MINIZIP)
  find_package(minizip-ng QUIET)
  if(minizip-ng_FOUND)
    message(STATUS "minizip-ng found for advanced compression")
    set(ATOM_HAS_MINIZIP
        TRUE
        CACHE BOOL "minizip-ng available" FORCE)
  else()
    message(STATUS "minizip-ng not found - using basic zlib compression")
    set(ATOM_HAS_MINIZIP
        FALSE
        CACHE BOOL "minizip-ng not available" FORCE)
  endif()
endif()

# Advanced networking (libuv) - only when explicitly requested
if(ATOM_USE_LIBUV)
  find_package(libuv CONFIG QUIET)
  if(libuv_FOUND)
    message(STATUS "libuv found for advanced async I/O")
    set(ATOM_HAS_LIBUV
        TRUE
        CACHE BOOL "libuv available" FORCE)
  else()
    message(STATUS "libuv not found - using asio for async I/O")
    set(ATOM_HAS_LIBUV
        FALSE
        CACHE BOOL "libuv not available" FORCE)
  endif()
endif()

# =============================================================================
# Platform-specific Dependencies
# =============================================================================

if(WIN32)
  # Windows-specific libraries are handled by target_link_libraries in
  # individual modules
  message(
    STATUS
      "Windows platform detected - platform-specific dependencies will be handled per module"
  )
endif()

if(UNIX AND NOT APPLE)
  # Linux-specific dependencies (commented out for now as per original
  # CMakeLists.txt) atom_find_dependency(X11 QUIET)
  # atom_find_dependency(PkgConfig REQUIRED) if(PkgConfig_FOUND)
  # pkg_check_modules(UDEV QUIET libudev) endif()
endif()

# =============================================================================
# Dependency Summary Function
# =============================================================================

# Function to print dependency summary (can be called on demand)
function(atom_print_dependency_summary)
  message(STATUS "")
  message(STATUS "======================================")
  message(STATUS "       DEPENDENCY SUMMARY")
  message(STATUS "======================================")

  # vcpkg info
  if(ATOM_VCPKG_AVAILABLE)
    message(STATUS "vcpkg:        ENABLED (${ATOM_VCPKG_TRIPLET})")
  else()
    message(STATUS "vcpkg:        DISABLED")
  endif()

  message(STATUS "")
  message(STATUS "--- Core Dependencies ---")
  _atom_dep_status("OpenSSL" OpenSSL_FOUND)
  _atom_dep_status("ZLIB" ZLIB_FOUND)
  _atom_dep_status("SQLite3" SQLite3_FOUND)
  _atom_dep_status("fmt" fmt_FOUND)
  _atom_dep_status("spdlog" spdlog_FOUND)
  _atom_dep_status("Asio" ASIO_FOUND)

  message(STATUS "")
  message(STATUS "--- Optional Dependencies ---")
  _atom_dep_status("CURL" CURL_FOUND)
  _atom_dep_status("OpenCV" ATOM_HAS_OPENCV)
  _atom_dep_status("TBB" ATOM_HAS_TBB)
  _atom_dep_status("minizip-ng" ATOM_HAS_MINIZIP)
  _atom_dep_status("libuv" ATOM_HAS_LIBUV)

  if(ATOM_USE_SSH)
    _atom_dep_status("LibSSH" LIBSSH_FOUND)
  endif()

  if(ATOM_BUILD_PYTHON_BINDINGS)
    message(STATUS "")
    message(STATUS "--- Python Bindings ---")
    _atom_dep_status("Python" Python_FOUND)
    _atom_dep_status("pybind11" pybind11_FOUND)
  endif()

  if(ATOM_BUILD_TESTS)
    message(STATUS "")
    message(STATUS "--- Testing ---")
    _atom_dep_status("GTest" GTEST_FOUND)
  endif()

  if(ATOM_USE_BOOST)
    message(STATUS "")
    message(STATUS "--- Boost ---")
    _atom_dep_status("Boost" Boost_FOUND)
  endif()

  message(STATUS "======================================")
  message(STATUS "")
endfunction()

# Helper function for dependency status display
function(_atom_dep_status name found_var)
  if(${found_var})
    message(STATUS "  [x] ${name}")
  else()
    message(STATUS "  [ ] ${name}")
  endif()
endfunction()

# =============================================================================
# Auto-print Summary
# =============================================================================
atom_print_dependency_summary()
