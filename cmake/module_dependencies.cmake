# Module Dependency Configuration
# This file defines the dependencies between modules in the Atom project.
# When a module is enabled, its dependencies are automatically enabled too.
#
# Format:
# set(ATOM_<MODULE>_DEPENDS <list of dependent modules>)
#
# The dependency system ensures that modules are built in the correct order
# and that all required dependencies are satisfied.

# Error handling module has no dependencies
set(ATOM_ERROR_DEPENDS "")

# Log module depends on error module
set(ATOM_LOG_DEPENDS atom-error)

# Algorithm module dependencies
set(ATOM_ALGORITHM_DEPENDS atom-error)

# Async module dependencies
set(ATOM_ASYNC_DEPENDS atom-error)

# Components module dependencies
set(ATOM_COMPONENTS_DEPENDS atom-error atom-utils)

# Connection module dependencies
set(ATOM_CONNECTION_DEPENDS atom-error atom-utils)

# IO module dependencies
set(ATOM_IO_DEPENDS atom-error)

# Metadata module dependencies
set(ATOM_META_DEPENDS atom-error)

# Search module dependencies
set(ATOM_SEARCH_DEPENDS atom-error atom-utils)

# Security module dependencies
set(ATOM_SECRET_DEPENDS atom-error)

# System info module dependencies
set(ATOM_SYSINFO_DEPENDS atom-error atom-system)

# System module dependencies
set(ATOM_SYSTEM_DEPENDS atom-error)

# Utils module dependencies
set(ATOM_UTILS_DEPENDS atom-error)

# Web module dependencies
set(ATOM_WEB_DEPENDS atom-error atom-utils atom-io)

# Set module priority order (build sequence)
set(ATOM_MODULE_BUILD_ORDER
    atom-error
   
    atom-meta
    atom-utils
    atom-algorithm
    atom-io
    atom-system
    atom-sysinfo
    atom-async
    atom-components
    atom-connection
    atom-search
    atom-secret
    atom-web
)
