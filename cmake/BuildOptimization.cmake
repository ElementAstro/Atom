# BuildOptimization.cmake Centralized build optimization configuration for the
# Atom project Includes: compiler caching, PCH, Unity Build, incremental build
# optimizations

include_guard(GLOBAL)

# =============================================================================
# Build Optimization Options
# =============================================================================

option(ATOM_ENABLE_CCACHE "Enable compiler caching (ccache/sccache)" ON)
option(ATOM_ENABLE_PCH "Enable precompiled headers" ON)
option(ATOM_ENABLE_UNITY_BUILD "Enable Unity Build (merged compilation)" OFF)
option(ATOM_ENABLE_FAST_LINK "Enable fast/incremental linking" ON)
option(ATOM_PARALLEL_LINK_JOBS "Number of parallel link jobs (0=auto)" 0)

# =============================================================================
# Phase 1: Compiler Cache Support (ccache/sccache)
# =============================================================================

function(atom_setup_compiler_cache)
  if(NOT ATOM_ENABLE_CCACHE)
    message(STATUS "Compiler caching disabled")
    return()
  endif()

  # Prefer sccache over ccache (better for distributed builds)
  find_program(SCCACHE_PROGRAM sccache)
  find_program(CCACHE_PROGRAM ccache)

  if(SCCACHE_PROGRAM)
    message(STATUS "Found sccache: ${SCCACHE_PROGRAM}")
    set(COMPILER_CACHE_PROGRAM ${SCCACHE_PROGRAM})
    set(COMPILER_CACHE_NAME "sccache")
  elseif(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(COMPILER_CACHE_PROGRAM ${CCACHE_PROGRAM})
    set(COMPILER_CACHE_NAME "ccache")
  else()
    message(STATUS "No compiler cache found (ccache/sccache)")
    message(STATUS "  Install ccache: https://ccache.dev/")
    message(STATUS "  Install sccache: https://github.com/mozilla/sccache")
    return()
  endif()

  # Set compiler launcher for C and C++
  set(CMAKE_C_COMPILER_LAUNCHER
      ${COMPILER_CACHE_PROGRAM}
      CACHE STRING "C compiler launcher" FORCE)
  set(CMAKE_CXX_COMPILER_LAUNCHER
      ${COMPILER_CACHE_PROGRAM}
      CACHE STRING "CXX compiler launcher" FORCE)

  # Store cache program for later use
  set(ATOM_COMPILER_CACHE_PROGRAM
      ${COMPILER_CACHE_PROGRAM}
      CACHE INTERNAL "Compiler cache program")
  set(ATOM_COMPILER_CACHE_NAME
      ${COMPILER_CACHE_NAME}
      CACHE INTERNAL "Compiler cache name")

  message(STATUS "Compiler caching enabled with ${COMPILER_CACHE_NAME}")

  # Configure ccache-specific options
  if(COMPILER_CACHE_NAME STREQUAL "ccache")
    # Set ccache configuration via environment
    set(ENV{CCACHE_SLOPPINESS}
        "pch_defines,time_macros,include_file_mtime,include_file_ctime")
    set(ENV{CCACHE_MAXSIZE} "5G")
    # Enable PCH support in ccache
    set(ENV{CCACHE_PCH_EXTSUM} "true")
  endif()

  # Configure sccache-specific options
  if(COMPILER_CACHE_NAME STREQUAL "sccache")
    # sccache uses different environment variables
    set(ENV{SCCACHE_CACHE_SIZE} "5G")
  endif()
endfunction()

# Custom target to show cache statistics
function(atom_add_cache_stats_target)
  if(NOT DEFINED ATOM_COMPILER_CACHE_PROGRAM)
    return()
  endif()

  if(ATOM_COMPILER_CACHE_NAME STREQUAL "ccache")
    add_custom_target(
      cache-stats
      COMMAND ${ATOM_COMPILER_CACHE_PROGRAM} -s
      COMMENT "Showing ccache statistics"
      VERBATIM)
    add_custom_target(
      cache-clear
      COMMAND ${ATOM_COMPILER_CACHE_PROGRAM} -C
      COMMENT "Clearing ccache"
      VERBATIM)
  elseif(ATOM_COMPILER_CACHE_NAME STREQUAL "sccache")
    add_custom_target(
      cache-stats
      COMMAND ${ATOM_COMPILER_CACHE_PROGRAM} --show-stats
      COMMENT "Showing sccache statistics"
      VERBATIM)
    add_custom_target(
      cache-clear
      COMMAND ${ATOM_COMPILER_CACHE_PROGRAM} --stop-server
      COMMENT "Stopping sccache server (clears cache)"
      VERBATIM)
  endif()
endfunction()

# =============================================================================
# Phase 2: Precompiled Headers (PCH) Support
# =============================================================================

# Function to setup PCH for a target
function(atom_target_precompile_headers target_name)
  if(NOT ATOM_ENABLE_PCH)
    return()
  endif()

  # Check CMake version for PCH support
  if(CMAKE_VERSION VERSION_LESS "3.16")
    message(STATUS "PCH requires CMake 3.16+, skipping for ${target_name}")
    return()
  endif()

  set(options REUSE_FROM)
  set(oneValueArgs PCH_HEADER REUSE_TARGET)
  set(multiValueArgs HEADERS)
  cmake_parse_arguments(APCH "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  # If reusing from another target
  if(APCH_REUSE_FROM AND APCH_REUSE_TARGET)
    if(TARGET ${APCH_REUSE_TARGET})
      target_precompile_headers(${target_name} REUSE_FROM ${APCH_REUSE_TARGET})
      message(STATUS "PCH: ${target_name} reusing from ${APCH_REUSE_TARGET}")
      return()
    endif()
  endif()

  # Use provided PCH header or default
  if(APCH_PCH_HEADER)
    set(pch_header ${APCH_PCH_HEADER})
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/atom/pch.hpp")
    set(pch_header "${CMAKE_SOURCE_DIR}/atom/pch.hpp")
  else()
    # Use provided headers list
    if(APCH_HEADERS)
      target_precompile_headers(${target_name} PRIVATE ${APCH_HEADERS})
      message(STATUS "PCH: ${target_name} using custom headers")
      return()
    endif()
    return()
  endif()

  target_precompile_headers(${target_name} PRIVATE ${pch_header})
  message(STATUS "PCH: ${target_name} using ${pch_header}")
endfunction()

# =============================================================================
# Phase 3: Unity Build Support
# =============================================================================

function(atom_setup_unity_build)
  if(NOT ATOM_ENABLE_UNITY_BUILD)
    message(STATUS "Unity Build disabled")
    return()
  endif()

  # Check CMake version for Unity Build support
  if(CMAKE_VERSION VERSION_LESS "3.16")
    message(STATUS "Unity Build requires CMake 3.16+, skipping")
    return()
  endif()

  # Enable Unity Build globally
  set(CMAKE_UNITY_BUILD
      ON
      CACHE BOOL "Enable Unity Build" FORCE)

  # Set batch size (number of sources per unity file) Larger = faster build, but
  # more memory usage
  set(CMAKE_UNITY_BUILD_BATCH_SIZE
      16
      CACHE STRING "Unity Build batch size" FORCE)

  message(
    STATUS "Unity Build enabled with batch size ${CMAKE_UNITY_BUILD_BATCH_SIZE}"
  )
endfunction()

# Function to configure Unity Build for a specific target
function(atom_target_unity_build target_name)
  if(NOT ATOM_ENABLE_UNITY_BUILD)
    return()
  endif()

  set(options DISABLE)
  set(oneValueArgs BATCH_SIZE)
  set(multiValueArgs EXCLUDE_SOURCES)
  cmake_parse_arguments(AUB "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(AUB_DISABLE)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD OFF)
    message(STATUS "Unity Build disabled for ${target_name}")
    return()
  endif()

  set_target_properties(${target_name} PROPERTIES UNITY_BUILD ON)

  if(AUB_BATCH_SIZE)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD_BATCH_SIZE
                                                    ${AUB_BATCH_SIZE})
  endif()

  # Exclude specific sources from Unity Build
  if(AUB_EXCLUDE_SOURCES)
    foreach(src ${AUB_EXCLUDE_SOURCES})
      set_source_files_properties(${src} PROPERTIES SKIP_UNITY_BUILD_INCLUSION
                                                    ON)
    endforeach()
  endif()
endfunction()

# =============================================================================
# Phase 4: Fast/Incremental Linking
# =============================================================================

function(atom_setup_fast_linking)
  if(NOT ATOM_ENABLE_FAST_LINK)
    message(STATUS "Fast linking disabled")
    return()
  endif()

  if(MSVC)
    # MSVC incremental linking
    add_link_options(/INCREMENTAL)
    # Use parallel linking
    if(ATOM_PARALLEL_LINK_JOBS GREATER 0)
      add_link_options(/CGTHREADS:${ATOM_PARALLEL_LINK_JOBS})
    endif()
    message(STATUS "Fast linking enabled (MSVC incremental)")

  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Try to use lld linker (much faster than ld)
    find_program(LLD_LINKER ld.lld)
    if(LLD_LINKER)
      add_link_options(-fuse-ld=lld)
      message(STATUS "Fast linking enabled (using lld)")
    else()
      # Try gold linker as fallback
      find_program(GOLD_LINKER ld.gold)
      if(GOLD_LINKER)
        add_link_options(-fuse-ld=gold)
        message(STATUS "Fast linking enabled (using gold)")
      endif()
    endif()

    # Enable split-dwarf for faster linking (debug info in separate file). Not
    # on Windows/PE: binutils emits sections below the image base and the
    # resulting binaries fail to load.
    if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo" AND NOT WIN32)
      add_compile_options(-gsplit-dwarf)
      add_link_options(-Wl,--gdb-index)
    endif()

  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Try to use gold or mold linker
    find_program(MOLD_LINKER mold)
    find_program(GOLD_LINKER ld.gold)

    if(MOLD_LINKER)
      add_link_options(-fuse-ld=mold)
      message(STATUS "Fast linking enabled (using mold)")
    elseif(GOLD_LINKER)
      add_link_options(-fuse-ld=gold)
      message(STATUS "Fast linking enabled (using gold)")
    endif()

    # Enable split-dwarf for faster linking. Not on Windows/PE: binutils emits
    # sections below the image base and the binaries fail to load.
    if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo" AND NOT WIN32)
      add_compile_options(-gsplit-dwarf)
    endif()
  endif()

  # Parallel link jobs
  if(ATOM_PARALLEL_LINK_JOBS GREATER 0)
    set_property(GLOBAL PROPERTY JOB_POOLS link_pool=${ATOM_PARALLEL_LINK_JOBS})
    set(CMAKE_JOB_POOL_LINK
        link_pool
        CACHE STRING "Link job pool" FORCE)
  endif()
endfunction()

# =============================================================================
# Phase 5: Object Directory Optimization
# =============================================================================

function(atom_setup_object_directories)
  # Separate object files by module to improve incremental builds
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
      ${CMAKE_BINARY_DIR}/lib
      CACHE PATH "" FORCE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
      ${CMAKE_BINARY_DIR}/lib
      CACHE PATH "" FORCE)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
      ${CMAKE_BINARY_DIR}/bin
      CACHE PATH "" FORCE)

  # Per-configuration output directories
  foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER ${config} CONFIG_UPPER)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER}
        ${CMAKE_BINARY_DIR}/lib/${config}
        CACHE PATH "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER}
        ${CMAKE_BINARY_DIR}/lib/${config}
        CACHE PATH "" FORCE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}
        ${CMAKE_BINARY_DIR}/bin/${config}
        CACHE PATH "" FORCE)
  endforeach()
endfunction()

# =============================================================================
# Phase 6: Dependency Caching
# =============================================================================

# Cache for found dependencies to avoid repeated searches
set(ATOM_DEPENDENCY_CACHE
    ""
    CACHE INTERNAL "Cached dependency results")

function(atom_cache_dependency dep_name found)
  set(ATOM_DEP_${dep_name}_CACHED
      ${found}
      CACHE INTERNAL "Cached result for ${dep_name}")
  list(APPEND ATOM_DEPENDENCY_CACHE ${dep_name})
  list(REMOVE_DUPLICATES ATOM_DEPENDENCY_CACHE)
  set(ATOM_DEPENDENCY_CACHE
      ${ATOM_DEPENDENCY_CACHE}
      CACHE INTERNAL "Cached dependency results")
endfunction()

function(atom_is_dependency_cached dep_name result_var)
  if(DEFINED ATOM_DEP_${dep_name}_CACHED)
    set(${result_var}
        TRUE
        PARENT_SCOPE)
  else()
    set(${result_var}
        FALSE
        PARENT_SCOPE)
  endif()
endfunction()

function(atom_get_cached_dependency dep_name result_var)
  if(DEFINED ATOM_DEP_${dep_name}_CACHED)
    set(${result_var}
        ${ATOM_DEP_${dep_name}_CACHED}
        PARENT_SCOPE)
  else()
    set(${result_var}
        FALSE
        PARENT_SCOPE)
  endif()
endfunction()

# =============================================================================
# Master Setup Function
# =============================================================================

function(atom_setup_build_optimizations)
  message(STATUS "")
  message(STATUS "=== Build Optimization Setup ===")

  # Setup compiler cache
  atom_setup_compiler_cache()

  # Setup Unity Build
  atom_setup_unity_build()

  # Setup fast linking
  atom_setup_fast_linking()

  # Setup object directories
  atom_setup_object_directories()

  # Add cache statistics target
  atom_add_cache_stats_target()

  message(STATUS "=================================")
  message(STATUS "")
endfunction()

# =============================================================================
# Build Timing Support
# =============================================================================

option(ATOM_ENABLE_BUILD_TIMING "Enable build timing reports" OFF)

if(ATOM_ENABLE_BUILD_TIMING)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
endif()
