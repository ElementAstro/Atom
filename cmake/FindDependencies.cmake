# FindDependencies.cmake Standardized dependency finding for the Atom project
# This module provides consistent dependency finding across all modules

include(FindPackageHandleStandardArgs)

# Set policy for consistent behavior
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# =============================================================================
# Utility Functions
# =============================================================================

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
# Summary
# =============================================================================

message(STATUS "=== Dependency Summary ===")
message(STATUS "OpenSSL: ${OpenSSL_FOUND}")
message(STATUS "ZLIB: ${ZLIB_FOUND}")
message(STATUS "SQLite3: ${SQLite3_FOUND}")
message(STATUS "fmt: ${fmt_FOUND}")
message(STATUS "spdlog: ${spdlog_FOUND}")
message(STATUS "CURL: ${CURL_FOUND}")
message(STATUS "Asio: ${ASIO_FOUND}")

if(ATOM_USE_SSH)
  message(STATUS "LibSSH: ${LIBSSH_FOUND}")
endif()

if(ATOM_BUILD_PYTHON_BINDINGS)
  message(STATUS "Python: ${Python_FOUND}")
  message(STATUS "pybind11: ${pybind11_FOUND}")
endif()

if(ATOM_BUILD_TESTS)
  message(STATUS "GTest: ${GTEST_FOUND}")
endif()

if(ATOM_USE_BOOST)
  message(STATUS "Boost: ${Boost_FOUND}")
endif()

message(STATUS "==========================")
