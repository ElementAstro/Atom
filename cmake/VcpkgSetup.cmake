# =============================================================================
# VcpkgSetup.cmake - vcpkg integration for Atom project
# =============================================================================
# This module configures vcpkg integration, detects vcpkg installation, sets up
# appropriate triplets, and provides helper functions.
#
# Main functions: atom_vcpkg_configure()         - Configure vcpkg with options
# atom_vcpkg_print_info()        - Print vcpkg configuration info
# atom_vcpkg_check_package()     - Check if a package is available
# atom_vcpkg_get_triplet()       - Get the current vcpkg triplet
# atom_vcpkg_enable_feature()    - Enable a vcpkg manifest feature
#
# Variables set: ATOM_VCPKG_ROOT               - vcpkg installation root
# ATOM_VCPKG_TRIPLET            - Current vcpkg triplet ATOM_VCPKG_MANIFEST_MODE
# - Whether manifest mode is active ATOM_VCPKG_INSTALLED_DIR      - vcpkg
# installed packages directory
#
# Author: Max Qian License: GPL3
# =============================================================================

include_guard(GLOBAL)

if(NOT USE_VCPKG)
  message(STATUS "USE_VCPKG is OFF. Skipping vcpkg setup.")
  # Still define helper functions but mark vcpkg as unavailable
  set(ATOM_VCPKG_AVAILABLE
      FALSE
      CACHE INTERNAL "vcpkg availability flag")
  return()
endif()

message(STATUS "Setting up vcpkg integration...")

# Include baseline management module (always available)
include(${CMAKE_CURRENT_LIST_DIR}/UpdateBaseline.cmake)

# Auto-update baseline if requested
if(UPDATE_VCPKG_BASELINE)
  atom_vcpkg_update_baseline()
endif()
set(VCPKG_INSTALL_OPTIONS
    "--no-print-usage"
    CACHE STRING "Additional vcpkg install options")

# On MSVC builds, prefer Visual Studio generator for vcpkg port builds to avoid
# rc.exe issues
if(MSVC AND NOT DEFINED VCPKG_CMAKE_GENERATOR)
  set(VCPKG_CMAKE_GENERATOR
      "Visual Studio 17 2022"
      CACHE STRING "Generator used by vcpkg to build ports")
  message(STATUS "Configured VCPKG_CMAKE_GENERATOR=${VCPKG_CMAKE_GENERATOR}")
endif()

# Sanitize PATH to avoid MSYS2 tools interfering with MSVC/vcpkg on Windows
if(WIN32 AND MSVC)
  if(DEFINED ENV{PATH})
    string(REPLACE ";" ";" _atom_path_sep "$ENV{PATH}")
    set(_atom_new_path "")
    foreach(_p IN LISTS _atom_path_sep)
      if(NOT _p MATCHES ".*msys64.*")
        if(_atom_new_path STREQUAL "")
          set(_atom_new_path "${_p}")
        else()
          set(_atom_new_path "${_atom_new_path};${_p}")
        endif()
      endif()
    endforeach()
    if(NOT _atom_new_path STREQUAL "")
      set(ENV{PATH} "${_atom_new_path}")
      message(STATUS "Sanitized PATH for MSVC/vcpkg to remove MSYS2 entries")
    endif()
    unset(_atom_new_path)
    unset(_atom_path_sep)
  endif()
endif()

if(DEFINED ENV{MSYSTEM})
  message(STATUS "MSYS2 environment detected by VcpkgSetup: $ENV{MSYSTEM}")
  set(ATOM_MSYS2_ENV
      TRUE
      CACHE INTERNAL "Flag indicating MSYS2 environment")
else()
  set(ATOM_MSYS2_ENV
      FALSE
      CACHE INTERNAL "Flag indicating MSYS2 environment")
endif()

if(DEFINED ENV{VCPKG_ROOT})
  set(POTENTIAL_VCPKG_PATH "$ENV{VCPKG_ROOT}")
  message(
    STATUS
      "Found vcpkg from VCPKG_ROOT environment variable: ${POTENTIAL_VCPKG_PATH}"
  )
else()
  if(ATOM_MSYS2_ENV)
    if(EXISTS "$ENV{USERPROFILE}/vcpkg") # Windows paths under MSYS2
      set(POTENTIAL_VCPKG_PATH "$ENV{USERPROFILE}/vcpkg")
    elseif(EXISTS "$ENV{HOME}/vcpkg") # Unix-like paths under MSYS2
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
  else() # Linux/macOS
    if(EXISTS "/usr/local/vcpkg")
      set(POTENTIAL_VCPKG_PATH "/usr/local/vcpkg")
    elseif(EXISTS "$ENV{HOME}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "$ENV{HOME}/vcpkg")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
    endif()
  endif()
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED POTENTIAL_VCPKG_PATH
     AND EXISTS "${POTENTIAL_VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE
        "${POTENTIAL_VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Set vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    if(NOT DEFINED ENV{VCPKG_ROOT})
      set(ENV{VCPKG_ROOT} "${POTENTIAL_VCPKG_PATH}") # Set for vcpkg.cmake
                                                     # script
      message(
        STATUS
          "Set VCPKG_ROOT environment variable for this CMake run: ${POTENTIAL_VCPKG_PATH}"
      )
    endif()
  else()
    message(
      FATAL_ERROR
        "USE_VCPKG is ON but vcpkg toolchain was not found. Searched VCPKG_ROOT or common paths. "
        "Please install vcpkg, set VCPKG_ROOT, or provide CMAKE_TOOLCHAIN_FILE."
    )
  endif()
else()
  message(
    STATUS
      "CMAKE_TOOLCHAIN_FILE is already set: ${CMAKE_TOOLCHAIN_FILE}. Assuming vcpkg is configured."
  )
  if(NOT DEFINED ENV{VCPKG_ROOT}
     AND CMAKE_TOOLCHAIN_FILE MATCHES
         "(.*/vcpkg)/scripts/buildsystems/vcpkg.cmake")
    set(POTENTIAL_VCPKG_PATH "${CMAKE_MATCH_1}")
    set(ENV{VCPKG_ROOT} "${POTENTIAL_VCPKG_PATH}")
    message(
      STATUS
        "Inferred and set VCPKG_ROOT from CMAKE_TOOLCHAIN_FILE: ${POTENTIAL_VCPKG_PATH}"
    )
  endif()

  # Enable tests-related manifest features so gtest is installed when building
  # tests
  if(ATOM_BUILD_TESTS)
    set(ENV{VCPKG_MANIFEST_FEATURES} "tests")
    message(STATUS "VCPKG_MANIFEST_FEATURES set to 'tests' for gtest support")
  endif()

endif()

# Ensure POTENTIAL_VCPKG_PATH is set for subsequent scripts if vcpkg is used
if(DEFINED POTENTIAL_VCPKG_PATH AND EXISTS "${POTENTIAL_VCPKG_PATH}")
  set(ATOM_VCPKG_ROOT
      "${POTENTIAL_VCPKG_PATH}"
      CACHE INTERNAL "Detected vcpkg root directory")
  message(STATUS "vcpkg setup complete. Vcpkg root: ${ATOM_VCPKG_ROOT}")
elseif(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}")
  set(ATOM_VCPKG_ROOT
      "$ENV{VCPKG_ROOT}"
      CACHE INTERNAL "Detected vcpkg root directory")
  message(STATUS "vcpkg setup complete. Vcpkg root: ${ATOM_VCPKG_ROOT}")
else()
  message(
    FATAL_ERROR
      "Vcpkg root directory (ATOM_VCPKG_ROOT) could not be determined. "
      "Ensure VCPKG_ROOT is set or vcpkg is in a standard location, or CMAKE_TOOLCHAIN_FILE points to vcpkg."
  )
endif()

# =============================================================================
# vcpkg Configuration Variables
# =============================================================================

# Mark vcpkg as available
set(ATOM_VCPKG_AVAILABLE
    TRUE
    CACHE INTERNAL "vcpkg availability flag")

# Detect triplet
if(DEFINED VCPKG_TARGET_TRIPLET)
  set(ATOM_VCPKG_TRIPLET
      "${VCPKG_TARGET_TRIPLET}"
      CACHE STRING "vcpkg triplet")
else()
  # Auto-detect triplet based on platform
  if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      if(MINGW)
        set(ATOM_VCPKG_TRIPLET
            "x64-mingw-dynamic"
            CACHE STRING "vcpkg triplet")
      else()
        set(ATOM_VCPKG_TRIPLET
            "x64-windows"
            CACHE STRING "vcpkg triplet")
      endif()
    else()
      if(MINGW)
        set(ATOM_VCPKG_TRIPLET
            "x86-mingw-dynamic"
            CACHE STRING "vcpkg triplet")
      else()
        set(ATOM_VCPKG_TRIPLET
            "x86-windows"
            CACHE STRING "vcpkg triplet")
      endif()
    endif()
  elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
      set(ATOM_VCPKG_TRIPLET
          "arm64-osx"
          CACHE STRING "vcpkg triplet")
    else()
      set(ATOM_VCPKG_TRIPLET
          "x64-osx"
          CACHE STRING "vcpkg triplet")
    endif()
  else() # Linux
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(ATOM_VCPKG_TRIPLET
          "x64-linux"
          CACHE STRING "vcpkg triplet")
    else()
      set(ATOM_VCPKG_TRIPLET
          "x86-linux"
          CACHE STRING "vcpkg triplet")
    endif()
  endif()
endif()

# Detect manifest mode
if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
  set(ATOM_VCPKG_MANIFEST_MODE
      TRUE
      CACHE INTERNAL "vcpkg manifest mode")
  set(ATOM_VCPKG_INSTALLED_DIR
      "${CMAKE_SOURCE_DIR}/vcpkg_installed/${ATOM_VCPKG_TRIPLET}"
      CACHE PATH "vcpkg installed packages directory")
else()
  set(ATOM_VCPKG_MANIFEST_MODE
      FALSE
      CACHE INTERNAL "vcpkg manifest mode")
  set(ATOM_VCPKG_INSTALLED_DIR
      "${ATOM_VCPKG_ROOT}/installed/${ATOM_VCPKG_TRIPLET}"
      CACHE PATH "vcpkg installed packages directory")
endif()

# =============================================================================
# vcpkg Helper Functions
# =============================================================================

# Function to print vcpkg configuration info
function(atom_vcpkg_print_info)
  message(STATUS "")
  message(STATUS "=== vcpkg Configuration ===")
  message(STATUS "vcpkg Root:       ${ATOM_VCPKG_ROOT}")
  message(STATUS "Triplet:          ${ATOM_VCPKG_TRIPLET}")
  message(STATUS "Manifest Mode:    ${ATOM_VCPKG_MANIFEST_MODE}")
  message(STATUS "Installed Dir:    ${ATOM_VCPKG_INSTALLED_DIR}")
  message(STATUS "Toolchain File:   ${CMAKE_TOOLCHAIN_FILE}")
  if(DEFINED ENV{VCPKG_MANIFEST_FEATURES})
    message(STATUS "Features:         $ENV{VCPKG_MANIFEST_FEATURES}")
  endif()
  message(STATUS "===========================")
  message(STATUS "")
endfunction()

# Function to check if a vcpkg package is installed
function(atom_vcpkg_check_package PACKAGE_NAME RESULT_VAR)
  set(_found FALSE)

  if(ATOM_VCPKG_AVAILABLE)
    # Check in installed directory
    set(_pkg_dir "${ATOM_VCPKG_INSTALLED_DIR}/share/${PACKAGE_NAME}")
    if(EXISTS "${_pkg_dir}")
      set(_found TRUE)
    endif()

    # Alternative: check for include directory
    if(NOT _found)
      set(_inc_dir "${ATOM_VCPKG_INSTALLED_DIR}/include")
      if(EXISTS "${_inc_dir}/${PACKAGE_NAME}"
         OR EXISTS "${_inc_dir}/${PACKAGE_NAME}.h")
        set(_found TRUE)
      endif()
    endif()
  endif()

  set(${RESULT_VAR}
      ${_found}
      PARENT_SCOPE)
endfunction()

# Function to get vcpkg triplet
function(atom_vcpkg_get_triplet RESULT_VAR)
  set(${RESULT_VAR}
      "${ATOM_VCPKG_TRIPLET}"
      PARENT_SCOPE)
endfunction()

# Function to enable a vcpkg manifest feature
function(atom_vcpkg_enable_feature FEATURE_NAME)
  if(ATOM_VCPKG_MANIFEST_MODE)
    if(DEFINED ENV{VCPKG_MANIFEST_FEATURES})
      set(ENV{VCPKG_MANIFEST_FEATURES}
          "$ENV{VCPKG_MANIFEST_FEATURES};${FEATURE_NAME}")
    else()
      set(ENV{VCPKG_MANIFEST_FEATURES} "${FEATURE_NAME}")
    endif()
    message(STATUS "Enabled vcpkg feature: ${FEATURE_NAME}")
  else()
    message(
      WARNING
        "atom_vcpkg_enable_feature: Not in manifest mode, feature '${FEATURE_NAME}' ignored"
    )
  endif()
endfunction()

# Function to list all installed vcpkg packages
function(atom_vcpkg_list_installed)
  if(NOT ATOM_VCPKG_AVAILABLE)
    message(STATUS "vcpkg is not available")
    return()
  endif()

  set(_share_dir "${ATOM_VCPKG_INSTALLED_DIR}/share")
  if(EXISTS "${_share_dir}")
    file(
      GLOB _packages
      RELATIVE "${_share_dir}"
      "${_share_dir}/*")
    list(FILTER _packages EXCLUDE REGEX "^vcpkg") # Exclude vcpkg internal dirs
    message(STATUS "Installed vcpkg packages: ${_packages}")
  else()
    message(STATUS "No packages installed in ${_share_dir}")
  endif()
endfunction()

# Function to configure vcpkg features based on CMake options
function(atom_vcpkg_configure_features)
  if(NOT ATOM_VCPKG_MANIFEST_MODE)
    return()
  endif()

  set(_features "")

  # Map CMake options to vcpkg features
  if(ATOM_BUILD_TESTS)
    list(APPEND _features "tests")
  endif()

  if(ATOM_BUILD_EXAMPLES)
    list(APPEND _features "examples")
  endif()

  if(ATOM_USE_OPENCV)
    list(APPEND _features "image")
  endif()

  if(ATOM_USE_TBB)
    list(APPEND _features "parallel")
  endif()

  if(ATOM_USE_BOOST)
    list(APPEND _features "boost-minimal")
  endif()

  if(ATOM_USE_MINIZIP)
    list(APPEND _features "compression")
  endif()

  if(ATOM_USE_LIBUV OR ATOM_USE_CURL)
    list(APPEND _features "network")
  endif()

  if(ATOM_BUILD_PYTHON_BINDINGS)
    list(APPEND _features "python")
  endif()

  # Set the features environment variable
  if(_features)
    list(JOIN _features ";" _features_str)
    set(ENV{VCPKG_MANIFEST_FEATURES} "${_features_str}")
    message(STATUS "vcpkg manifest features: ${_features_str}")
  endif()
endfunction()

# Function to add vcpkg include/lib paths for a target
function(atom_vcpkg_setup_target TARGET_NAME)
  if(NOT ATOM_VCPKG_AVAILABLE OR NOT TARGET ${TARGET_NAME})
    return()
  endif()

  # Add vcpkg include directory
  if(EXISTS "${ATOM_VCPKG_INSTALLED_DIR}/include")
    target_include_directories(${TARGET_NAME} SYSTEM
                               PRIVATE "${ATOM_VCPKG_INSTALLED_DIR}/include")
  endif()

  # Add vcpkg library directory
  if(EXISTS "${ATOM_VCPKG_INSTALLED_DIR}/lib")
    target_link_directories(${TARGET_NAME} PRIVATE
                            "${ATOM_VCPKG_INSTALLED_DIR}/lib")
  endif()
endfunction()

# =============================================================================
# Auto-configure features based on CMake options
# =============================================================================
atom_vcpkg_configure_features()

message(STATUS "vcpkg triplet: ${ATOM_VCPKG_TRIPLET}")
message(STATUS "vcpkg manifest mode: ${ATOM_VCPKG_MANIFEST_MODE}")
