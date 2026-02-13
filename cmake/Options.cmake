# Options.cmake Centralized build options with grouping and documentation for
# the Atom project This file provides module grouping options and help
# functions. Module dependency data is defined in ModuleDependenciesData.cmake

include_guard(GLOBAL)

# Include module dependencies data for consistent definitions
include(${CMAKE_CURRENT_LIST_DIR}/ModuleDependenciesData.cmake)

# =============================================================================
# Module Group Definitions (derived from ModuleDependenciesData.cmake)
# =============================================================================
# These lists use UPPERCASE names for CMake option compatibility The source data
# in ModuleDependenciesData.cmake uses atom-xxx format

# Core modules - fundamental functionality required by most other modules
set(ATOM_CORE_MODULES ERROR TYPE CONTAINERS META)

# Utility modules - general-purpose utilities
set(ATOM_UTILITY_MODULES UTILS ALGORITHM MEMORY LOG)

# System modules - system interaction and information
set(ATOM_SYSTEM_MODULES SYSTEM SYSINFO IO SERIAL)

# Network modules - networking and communication
set(ATOM_NETWORK_MODULES CONNECTION WEB ASYNC)

# Application modules - high-level application components
set(ATOM_APPLICATION_MODULES COMPONENTS IMAGE SEARCH SECRET)

# =============================================================================
# Quick Build Options (Module Groups)
# =============================================================================

option(ATOM_BUILD_MINIMAL "Build only core modules (minimal footprint)" OFF)
option(ATOM_BUILD_CORE "Build core and utility modules" OFF)
option(ATOM_BUILD_SYSTEM "Build system-related modules" OFF)
option(ATOM_BUILD_NETWORK "Build networking modules" OFF)
option(ATOM_BUILD_APPLICATION "Build application-level modules" OFF)

# =============================================================================
# Process Group Options
# =============================================================================

function(atom_process_module_groups)
  # If BUILD_ALL is ON, enable everything
  if(ATOM_BUILD_ALL)
    message(STATUS "ATOM_BUILD_ALL is ON - enabling all modules")
    return()
  endif()

  # Process MINIMAL option (only core)
  if(ATOM_BUILD_MINIMAL)
    message(STATUS "ATOM_BUILD_MINIMAL is ON - enabling core modules only")
    foreach(module ${ATOM_CORE_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
    # Disable non-core modules
    foreach(module ${ATOM_UTILITY_MODULES} ${ATOM_SYSTEM_MODULES}
                   ${ATOM_NETWORK_MODULES} ${ATOM_APPLICATION_MODULES})
      if(NOT module IN_LIST ATOM_CORE_MODULES)
        set(ATOM_BUILD_${module}
            OFF
            CACHE BOOL "Build ${module} module" FORCE)
      endif()
    endforeach()
    return()
  endif()

  # Process CORE option
  if(ATOM_BUILD_CORE)
    message(STATUS "ATOM_BUILD_CORE is ON - enabling core and utility modules")
    foreach(module ${ATOM_CORE_MODULES} ${ATOM_UTILITY_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
  endif()

  # Process SYSTEM option
  if(ATOM_BUILD_SYSTEM)
    message(STATUS "ATOM_BUILD_SYSTEM is ON - enabling system modules")
    foreach(module ${ATOM_SYSTEM_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
    # System modules depend on core
    foreach(module ${ATOM_CORE_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
  endif()

  # Process NETWORK option
  if(ATOM_BUILD_NETWORK)
    message(STATUS "ATOM_BUILD_NETWORK is ON - enabling network modules")
    foreach(module ${ATOM_NETWORK_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
    # Network modules depend on core and utils
    foreach(module ${ATOM_CORE_MODULES} UTILS)
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
  endif()

  # Process APPLICATION option
  if(ATOM_BUILD_APPLICATION)
    message(
      STATUS "ATOM_BUILD_APPLICATION is ON - enabling application modules")
    foreach(module ${ATOM_APPLICATION_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
    # Application modules depend on core
    foreach(module ${ATOM_CORE_MODULES})
      set(ATOM_BUILD_${module}
          ON
          CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
  endif()
endfunction()

# =============================================================================
# Module Information and Help
# =============================================================================

function(atom_print_module_help)
  message(STATUS "")
  message(
    STATUS
      "================================================================================"
  )
  message(STATUS "                          ATOM BUILD OPTIONS HELP")
  message(
    STATUS
      "================================================================================"
  )
  message(STATUS "")
  message(STATUS "QUICK BUILD OPTIONS (Module Groups):")
  message(STATUS "  -DATOM_BUILD_ALL=ON           Build all modules (default)")
  message(
    STATUS
      "  -DATOM_BUILD_MINIMAL=ON       Build only core modules (smallest footprint)"
  )
  message(STATUS "  -DATOM_BUILD_CORE=ON          Build core + utility modules")
  message(STATUS "  -DATOM_BUILD_SYSTEM=ON        Build system-related modules")
  message(STATUS "  -DATOM_BUILD_NETWORK=ON       Build networking modules")
  message(
    STATUS "  -DATOM_BUILD_APPLICATION=ON   Build application-level modules")
  message(STATUS "")
  message(STATUS "MODULE GROUPS:")
  message(STATUS "  Core:        ${ATOM_CORE_MODULES}")
  message(STATUS "  Utility:     ${ATOM_UTILITY_MODULES}")
  message(STATUS "  System:      ${ATOM_SYSTEM_MODULES}")
  message(STATUS "  Network:     ${ATOM_NETWORK_MODULES}")
  message(STATUS "  Application: ${ATOM_APPLICATION_MODULES}")
  message(STATUS "")
  message(STATUS "BUILD PERFORMANCE OPTIONS:")
  message(
    STATUS
      "  -DATOM_ENABLE_CCACHE=ON       Enable compiler caching (ccache/sccache)"
  )
  message(STATUS "  -DATOM_ENABLE_PCH=ON          Enable precompiled headers")
  message(
    STATUS
      "  -DATOM_ENABLE_UNITY_BUILD=ON  Enable unity build (merged compilation)")
  message(
    STATUS "  -DATOM_ENABLE_FAST_LINK=ON    Enable fast/incremental linking")
  message(STATUS "")
  message(STATUS "OPTIONAL FEATURE DEPENDENCIES:")
  message(
    STATUS "  -DATOM_USE_OPENCV=ON          Enable OpenCV for image processing")
  message(
    STATUS
      "  -DATOM_USE_TBB=ON             Enable Intel TBB for parallel algorithms"
  )
  message(STATUS "  -DATOM_USE_BOOST=ON           Enable Boost libraries")
  message(
    STATUS "  -DATOM_USE_MINIZIP=ON         Enable minizip-ng for compression")
  message(STATUS "  -DATOM_USE_LIBUV=ON           Enable libuv for async I/O")
  message(STATUS "")
  message(STATUS "BUILD TARGETS:")
  message(STATUS "  -DATOM_BUILD_TESTS=ON         Build test suite")
  message(STATUS "  -DATOM_BUILD_EXAMPLES=ON      Build examples")
  message(STATUS "  -DATOM_BUILD_PYTHON_BINDINGS=ON Build Python bindings")
  message(STATUS "")
  message(STATUS "CMAKE PRESETS:")
  message(STATUS "  cmake --preset release        Standard release build")
  message(STATUS "  cmake --preset debug          Debug build with symbols")
  message(STATUS "  cmake --preset fast-build     All optimizations enabled")
  message(STATUS "  cmake --preset minimal        Core modules only")
  message(STATUS "  cmake --preset minsizerel     Optimized for binary size")
  message(STATUS "")
  message(
    STATUS
      "================================================================================"
  )
endfunction()

# Print help if requested
option(ATOM_PRINT_OPTIONS "Print available build options" OFF)
if(ATOM_PRINT_OPTIONS)
  atom_print_module_help()
endif()

# =============================================================================
# Module Status Summary
# =============================================================================

function(atom_print_module_status)
  message(STATUS "")
  message(STATUS "=== Module Build Status ===")

  # Core modules
  message(STATUS "Core Modules:")
  foreach(module ${ATOM_CORE_MODULES})
    if(ATOM_BUILD_${module})
      message(STATUS "  [x] ${module}")
    else()
      message(STATUS "  [ ] ${module}")
    endif()
  endforeach()

  # Utility modules
  message(STATUS "Utility Modules:")
  foreach(module ${ATOM_UTILITY_MODULES})
    if(ATOM_BUILD_${module})
      message(STATUS "  [x] ${module}")
    else()
      message(STATUS "  [ ] ${module}")
    endif()
  endforeach()

  # System modules
  message(STATUS "System Modules:")
  foreach(module ${ATOM_SYSTEM_MODULES})
    if(ATOM_BUILD_${module})
      message(STATUS "  [x] ${module}")
    else()
      message(STATUS "  [ ] ${module}")
    endif()
  endforeach()

  # Network modules
  message(STATUS "Network Modules:")
  foreach(module ${ATOM_NETWORK_MODULES})
    if(ATOM_BUILD_${module})
      message(STATUS "  [x] ${module}")
    else()
      message(STATUS "  [ ] ${module}")
    endif()
  endforeach()

  # Application modules
  message(STATUS "Application Modules:")
  foreach(module ${ATOM_APPLICATION_MODULES})
    if(ATOM_BUILD_${module})
      message(STATUS "  [x] ${module}")
    else()
      message(STATUS "  [ ] ${module}")
    endif()
  endforeach()

  message(STATUS "===========================")
  message(STATUS "")
endfunction()

# =============================================================================
# Dependency-aware Module Disable
# =============================================================================

# Function to disable a module and warn about dependents
function(atom_disable_module module_name reason)
  string(TOUPPER ${module_name} MODULE_UPPER)

  if(ATOM_BUILD_${MODULE_UPPER})
    message(STATUS "Disabling module ${module_name}: ${reason}")
    set(ATOM_BUILD_${MODULE_UPPER}
        OFF
        CACHE BOOL "Build ${module_name} module" FORCE)
  endif()
endfunction()

# NOTE: atom_check_module_dependencies has been moved to
# ModuleDependencies.cmake Use atom_validate_module_dependencies() from that
# file instead.
