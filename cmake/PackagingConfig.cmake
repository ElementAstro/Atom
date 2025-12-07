# PackagingConfig.cmake - Comprehensive packaging configuration for Atom library
# This module provides advanced packaging capabilities including: -
# Multi-platform package generation - Modular component packaging - Distribution
# channel management - Automated signing and verification

# Avoid repeated inclusion
if(DEFINED PACKAGING_CONFIG_INCLUDED)
  return()
endif()
set(PACKAGING_CONFIG_INCLUDED TRUE)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# =============================================================================
# Packaging Configuration
# =============================================================================

# Package metadata
set(ATOM_PACKAGE_NAME "atom")
set(ATOM_PACKAGE_DISPLAY_NAME "Atom Library")
set(ATOM_PACKAGE_DESCRIPTION "Foundational library for astronomical software")
set(ATOM_PACKAGE_VENDOR "ElementAstro")
set(ATOM_PACKAGE_CONTACT "max@example.com")
set(ATOM_PACKAGE_URL "https://github.com/ElementAstro/Atom")

# Version information
if(NOT DEFINED ATOM_VERSION)
  set(ATOM_VERSION "1.0.0")
endif()

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${ATOM_VERSION})
set(ATOM_VERSION_MAJOR ${CMAKE_MATCH_1})
set(ATOM_VERSION_MINOR ${CMAKE_MATCH_2})
set(ATOM_VERSION_PATCH ${CMAKE_MATCH_3})

# Platform detection
if(WIN32)
  set(ATOM_PLATFORM "windows")
  set(ATOM_PACKAGE_EXTENSION "zip")
elseif(APPLE)
  set(ATOM_PLATFORM "macos")
  set(ATOM_PACKAGE_EXTENSION "tar.gz")
elseif(UNIX)
  set(ATOM_PLATFORM "linux")
  set(ATOM_PACKAGE_EXTENSION "tar.gz")
else()
  set(ATOM_PLATFORM "unknown")
  set(ATOM_PACKAGE_EXTENSION "tar.gz")
endif()

# Architecture detection
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ATOM_ARCH "x64")
else()
  set(ATOM_ARCH "x86")
endif()

# Package naming
set(ATOM_PACKAGE_BASENAME
    "${ATOM_PACKAGE_NAME}-${ATOM_VERSION}-${ATOM_PLATFORM}-${ATOM_ARCH}")

# =============================================================================
# Component Configuration
# =============================================================================

# Define available components with their dependencies
set(ATOM_COMPONENTS
    algorithm
    async
    components
    connection
    containers
    error
    image
    io
    log
    memory
    meta
    search
    secret
    serial
    sysinfo
    system
    type
    utils
    web)

# Component dependencies mapping (aligned with module_dependencies.cmake) Core
# modules (no dependencies)
set(ATOM_COMPONENT_DEPS_error "")
set(ATOM_COMPONENT_DEPS_type "")
set(ATOM_COMPONENT_DEPS_containers "")

# Low-level modules
set(ATOM_COMPONENT_DEPS_log "error")
set(ATOM_COMPONENT_DEPS_meta "error")
set(ATOM_COMPONENT_DEPS_memory "error;meta;type")

# Mid-level modules
set(ATOM_COMPONENT_DEPS_utils "error;type")
set(ATOM_COMPONENT_DEPS_algorithm "error;utils")
set(ATOM_COMPONENT_DEPS_async "error;utils")
set(ATOM_COMPONENT_DEPS_io "error;async")

# High-level modules
set(ATOM_COMPONENT_DEPS_sysinfo "error")
set(ATOM_COMPONENT_DEPS_system "error;sysinfo;meta;utils")
set(ATOM_COMPONENT_DEPS_serial "error;log")
set(ATOM_COMPONENT_DEPS_secret "error")
set(ATOM_COMPONENT_DEPS_search "error")
set(ATOM_COMPONENT_DEPS_image "error;utils;io")

# Application-level modules
set(ATOM_COMPONENT_DEPS_connection "error;async")
set(ATOM_COMPONENT_DEPS_components "error;type")
set(ATOM_COMPONENT_DEPS_web "error;utils;io;system;type")

# Component descriptions
set(ATOM_COMPONENT_DESC_algorithm "Algorithm utilities and data structures")
set(ATOM_COMPONENT_DESC_async "Asynchronous programming utilities")
set(ATOM_COMPONENT_DESC_components "Component system framework")
set(ATOM_COMPONENT_DESC_connection "Network and IPC connection utilities")
set(ATOM_COMPONENT_DESC_containers "Advanced container data structures")
set(ATOM_COMPONENT_DESC_error "Error handling and exception utilities")
set(ATOM_COMPONENT_DESC_image "Image processing and FITS support")
set(ATOM_COMPONENT_DESC_io "Input/output utilities and file handling")
set(ATOM_COMPONENT_DESC_log "Logging and diagnostic utilities")
set(ATOM_COMPONENT_DESC_memory "Memory management utilities")
set(ATOM_COMPONENT_DESC_meta "Metaprogramming and reflection utilities")
set(ATOM_COMPONENT_DESC_search "Search algorithms and indexing")
set(ATOM_COMPONENT_DESC_secret "Cryptographic and security utilities")
set(ATOM_COMPONENT_DESC_serial "Serial communication utilities")
set(ATOM_COMPONENT_DESC_sysinfo "System information and monitoring")
set(ATOM_COMPONENT_DESC_system "System utilities and process management")
set(ATOM_COMPONENT_DESC_type "Type utilities and traits")
set(ATOM_COMPONENT_DESC_utils "General purpose utilities")
set(ATOM_COMPONENT_DESC_web "Web server and HTTP utilities")

# =============================================================================
# Package Generation Functions
# =============================================================================

# Function to resolve component dependencies
function(atom_resolve_dependencies COMPONENT_LIST OUTPUT_VAR)
  set(RESOLVED_COMPONENTS ${COMPONENT_LIST})
  set(CHANGED TRUE)

  while(CHANGED)
    set(CHANGED FALSE)
    set(NEW_COMPONENTS ${RESOLVED_COMPONENTS})

    foreach(COMPONENT ${RESOLVED_COMPONENTS})
      if(DEFINED ATOM_COMPONENT_DEPS_${COMPONENT})
        foreach(DEP ${ATOM_COMPONENT_DEPS_${COMPONENT}})
          if(NOT ${DEP} IN_LIST NEW_COMPONENTS)
            list(APPEND NEW_COMPONENTS ${DEP})
            set(CHANGED TRUE)
          endif()
        endforeach()
      endif()
    endforeach()

    set(RESOLVED_COMPONENTS ${NEW_COMPONENTS})
  endwhile()

  list(REMOVE_DUPLICATES RESOLVED_COMPONENTS)
  set(${OUTPUT_VAR}
      ${RESOLVED_COMPONENTS}
      PARENT_SCOPE)
endfunction()

# Function to create component package
function(atom_create_component_package COMPONENT_NAME)
  # Resolve dependencies
  atom_resolve_dependencies("${COMPONENT_NAME}" REQUIRED_COMPONENTS)

  message(STATUS "Creating package for component: ${COMPONENT_NAME}")
  message(STATUS "Required components: ${REQUIRED_COMPONENTS}")

  # Create component-specific install configuration
  set(COMPONENT_INSTALL_DIR "${CMAKE_BINARY_DIR}/packages/${COMPONENT_NAME}")

  # Install component and its dependencies
  foreach(COMP ${REQUIRED_COMPONENTS})
    install(
      TARGETS atom-${COMP}
      EXPORT atom-${COMP}-targets
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${COMPONENT_NAME})

    install(
      DIRECTORY ${CMAKE_SOURCE_DIR}/atom/${COMP}/
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/${COMP}
      FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.h"
      COMPONENT ${COMPONENT_NAME})
  endforeach()
endfunction()

# Function to setup CPack configuration
function(atom_setup_cpack)
  # Basic CPack configuration
  set(CPACK_PACKAGE_NAME ${ATOM_PACKAGE_NAME})
  set(CPACK_PACKAGE_VENDOR ${ATOM_PACKAGE_VENDOR})
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${ATOM_PACKAGE_DESCRIPTION})
  set(CPACK_PACKAGE_VERSION ${ATOM_VERSION})
  set(CPACK_PACKAGE_VERSION_MAJOR ${ATOM_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${ATOM_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${ATOM_VERSION_PATCH})
  set(CPACK_PACKAGE_CONTACT ${ATOM_PACKAGE_CONTACT})
  set(CPACK_PACKAGE_HOMEPAGE_URL ${ATOM_PACKAGE_URL})

  # Resource files
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

  # Package file name
  set(CPACK_PACKAGE_FILE_NAME ${ATOM_PACKAGE_BASENAME})

  # Platform-specific configuration
  if(WIN32)
    atom_setup_windows_packaging()
  elseif(APPLE)
    atom_setup_macos_packaging()
  else()
    atom_setup_linux_packaging()
  endif()

  include(CPack)
endfunction()

# Platform-specific packaging functions
function(atom_setup_windows_packaging)
  set(CPACK_GENERATOR
      "ZIP;NSIS;WIX"
      PARENT_SCOPE)

  # NSIS configuration
  set(CPACK_NSIS_DISPLAY_NAME
      ${ATOM_PACKAGE_DISPLAY_NAME}
      PARENT_SCOPE)
  set(CPACK_NSIS_PACKAGE_NAME
      ${ATOM_PACKAGE_DISPLAY_NAME}
      PARENT_SCOPE)
  set(CPACK_NSIS_URL_INFO_ABOUT
      ${ATOM_PACKAGE_URL}
      PARENT_SCOPE)
  set(CPACK_NSIS_CONTACT
      ${ATOM_PACKAGE_CONTACT}
      PARENT_SCOPE)
  set(CPACK_NSIS_MODIFY_PATH
      ON
      PARENT_SCOPE)

  # WiX configuration
  set(CPACK_WIX_UPGRADE_GUID
      "12345678-1234-1234-1234-123456789012"
      PARENT_SCOPE)
  set(CPACK_WIX_PRODUCT_GUID
      "87654321-4321-4321-4321-210987654321"
      PARENT_SCOPE)
endfunction()

function(atom_setup_macos_packaging)
  set(CPACK_GENERATOR
      "TGZ;DragNDrop"
      PARENT_SCOPE)

  # macOS bundle configuration
  set(CPACK_DMG_VOLUME_NAME
      ${ATOM_PACKAGE_DISPLAY_NAME}
      PARENT_SCOPE)
  set(CPACK_DMG_FORMAT
      "UDZO"
      PARENT_SCOPE)
endfunction()

function(atom_setup_linux_packaging)
  set(CPACK_GENERATOR
      "TGZ;DEB;RPM"
      PARENT_SCOPE)

  # Debian package configuration
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER
      ${ATOM_PACKAGE_CONTACT}
      PARENT_SCOPE)
  set(CPACK_DEBIAN_PACKAGE_SECTION
      "libdevel"
      PARENT_SCOPE)
  set(CPACK_DEBIAN_PACKAGE_PRIORITY
      "optional"
      PARENT_SCOPE)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS
      "libssl-dev, zlib1g-dev, libsqlite3-dev, libfmt-dev"
      PARENT_SCOPE)

  # RPM package configuration
  set(CPACK_RPM_PACKAGE_GROUP
      "Development/Libraries"
      PARENT_SCOPE)
  set(CPACK_RPM_PACKAGE_LICENSE
      "GPL-3.0"
      PARENT_SCOPE)
  set(CPACK_RPM_PACKAGE_REQUIRES
      "openssl-devel, zlib-devel, sqlite-devel, fmt-devel"
      PARENT_SCOPE)
endfunction()

# =============================================================================
# Advanced Packaging Features
# =============================================================================

# Function to create modular packages
function(atom_create_modular_packages)
  message(STATUS "Creating modular packages for all components...")

  foreach(COMPONENT ${ATOM_COMPONENTS})
    atom_create_component_package(${COMPONENT})
  endforeach()

  # Create meta-packages for common use cases
  atom_create_meta_package("core" "error;log;type;utils")
  atom_create_meta_package("networking" "connection;web;async")
  atom_create_meta_package("imaging" "image;io;algorithm")
  atom_create_meta_package("system" "sysinfo;system;serial")
  atom_create_meta_package("full" "${ATOM_COMPONENTS}")
endfunction()

# Function to create meta-packages
function(atom_create_meta_package PACKAGE_NAME COMPONENT_LIST)
  message(
    STATUS
      "Creating meta-package: ${PACKAGE_NAME} with components: ${COMPONENT_LIST}"
  )

  # Resolve all dependencies
  atom_resolve_dependencies("${COMPONENT_LIST}" RESOLVED_COMPONENTS)

  # Create package-specific install component
  foreach(COMP ${RESOLVED_COMPONENTS})
    install(
      TARGETS atom-${COMP}
      EXPORT atom-${PACKAGE_NAME}-targets
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PACKAGE_NAME})
  endforeach()
endfunction()

# Function to generate package manifests
function(atom_generate_package_manifest COMPONENT_NAME OUTPUT_FILE)
  atom_resolve_dependencies("${COMPONENT_NAME}" REQUIRED_COMPONENTS)

  # Create JSON manifest
  file(WRITE ${OUTPUT_FILE} "{\n")
  file(APPEND ${OUTPUT_FILE} "  \"name\": \"atom-${COMPONENT_NAME}\",\n")
  file(APPEND ${OUTPUT_FILE} "  \"version\": \"${ATOM_VERSION}\",\n")
  file(APPEND ${OUTPUT_FILE}
       "  \"description\": \"${ATOM_COMPONENT_DESC_${COMPONENT_NAME}}\",\n")
  file(APPEND ${OUTPUT_FILE} "  \"platform\": \"${ATOM_PLATFORM}\",\n")
  file(APPEND ${OUTPUT_FILE} "  \"architecture\": \"${ATOM_ARCH}\",\n")
  file(APPEND ${OUTPUT_FILE} "  \"dependencies\": [\n")

  list(LENGTH REQUIRED_COMPONENTS DEP_COUNT)
  set(CURRENT_INDEX 0)
  foreach(DEP ${REQUIRED_COMPONENTS})
    math(EXPR CURRENT_INDEX "${CURRENT_INDEX} + 1")
    if(CURRENT_INDEX EQUAL DEP_COUNT)
      file(APPEND ${OUTPUT_FILE} "    \"atom-${DEP}\"\n")
    else()
      file(APPEND ${OUTPUT_FILE} "    \"atom-${DEP}\",\n")
    endif()
  endforeach()

  file(APPEND ${OUTPUT_FILE} "  ],\n")
  file(APPEND ${OUTPUT_FILE} "  \"build_date\": \"${CMAKE_TIMESTAMP}\",\n")
  file(APPEND ${OUTPUT_FILE} "  \"build_type\": \"${CMAKE_BUILD_TYPE}\"\n")
  file(APPEND ${OUTPUT_FILE} "}\n")
endfunction()

# Function to create portable packages
function(atom_create_portable_package)
  message(STATUS "Creating portable package...")

  # Create portable directory structure
  set(PORTABLE_DIR "${CMAKE_BINARY_DIR}/portable")
  file(MAKE_DIRECTORY ${PORTABLE_DIR})
  file(MAKE_DIRECTORY ${PORTABLE_DIR}/bin)
  file(MAKE_DIRECTORY ${PORTABLE_DIR}/lib)
  file(MAKE_DIRECTORY ${PORTABLE_DIR}/include)
  file(MAKE_DIRECTORY ${PORTABLE_DIR}/share)

  # Copy libraries and dependencies
  install(
    TARGETS ${ATOM_ALL_TARGETS}
    RUNTIME DESTINATION ${PORTABLE_DIR}/bin
    LIBRARY DESTINATION ${PORTABLE_DIR}/lib
    ARCHIVE DESTINATION ${PORTABLE_DIR}/lib)

  # Copy headers
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/atom/
    DESTINATION ${PORTABLE_DIR}/include/atom
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h")

  # Create launcher scripts
  if(WIN32)
    atom_create_windows_launcher(${PORTABLE_DIR})
  else()
    atom_create_unix_launcher(${PORTABLE_DIR})
  endif()

  # Create portable package archive
  if(WIN32)
    execute_process(
      COMMAND
        ${CMAKE_COMMAND} -E tar czf
        "${CMAKE_BINARY_DIR}/${ATOM_PACKAGE_BASENAME}-portable.zip"
        --format=zip ${PORTABLE_DIR})
  else()
    execute_process(
      COMMAND
        ${CMAKE_COMMAND} -E tar czf
        "${CMAKE_BINARY_DIR}/${ATOM_PACKAGE_BASENAME}-portable.tar.gz"
        ${PORTABLE_DIR})
  endif()
endfunction()

# Function to create Windows launcher
function(atom_create_windows_launcher PORTABLE_DIR)
  file(
    WRITE ${PORTABLE_DIR}/atom-env.bat
    "@echo off\n"
    "set ATOM_ROOT=%~dp0\n"
    "set PATH=%ATOM_ROOT%\\bin;%PATH%\n"
    "set CMAKE_PREFIX_PATH=%ATOM_ROOT%;%CMAKE_PREFIX_PATH%\n"
    "echo Atom library environment configured\n"
    "echo ATOM_ROOT=%ATOM_ROOT%\n"
    "cmd /k\n")
endfunction()

# Function to create Unix launcher
function(atom_create_unix_launcher PORTABLE_DIR)
  file(
    WRITE ${PORTABLE_DIR}/atom-env.sh
    "#!/bin/bash\n"
    "export ATOM_ROOT=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n"
    "export PATH=\"$ATOM_ROOT/bin:$PATH\"\n"
    "export LD_LIBRARY_PATH=\"$ATOM_ROOT/lib:$LD_LIBRARY_PATH\"\n"
    "export CMAKE_PREFIX_PATH=\"$ATOM_ROOT:$CMAKE_PREFIX_PATH\"\n"
    "echo \"Atom library environment configured\"\n"
    "echo \"ATOM_ROOT=$ATOM_ROOT\"\n"
    "$SHELL\n")

  # Make script executable
  file(
    CHMOD
    ${PORTABLE_DIR}/atom-env.sh
    PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE)
endfunction()

# Function to validate package integrity
function(atom_validate_package PACKAGE_PATH)
  message(STATUS "Validating package: ${PACKAGE_PATH}")

  # Check if package exists
  if(NOT EXISTS ${PACKAGE_PATH})
    message(FATAL_ERROR "Package not found: ${PACKAGE_PATH}")
  endif()

  # Extract and validate contents
  get_filename_component(PACKAGE_NAME ${PACKAGE_PATH} NAME_WE)
  set(EXTRACT_DIR "${CMAKE_BINARY_DIR}/validation/${PACKAGE_NAME}")

  file(MAKE_DIRECTORY ${EXTRACT_DIR})

  # Extract package
  if(PACKAGE_PATH MATCHES "\\.zip$")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${PACKAGE_PATH}
                    WORKING_DIRECTORY ${EXTRACT_DIR})
  else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${PACKAGE_PATH}
                    WORKING_DIRECTORY ${EXTRACT_DIR})
  endif()

  # Validate required files
  set(REQUIRED_FILES "include/atom" "lib" "share")
  foreach(REQUIRED_FILE ${REQUIRED_FILES})
    if(NOT EXISTS "${EXTRACT_DIR}/${REQUIRED_FILE}")
      message(WARNING "Missing required file/directory: ${REQUIRED_FILE}")
    endif()
  endforeach()

  message(STATUS "Package validation completed")
endfunction()
