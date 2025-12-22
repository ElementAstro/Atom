# =============================================================================
# VersionConfig.cmake - Project version configuration
# =============================================================================
# This module configures the project version from Git and generates version
# headers for use in the build.
#
# This file works in conjunction with: - cmake/GitVersion.cmake     - Git
# version extraction functions - cmake/version.h.in         - Unified version
# header template - scripts/version/*          - Version management scripts
#
# Author: Max Qian License: GPL3
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# Version Configuration
# =============================================================================

# Configure version from Git (uses unified template version.h.in)
atom_configure_version_from_git(
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/atom_version.h" VERSION_VARIABLE
  ATOM_VERSION PREFIX "ATOM")

# Update project version with Git version
if(DEFINED ATOM_VERSION AND NOT "${ATOM_VERSION}" STREQUAL "")
  set(PROJECT_VERSION ${ATOM_VERSION})
  message(STATUS "Using Git-derived version: ${PROJECT_VERSION}")
else()
  message(STATUS "Using default project version: ${PROJECT_VERSION}")
endif()

# =============================================================================
# Compile Definitions
# =============================================================================

# Pass version information as definitions to all targets
add_compile_definitions(ATOM_VERSION="${PROJECT_VERSION}"
                        ATOM_VERSION_STRING="${PROJECT_VERSION}")

# =============================================================================
# Include Directories
# =============================================================================

# Ensure the generated version header is included in builds
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# =============================================================================
# Scripts Integration (Optional)
# =============================================================================

# Export version info for scripts/version/* integration This creates a file that
# scripts can source to get version info
if(COMMAND atom_export_version_info)
  atom_export_version_info("${CMAKE_CURRENT_BINARY_DIR}/version_info.env")
endif()

# =============================================================================
# Summary
# =============================================================================

message(STATUS "Atom project version configured to: ${PROJECT_VERSION}")
