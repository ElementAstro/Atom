# =============================================================================
# Module Dependency Configuration
# =============================================================================
# This file defines the dependencies between modules in the Atom project. When a
# module is enabled, its dependencies are automatically enabled too.
#
# Format: set(ATOM_<MODULE>_DEPENDS <list of dependent modules>)
#
# The dependency system ensures that modules are built in the correct order and
# that all required dependencies are satisfied.
#
# Author: Max Qian License: GPL3
# =============================================================================

# Avoid repeated inclusion
if(DEFINED MODULE_DEPENDENCIES_DATA_INCLUDED)
  return()
endif()
set(MODULE_DEPENDENCIES_DATA_INCLUDED TRUE)

# =============================================================================
# Core Module Dependencies (no dependencies)
# =============================================================================

# Error handling module - foundation of all error handling
set(ATOM_ATOM_ERROR_DEPENDS "")

# Type module - basic type utilities (header-only, minimal deps)
set(ATOM_ATOM_TYPE_DEPENDS "")

# Containers module - advanced containers (header-only)
set(ATOM_ATOM_CONTAINERS_DEPENDS "")

# =============================================================================
# Low-Level Module Dependencies
# =============================================================================

# Log module - logging utilities
set(ATOM_ATOM_LOG_DEPENDS atom-error)

# Meta module - metaprogramming utilities
set(ATOM_ATOM_META_DEPENDS atom-error)

# Memory module - memory management
set(ATOM_ATOM_MEMORY_DEPENDS atom-error atom-meta atom-type)

# =============================================================================
# Mid-Level Module Dependencies
# =============================================================================

# Utils module - general utilities
set(ATOM_ATOM_UTILS_DEPENDS atom-error atom-type)

# Algorithm module - algorithm utilities
set(ATOM_ATOM_ALGORITHM_DEPENDS atom-error atom-utils)

# IO module - input/output utilities
set(ATOM_ATOM_IO_DEPENDS atom-error atom-async)

# Async module - asynchronous programming
set(ATOM_ATOM_ASYNC_DEPENDS atom-error atom-utils)

# =============================================================================
# High-Level Module Dependencies
# =============================================================================

# System module - system utilities
set(ATOM_ATOM_SYSTEM_DEPENDS atom-error atom-sysinfo atom-meta atom-utils)

# Sysinfo module - system information
set(ATOM_ATOM_SYSINFO_DEPENDS atom-error)

# Serial module - serial communication
set(ATOM_ATOM_SERIAL_DEPENDS atom-error atom-log)

# Secret module - cryptographic utilities
set(ATOM_ATOM_SECRET_DEPENDS atom-error)

# Search module - search and database utilities
set(ATOM_ATOM_SEARCH_DEPENDS atom-error)

# Image module - image processing
set(ATOM_ATOM_IMAGE_DEPENDS atom-error atom-utils atom-io)

# =============================================================================
# Application-Level Module Dependencies
# =============================================================================

# Connection module - network connections
set(ATOM_ATOM_CONNECTION_DEPENDS atom-error atom-async)

# Components module - component framework
set(ATOM_ATOM_COMPONENTS_DEPENDS atom-error atom-type)

# Web module - web utilities
set(ATOM_ATOM_WEB_DEPENDS atom-error atom-utils atom-io atom-system atom-type)

# =============================================================================
# Module Build Order (topologically sorted)
# =============================================================================
# This defines the order in which modules should be built to satisfy
# dependencies

set(ATOM_MODULE_BUILD_ORDER
    # Core (no dependencies)
    atom-error
    atom-type
    atom-containers
    # Low-level
    atom-log
    atom-meta
    atom-memory
    # Mid-level
    atom-utils
    atom-algorithm
    atom-async
    atom-io
    # High-level
    atom-sysinfo
    atom-system
    atom-serial
    atom-secret
    atom-search
    atom-image
    # Application-level
    atom-connection
    atom-components
    atom-web)

# =============================================================================
# Module Categories (for grouping in IDEs and documentation)
# =============================================================================

set(ATOM_CORE_MODULES atom-error atom-type atom-containers)
set(ATOM_LOWLEVEL_MODULES atom-log atom-meta atom-memory)
set(ATOM_MIDLEVEL_MODULES atom-utils atom-algorithm atom-async atom-io)
set(ATOM_HIGHLEVEL_MODULES atom-sysinfo atom-system atom-serial atom-secret
                           atom-search atom-image)
set(ATOM_APP_MODULES atom-connection atom-components atom-web)

# All modules list
set(ATOM_ALL_MODULES
    ${ATOM_CORE_MODULES} ${ATOM_LOWLEVEL_MODULES} ${ATOM_MIDLEVEL_MODULES}
    ${ATOM_HIGHLEVEL_MODULES} ${ATOM_APP_MODULES})
