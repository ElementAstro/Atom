# =============================================================================
# UpdateBaseline.cmake - vcpkg baseline management
# =============================================================================
# This module provides comprehensive vcpkg baseline management including: -
# Fetching latest baseline from GitHub - Caching baseline information - Backup
# and rollback support - Version validation - vcpkg.json version updates
#
# Main functions: atom_vcpkg_update_baseline()      - Update to latest baseline
# atom_vcpkg_set_baseline()         - Set specific baseline
# atom_vcpkg_get_current_baseline() - Get current baseline from vcpkg.json
# atom_vcpkg_backup_manifest()      - Backup vcpkg.json
# atom_vcpkg_restore_manifest()     - Restore vcpkg.json from backup
# atom_vcpkg_update_version()       - Update version in vcpkg.json
# atom_vcpkg_validate_baseline()    - Validate a baseline SHA
#
# Options: UPDATE_VCPKG_BASELINE      - Enable baseline update
# VCPKG_BASELINE_CACHE_HOURS - Cache duration (default: 24)
#
# Author: Max Qian License: GPL3
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# Configuration
# =============================================================================

# Known good baseline (fallback)
set(ATOM_VCPKG_DEFAULT_BASELINE
    "dbe35ceb30c688bf72e952ab23778e009a578f18"
    CACHE STRING "Default vcpkg baseline SHA")

# Cache duration in hours
set(VCPKG_BASELINE_CACHE_HOURS
    24
    CACHE STRING "Hours to cache baseline info")

# GitHub API URLs
set(ATOM_VCPKG_COMMITS_API
    "https://api.github.com/repos/microsoft/vcpkg/commits/master")
set(ATOM_VCPKG_TAGS_API "https://api.github.com/repos/microsoft/vcpkg/tags")

# Paths
set(ATOM_VCPKG_JSON_PATH "${CMAKE_SOURCE_DIR}/vcpkg.json")
set(ATOM_VCPKG_BACKUP_PATH "${CMAKE_BINARY_DIR}/vcpkg.json.backup")
set(ATOM_VCPKG_CACHE_FILE "${CMAKE_BINARY_DIR}/vcpkg_baseline_cache.txt")

# =============================================================================
# Helper Functions
# =============================================================================

# Function to check if cache is valid
function(_atom_vcpkg_cache_valid RESULT_VAR)
  set(_valid FALSE)

  if(EXISTS "${ATOM_VCPKG_CACHE_FILE}")
    file(TIMESTAMP "${ATOM_VCPKG_CACHE_FILE}" _cache_time "%s")
    string(TIMESTAMP _current_time "%s")
    math(EXPR _age_seconds "${_current_time} - ${_cache_time}")
    math(EXPR _max_age_seconds "${VCPKG_BASELINE_CACHE_HOURS} * 3600")

    if(_age_seconds LESS _max_age_seconds)
      set(_valid TRUE)
    endif()
  endif()

  set(${RESULT_VAR}
      ${_valid}
      PARENT_SCOPE)
endfunction()

# Function to read cached baseline
function(_atom_vcpkg_read_cache RESULT_VAR)
  set(_baseline "")

  if(EXISTS "${ATOM_VCPKG_CACHE_FILE}")
    file(READ "${ATOM_VCPKG_CACHE_FILE}" _content)
    string(STRIP "${_content}" _baseline)
  endif()

  set(${RESULT_VAR}
      "${_baseline}"
      PARENT_SCOPE)
endfunction()

# Function to write baseline to cache
function(_atom_vcpkg_write_cache BASELINE)
  file(WRITE "${ATOM_VCPKG_CACHE_FILE}" "${BASELINE}")
endfunction()

# Function to fetch baseline from GitHub API
function(_atom_vcpkg_fetch_baseline RESULT_VAR SUCCESS_VAR)
  set(_baseline "")
  set(_success FALSE)

  set(_temp_file "${CMAKE_BINARY_DIR}/vcpkg_api_response.json")

  message(STATUS "Fetching latest vcpkg baseline from GitHub...")

  file(
    DOWNLOAD "${ATOM_VCPKG_COMMITS_API}" "${_temp_file}"
    TIMEOUT 15
    STATUS _download_status
    LOG _download_log
    TLS_VERIFY ON)

  list(GET _download_status 0 _error_code)
  list(GET _download_status 1 _error_msg)

  if(_error_code EQUAL 0)
    file(READ "${_temp_file}" _response)
    file(REMOVE "${_temp_file}")

    # Extract SHA from JSON response
    string(REGEX MATCH "\"sha\"[ \t\n\r]*:[ \t\n\r]*\"([a-f0-9]+)\"" _match
                 "${_response}")

    if(_match)
      set(_baseline "${CMAKE_MATCH_1}")
      set(_success TRUE)
      message(STATUS "Fetched baseline: ${_baseline}")
    else()
      message(WARNING "Failed to parse baseline from GitHub response")
    endif()
  else()
    message(WARNING "Failed to download from GitHub: ${_error_msg}")
    if(EXISTS "${_temp_file}")
      file(REMOVE "${_temp_file}")
    endif()
  endif()

  set(${RESULT_VAR}
      "${_baseline}"
      PARENT_SCOPE)
  set(${SUCCESS_VAR}
      ${_success}
      PARENT_SCOPE)
endfunction()

# =============================================================================
# Public Functions
# =============================================================================

# Function to get current baseline from vcpkg.json
function(atom_vcpkg_get_current_baseline RESULT_VAR)
  set(_baseline "")

  if(EXISTS "${ATOM_VCPKG_JSON_PATH}")
    file(READ "${ATOM_VCPKG_JSON_PATH}" _content)

    string(REGEX MATCH
                 "\"builtin-baseline\"[ \t\n\r]*:[ \t\n\r]*\"([a-f0-9]+)\""
                 _match "${_content}")

    if(_match)
      set(_baseline "${CMAKE_MATCH_1}")
    endif()
  endif()

  set(${RESULT_VAR}
      "${_baseline}"
      PARENT_SCOPE)
endfunction()

# Function to backup vcpkg.json
function(atom_vcpkg_backup_manifest)
  if(EXISTS "${ATOM_VCPKG_JSON_PATH}")
    file(COPY "${ATOM_VCPKG_JSON_PATH}" DESTINATION "${CMAKE_BINARY_DIR}")
    file(RENAME "${CMAKE_BINARY_DIR}/vcpkg.json" "${ATOM_VCPKG_BACKUP_PATH}")
    message(STATUS "Backed up vcpkg.json to ${ATOM_VCPKG_BACKUP_PATH}")
  else()
    message(WARNING "vcpkg.json not found, cannot backup")
  endif()
endfunction()

# Function to restore vcpkg.json from backup
function(atom_vcpkg_restore_manifest)
  if(EXISTS "${ATOM_VCPKG_BACKUP_PATH}")
    file(COPY "${ATOM_VCPKG_BACKUP_PATH}" DESTINATION "${CMAKE_SOURCE_DIR}")
    file(RENAME "${CMAKE_SOURCE_DIR}/vcpkg.json.backup"
         "${ATOM_VCPKG_JSON_PATH}")
    message(STATUS "Restored vcpkg.json from backup")
  else()
    message(WARNING "No backup found at ${ATOM_VCPKG_BACKUP_PATH}")
  endif()
endfunction()

# Function to validate a baseline SHA (check if it exists on GitHub)
function(atom_vcpkg_validate_baseline BASELINE RESULT_VAR)
  set(_valid FALSE)

  if(NOT "${BASELINE}" MATCHES "^[a-f0-9]{40}$")
    message(WARNING "Invalid baseline format: ${BASELINE}")
    set(${RESULT_VAR}
        FALSE
        PARENT_SCOPE)
    return()
  endif()

  # Try to fetch commit info for this specific SHA
  set(_api_url
      "https://api.github.com/repos/microsoft/vcpkg/commits/${BASELINE}")
  set(_temp_file "${CMAKE_BINARY_DIR}/vcpkg_validate.json")

  file(
    DOWNLOAD "${_api_url}" "${_temp_file}"
    TIMEOUT 10
    STATUS _status)

  list(GET _status 0 _error_code)

  if(_error_code EQUAL 0)
    file(READ "${_temp_file}" _response)
    if(_response MATCHES "\"sha\"")
      set(_valid TRUE)
    endif()
  endif()

  if(EXISTS "${_temp_file}")
    file(REMOVE "${_temp_file}")
  endif()

  set(${RESULT_VAR}
      ${_valid}
      PARENT_SCOPE)
endfunction()

# Function to set a specific baseline in vcpkg.json
function(atom_vcpkg_set_baseline NEW_BASELINE)
  if(NOT EXISTS "${ATOM_VCPKG_JSON_PATH}")
    message(FATAL_ERROR "vcpkg.json not found at ${ATOM_VCPKG_JSON_PATH}")
    return()
  endif()

  # Validate baseline format
  if(NOT "${NEW_BASELINE}" MATCHES "^[a-f0-9]{40}$")
    message(FATAL_ERROR "Invalid baseline SHA format: ${NEW_BASELINE}")
    return()
  endif()

  # Backup before modification
  atom_vcpkg_backup_manifest()

  # Read current content
  file(READ "${ATOM_VCPKG_JSON_PATH}" _content)

  # Get current baseline for comparison
  atom_vcpkg_get_current_baseline(_old_baseline)

  if("${_old_baseline}" STREQUAL "${NEW_BASELINE}")
    message(STATUS "Baseline already set to ${NEW_BASELINE}, no change needed")
    return()
  endif()

  # Update baseline
  if(_content MATCHES "\"builtin-baseline\"")
    string(
      REGEX
      REPLACE "\"builtin-baseline\"[ \t\n\r]*:[ \t\n\r]*\"[a-f0-9]*\""
              "\"builtin-baseline\": \"${NEW_BASELINE}\"" _updated
              "${_content}")
  else()
    # Add baseline after opening brace
    string(REGEX
           REPLACE "\\{" "{\n  \"builtin-baseline\": \"${NEW_BASELINE}\","
                   _updated "${_content}")
  endif()

  # Write updated content
  file(WRITE "${ATOM_VCPKG_JSON_PATH}" "${_updated}")

  message(STATUS "Updated baseline: ${_old_baseline} -> ${NEW_BASELINE}")
endfunction()

# Function to update vcpkg.json version
function(atom_vcpkg_update_version NEW_VERSION)
  if(NOT EXISTS "${ATOM_VCPKG_JSON_PATH}")
    message(WARNING "vcpkg.json not found")
    return()
  endif()

  # Validate version format
  if(NOT "${NEW_VERSION}" MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    message(WARNING "Invalid version format: ${NEW_VERSION}")
    return()
  endif()

  file(READ "${ATOM_VCPKG_JSON_PATH}" _content)

  # Update version field
  if(_content MATCHES "\"version\"[ \t\n\r]*:")
    string(REGEX
           REPLACE "\"version\"[ \t\n\r]*:[ \t\n\r]*\"[^\"]*\""
                   "\"version\": \"${NEW_VERSION}\"" _updated "${_content}")

    file(WRITE "${ATOM_VCPKG_JSON_PATH}" "${_updated}")
    message(STATUS "Updated vcpkg.json version to ${NEW_VERSION}")
  else()
    message(WARNING "No version field found in vcpkg.json")
  endif()
endfunction()

# Main function to update to latest baseline
function(atom_vcpkg_update_baseline)
  set(options FORCE NO_CACHE NO_BACKUP VALIDATE)
  set(oneValueArgs BASELINE)
  cmake_parse_arguments(UPD "${options}" "${oneValueArgs}" "" ${ARGN})

  # If specific baseline provided, use it
  if(DEFINED UPD_BASELINE)
    if(UPD_VALIDATE)
      atom_vcpkg_validate_baseline("${UPD_BASELINE}" _is_valid)
      if(NOT _is_valid)
        message(FATAL_ERROR "Baseline validation failed: ${UPD_BASELINE}")
        return()
      endif()
    endif()
    atom_vcpkg_set_baseline("${UPD_BASELINE}")
    return()
  endif()

  # Check cache first (unless FORCE or NO_CACHE)
  if(NOT UPD_FORCE AND NOT UPD_NO_CACHE)
    _atom_vcpkg_cache_valid(_cache_valid)
    if(_cache_valid)
      _atom_vcpkg_read_cache(_cached_baseline)
      if(NOT "${_cached_baseline}" STREQUAL "")
        message(STATUS "Using cached baseline: ${_cached_baseline}")
        atom_vcpkg_set_baseline("${_cached_baseline}")
        return()
      endif()
    endif()
  endif()

  # Fetch from GitHub
  _atom_vcpkg_fetch_baseline(_new_baseline _fetch_success)

  if(_fetch_success)
    # Validate if requested
    if(UPD_VALIDATE)
      atom_vcpkg_validate_baseline("${_new_baseline}" _is_valid)
      if(NOT _is_valid)
        message(WARNING "Fetched baseline failed validation, using default")
        set(_new_baseline "${ATOM_VCPKG_DEFAULT_BASELINE}")
      endif()
    endif()

    # Update cache
    if(NOT UPD_NO_CACHE)
      _atom_vcpkg_write_cache("${_new_baseline}")
    endif()

    # Backup if not disabled
    if(NOT UPD_NO_BACKUP)
      atom_vcpkg_backup_manifest()
    endif()

    atom_vcpkg_set_baseline("${_new_baseline}")
  else()
    message(
      WARNING
        "Failed to fetch baseline, using default: ${ATOM_VCPKG_DEFAULT_BASELINE}"
    )
    atom_vcpkg_set_baseline("${ATOM_VCPKG_DEFAULT_BASELINE}")
  endif()
endfunction()

# Function to print baseline status
function(atom_vcpkg_print_baseline_info)
  message(STATUS "")
  message(STATUS "=== vcpkg Baseline Info ===")

  atom_vcpkg_get_current_baseline(_current)
  if("${_current}" STREQUAL "")
    message(STATUS "Current:  (not set)")
  else()
    message(STATUS "Current:  ${_current}")
  endif()

  message(STATUS "Default:  ${ATOM_VCPKG_DEFAULT_BASELINE}")

  _atom_vcpkg_cache_valid(_cache_valid)
  if(_cache_valid)
    _atom_vcpkg_read_cache(_cached)
    message(STATUS "Cached:   ${_cached}")
  else()
    message(STATUS "Cached:   (expired or not available)")
  endif()

  if(EXISTS "${ATOM_VCPKG_BACKUP_PATH}")
    message(STATUS "Backup:   Available")
  else()
    message(STATUS "Backup:   Not available")
  endif()

  message(STATUS "===========================")
  message(STATUS "")
endfunction()

# =============================================================================
# Legacy Compatibility
# =============================================================================

# Legacy function name for backward compatibility
function(update_vcpkg_baseline)
  atom_vcpkg_update_baseline(${ARGN})
endfunction()

# =============================================================================
# Auto-execute if UPDATE_VCPKG_BASELINE is set
# =============================================================================

if(UPDATE_VCPKG_BASELINE)
  message(STATUS "UPDATE_VCPKG_BASELINE is ON, updating baseline...")
  atom_vcpkg_update_baseline()
  atom_vcpkg_print_baseline_info()
endif()
