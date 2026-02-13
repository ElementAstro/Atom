# =============================================================================
# GitVersion.cmake - Git-based version configuration
# =============================================================================
# This module provides functions to extract version information from Git tags
# and commits, generating version headers for the project.
#
# Main functions: atom_configure_version_from_git() - Extract version from Git
# and generate header atom_configure_version()          - Configure both version
# files
#
# Variables set by this module: PROJECT_VERSION,
# PROJECT_VERSION_MAJOR/MINOR/PATCH GIT_HASH, GIT_BRANCH, GIT_TAG,
# GIT_COMMIT_COUNT, GIT_DIRTY_RESULT BUILD_TIME
#
# Compatible with: scripts/version/version-manager.sh,
# scripts/version/version.ps1
#
# Author: Max Qian License: GPL3
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# Helper Functions
# =============================================================================

# Function to get current timestamp in ISO 8601 format
function(_atom_get_build_time OUTPUT_VAR)
  string(TIMESTAMP _timestamp "%Y-%m-%dT%H:%M:%S")
  set(${OUTPUT_VAR}
      "${_timestamp}"
      PARENT_SCOPE)
endfunction()

# Function to safely execute a Git command with fallback
function(_atom_git_command OUTPUT_VAR DEFAULT_VALUE)
  set(_result "${DEFAULT_VALUE}")
  if(GIT_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} ${ARGN}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE _cmd_result
      OUTPUT_VARIABLE _cmd_output
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_cmd_result EQUAL 0 AND NOT "${_cmd_output}" STREQUAL "")
      set(_result "${_cmd_output}")
    endif()
  endif()
  set(${OUTPUT_VAR}
      "${_result}"
      PARENT_SCOPE)
endfunction()

# =============================================================================
# Main Version Configuration Function
# =============================================================================

# Function to configure version from Git repository This function extracts
# comprehensive version information from Git and generates version headers
# compatible with scripts/version/*.
function(atom_configure_version_from_git)
  # Parse arguments
  set(options "")
  set(oneValueArgs OUTPUT_HEADER VERSION_VARIABLE PREFIX)
  set(multiValueArgs "")
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  # Set default values
  if(NOT DEFINED ARG_OUTPUT_HEADER)
    set(ARG_OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")
  endif()

  if(NOT DEFINED ARG_VERSION_VARIABLE)
    set(ARG_VERSION_VARIABLE PROJECT_VERSION)
  endif()

  if(NOT DEFINED ARG_PREFIX)
    set(ARG_PREFIX "${PROJECT_NAME}")
  endif()

  # Initialize default values (used when Git is not available)
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 0)
  set(VERSION_PATCH 0)
  set(VERSION_STRING "0.0.0-unknown")
  set(GIT_HASH "unknown")
  set(GIT_BRANCH "unknown")
  set(GIT_TAG "")
  set(GIT_COMMIT_COUNT 0)
  set(GIT_DIRTY_RESULT 0)

  # Get build timestamp
  _atom_get_build_time(BUILD_TIME)

  # Get Git information
  find_package(Git QUIET)
  if(GIT_FOUND)
    # Check if in a Git repository
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE GIT_REPO_CHECK
      OUTPUT_QUIET ERROR_QUIET)

    if(GIT_REPO_CHECK EQUAL 0)
      # Get the current commit short hash
      _atom_git_command(GIT_HASH "unknown" rev-parse --short HEAD)

      # Get the current branch name
      _atom_git_command(GIT_BRANCH "unknown" rev-parse --abbrev-ref HEAD)

      # Get the most recent tag
      execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_TAG_RESULT
        OUTPUT_VARIABLE GIT_TAG
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

      if(NOT GIT_TAG_RESULT EQUAL 0)
        set(GIT_TAG "")
      endif()

      # Get the number of commits since the most recent tag (or total if no tag)
      if(NOT "${GIT_TAG}" STREQUAL "")
        _atom_git_command(GIT_COMMIT_COUNT "0" rev-list --count
                          ${GIT_TAG}..HEAD)
      else()
        _atom_git_command(GIT_COMMIT_COUNT "0" rev-list --count HEAD)
      endif()

      # Check if the working directory is clean
      execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIRTY_RESULT)

      # Build version string from tag or VERSION file
      if(NOT "${GIT_TAG}" STREQUAL "")
        # Parse tag version number (assuming format vX.Y.Z or X.Y.Z)
        string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH
                     "${GIT_TAG}")
        if(VERSION_MATCH)
          set(VERSION_MAJOR ${CMAKE_MATCH_1})
          set(VERSION_MINOR ${CMAKE_MATCH_2})
          set(VERSION_PATCH ${CMAKE_MATCH_3})
        endif()
      endif()

      # Try to read from VERSION file if no valid tag
      if(VERSION_MAJOR EQUAL 0
         AND VERSION_MINOR EQUAL 0
         AND VERSION_PATCH EQUAL 0)
        set(VERSION_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt")
        if(EXISTS "${VERSION_FILE_PATH}")
          file(READ "${VERSION_FILE_PATH}" VERSION_FILE_CONTENT)
          string(STRIP "${VERSION_FILE_CONTENT}" VERSION_FILE_CONTENT)
          string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH
                       "${VERSION_FILE_CONTENT}")
          if(VERSION_MATCH)
            set(VERSION_MAJOR ${CMAKE_MATCH_1})
            set(VERSION_MINOR ${CMAKE_MATCH_2})
            set(VERSION_PATCH ${CMAKE_MATCH_3})
            message(
              STATUS "Read version from VERSION file: ${VERSION_FILE_CONTENT}")
          endif()
        endif()
      endif()

      # Build final version string
      set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

      message(
        STATUS
          "Git version: ${VERSION_STRING} (Hash: ${GIT_HASH}, Branch: ${GIT_BRANCH}, Tag: ${GIT_TAG}, Commits: ${GIT_COMMIT_COUNT}, Dirty: ${GIT_DIRTY_RESULT})"
      )
    else()
      message(
        WARNING
          "Current directory is not a Git repository, using default version")
    endif()
  else()
    message(WARNING "Git not found, using default version")
  endif()

  # Set variables in parent scope
  set(${ARG_VERSION_VARIABLE}
      "${VERSION_STRING}"
      PARENT_SCOPE)
  set(PROJECT_VERSION
      "${VERSION_STRING}"
      PARENT_SCOPE)
  set(PROJECT_VERSION_MAJOR
      ${VERSION_MAJOR}
      PARENT_SCOPE)
  set(PROJECT_VERSION_MINOR
      ${VERSION_MINOR}
      PARENT_SCOPE)
  set(PROJECT_VERSION_PATCH
      ${VERSION_PATCH}
      PARENT_SCOPE)
  set(GIT_HASH
      "${GIT_HASH}"
      PARENT_SCOPE)
  set(GIT_BRANCH
      "${GIT_BRANCH}"
      PARENT_SCOPE)
  set(GIT_TAG
      "${GIT_TAG}"
      PARENT_SCOPE)
  set(GIT_COMMIT_COUNT
      ${GIT_COMMIT_COUNT}
      PARENT_SCOPE)
  set(GIT_DIRTY_RESULT
      ${GIT_DIRTY_RESULT}
      PARENT_SCOPE)
  set(BUILD_TIME
      "${BUILD_TIME}"
      PARENT_SCOPE)

  # Generate version header file using the unified template
  set(VERSION_TEMPLATE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in")
  if(EXISTS "${VERSION_TEMPLATE_PATH}")
    configure_file("${VERSION_TEMPLATE_PATH}" "${ARG_OUTPUT_HEADER}" @ONLY)
    message(STATUS "Generated version header: ${ARG_OUTPUT_HEADER}")
  else()
    message(WARNING "Version template not found: ${VERSION_TEMPLATE_PATH}")
  endif()
endfunction()

# =============================================================================
# Unified Version Configuration Function
# =============================================================================

# Function to configure all version files at once This is the recommended entry
# point for version configuration
function(atom_configure_version)
  # Parse arguments
  set(options "")
  set(oneValueArgs VERSION_VARIABLE OUTPUT_DIR)
  set(multiValueArgs "")
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(NOT DEFINED ARG_VERSION_VARIABLE)
    set(ARG_VERSION_VARIABLE ATOM_VERSION)
  endif()

  if(NOT DEFINED ARG_OUTPUT_DIR)
    set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  endif()

  # Generate the main version header with Git info
  atom_configure_version_from_git(
    OUTPUT_HEADER "${ARG_OUTPUT_DIR}/atom_version.h" VERSION_VARIABLE
    ${ARG_VERSION_VARIABLE} PREFIX "ATOM")

  # Export version to parent scope
  set(${ARG_VERSION_VARIABLE}
      "${${ARG_VERSION_VARIABLE}}"
      PARENT_SCOPE)
  set(PROJECT_VERSION
      "${PROJECT_VERSION}"
      PARENT_SCOPE)
  set(PROJECT_VERSION_MAJOR
      "${PROJECT_VERSION_MAJOR}"
      PARENT_SCOPE)
  set(PROJECT_VERSION_MINOR
      "${PROJECT_VERSION_MINOR}"
      PARENT_SCOPE)
  set(PROJECT_VERSION_PATCH
      "${PROJECT_VERSION_PATCH}"
      PARENT_SCOPE)

  message(STATUS "Version configuration complete: ${PROJECT_VERSION}")
endfunction()

# =============================================================================
# Utility Functions for Scripts Integration
# =============================================================================

# Function to export version info to a format compatible with scripts/version/*
# This allows the build system and scripts to share version information
function(atom_export_version_info OUTPUT_FILE)
  set(_content "# Auto-generated version info for scripts integration\n")
  string(APPEND _content "# Generated by CMake at configure time\n")
  string(APPEND _content "VERSION=${PROJECT_VERSION}\n")
  string(APPEND _content "VERSION_MAJOR=${PROJECT_VERSION_MAJOR}\n")
  string(APPEND _content "VERSION_MINOR=${PROJECT_VERSION_MINOR}\n")
  string(APPEND _content "VERSION_PATCH=${PROJECT_VERSION_PATCH}\n")
  string(APPEND _content "GIT_HASH=${GIT_HASH}\n")
  string(APPEND _content "GIT_BRANCH=${GIT_BRANCH}\n")
  string(APPEND _content "GIT_TAG=${GIT_TAG}\n")
  string(APPEND _content "GIT_COMMIT_COUNT=${GIT_COMMIT_COUNT}\n")
  string(APPEND _content "GIT_DIRTY=${GIT_DIRTY_RESULT}\n")
  string(APPEND _content "BUILD_TIME=${BUILD_TIME}\n")
  string(APPEND _content "BUILD_TYPE=${CMAKE_BUILD_TYPE}\n")
  string(APPEND _content "COMPILER_ID=${CMAKE_CXX_COMPILER_ID}\n")
  string(APPEND _content "COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}\n")
  string(APPEND _content "SYSTEM_NAME=${CMAKE_SYSTEM_NAME}\n")
  string(APPEND _content "SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}\n")

  file(WRITE "${OUTPUT_FILE}" "${_content}")
  message(STATUS "Exported version info to: ${OUTPUT_FILE}")
endfunction()

# Function to print version summary
function(atom_print_version_info)
  message(STATUS "")
  message(STATUS "=== Atom Version Information ===")
  message(STATUS "Version:     ${PROJECT_VERSION}")
  message(STATUS "Git Hash:    ${GIT_HASH}")
  message(STATUS "Git Branch:  ${GIT_BRANCH}")
  message(STATUS "Git Tag:     ${GIT_TAG}")
  message(STATUS "Commits:     ${GIT_COMMIT_COUNT}")
  message(STATUS "Dirty:       ${GIT_DIRTY_RESULT}")
  message(STATUS "Build Time:  ${BUILD_TIME}")
  message(STATUS "Build Type:  ${CMAKE_BUILD_TYPE}")
  message(
    STATUS "Compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
  )
  message(STATUS "System:      ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
  message(STATUS "================================")
  message(STATUS "")
endfunction()
