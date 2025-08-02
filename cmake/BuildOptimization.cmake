# cmake/BuildOptimization.cmake
# Advanced build optimization module for cross-platform builds
# Provides intelligent build optimization based on system capabilities

# Avoid repeated inclusion
if(DEFINED BUILD_OPTIMIZATION_INCLUDED)
    return()
endif()
set(BUILD_OPTIMIZATION_INCLUDED TRUE)

include(ProcessorCount)
include(CheckCXXCompilerFlag)

# -----------------------------------------------------------------------------
# System Capability Detection
# -----------------------------------------------------------------------------
function(detect_system_capabilities)
    # Detect number of CPU cores
    ProcessorCount(CPU_CORES)
    if(NOT CPU_CORES EQUAL 0)
        set(ATOM_CPU_CORES ${CPU_CORES} PARENT_SCOPE)
    else()
        set(ATOM_CPU_CORES 4 PARENT_SCOPE) # Default fallback
    endif()
    
    # Detect available memory (Linux/macOS)
    if(UNIX)
        if(APPLE)
            execute_process(
                COMMAND sysctl -n hw.memsize
                OUTPUT_VARIABLE MEMORY_BYTES
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(MEMORY_BYTES)
                math(EXPR MEMORY_GB "${MEMORY_BYTES} / 1024 / 1024 / 1024")
                set(ATOM_MEMORY_GB ${MEMORY_GB} PARENT_SCOPE)
            else()
                set(ATOM_MEMORY_GB 8 PARENT_SCOPE) # Default fallback
            endif()
        else()
            # Linux
            execute_process(
                COMMAND awk "/MemTotal/ {printf \"%.0f\", $2/1024/1024}" /proc/meminfo
                OUTPUT_VARIABLE MEMORY_GB
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(MEMORY_GB)
                set(ATOM_MEMORY_GB ${MEMORY_GB} PARENT_SCOPE)
            else()
                set(ATOM_MEMORY_GB 8 PARENT_SCOPE) # Default fallback
            endif()
        endif()
    else()
        # Windows - use default
        set(ATOM_MEMORY_GB 8 PARENT_SCOPE)
    endif()
    
    message(STATUS "Detected system: ${ATOM_CPU_CORES} CPU cores, ${ATOM_MEMORY_GB}GB RAM")
endfunction()

# -----------------------------------------------------------------------------
# Parallel Build Optimization
# -----------------------------------------------------------------------------
function(optimize_parallel_build)
    detect_system_capabilities()
    
    # Calculate optimal parallel jobs
    # Consider both CPU and memory constraints
    math(EXPR MEMORY_LIMITED_JOBS "${ATOM_MEMORY_GB} / 2") # 2GB per job
    set(CPU_LIMITED_JOBS ${ATOM_CPU_CORES})
    
    # Use the more conservative limit
    if(MEMORY_LIMITED_JOBS LESS CPU_LIMITED_JOBS)
        set(OPTIMAL_JOBS ${MEMORY_LIMITED_JOBS})
    else()
        set(OPTIMAL_JOBS ${CPU_LIMITED_JOBS})
    endif()
    
    # Ensure at least 1 job and at most 16 jobs
    if(OPTIMAL_JOBS LESS 1)
        set(OPTIMAL_JOBS 1)
    elseif(OPTIMAL_JOBS GREATER 16)
        set(OPTIMAL_JOBS 16)
    endif()
    
    # Set parallel build options
    if(CMAKE_GENERATOR MATCHES "Ninja")
        # Ninja handles parallelism automatically, but we can set a limit
        set(CMAKE_JOB_POOL_COMPILE compile_pool)
        set(CMAKE_JOB_POOL_LINK link_pool)
        set_property(GLOBAL PROPERTY JOB_POOLS 
            compile_pool=${OPTIMAL_JOBS} 
            link_pool=2) # Limit link jobs to prevent memory issues
    elseif(CMAKE_GENERATOR MATCHES "Make")
        # For Make, set MAKEFLAGS
        set(ENV{MAKEFLAGS} "-j${OPTIMAL_JOBS}")
    endif()
    
    message(STATUS "Optimized for ${OPTIMAL_JOBS} parallel jobs")
    set(ATOM_PARALLEL_JOBS ${OPTIMAL_JOBS} PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Compiler-Specific Optimizations
# -----------------------------------------------------------------------------
function(apply_compiler_optimizations)
    # Check for compiler-specific optimization flags
    set(OPTIMIZATION_FLAGS "")
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # GCC-specific optimizations
        check_cxx_compiler_flag(-ffast-math HAS_FAST_MATH)
        check_cxx_compiler_flag(-funroll-loops HAS_UNROLL_LOOPS)
        check_cxx_compiler_flag(-fomit-frame-pointer HAS_OMIT_FRAME_POINTER)
        
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            if(HAS_FAST_MATH)
                list(APPEND OPTIMIZATION_FLAGS -ffast-math)
            endif()
            if(HAS_UNROLL_LOOPS)
                list(APPEND OPTIMIZATION_FLAGS -funroll-loops)
            endif()
            if(HAS_OMIT_FRAME_POINTER)
                list(APPEND OPTIMIZATION_FLAGS -fomit-frame-pointer)
            endif()
        endif()
        
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang-specific optimizations
        check_cxx_compiler_flag(-ffast-math HAS_FAST_MATH)
        check_cxx_compiler_flag(-funroll-loops HAS_UNROLL_LOOPS)
        
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            if(HAS_FAST_MATH)
                list(APPEND OPTIMIZATION_FLAGS -ffast-math)
            endif()
            if(HAS_UNROLL_LOOPS)
                list(APPEND OPTIMIZATION_FLAGS -funroll-loops)
            endif()
        endif()
        
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # MSVC-specific optimizations
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            list(APPEND OPTIMIZATION_FLAGS /Ox /Ob2 /Oi /Ot /Oy /GL)
        endif()
    endif()
    
    if(OPTIMIZATION_FLAGS)
        add_compile_options(${OPTIMIZATION_FLAGS})
        message(STATUS "Applied compiler optimizations: ${OPTIMIZATION_FLAGS}")
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Unity Build Configuration
# -----------------------------------------------------------------------------
function(configure_unity_build TARGET_NAME)
    if(ATOM_ENABLE_UNITY_BUILD AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        set_target_properties(${TARGET_NAME} PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE ${CMAKE_UNITY_BUILD_BATCH_SIZE}
        )
        message(STATUS "Unity build enabled for ${TARGET_NAME} with batch size ${CMAKE_UNITY_BUILD_BATCH_SIZE}")
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Precompiled Headers Setup
# -----------------------------------------------------------------------------
function(setup_precompiled_headers TARGET_NAME)
    if(ATOM_ENABLE_PRECOMPILED_HEADERS AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        # Common headers that are frequently included
        set(PCH_HEADERS
            <iostream>
            <string>
            <vector>
            <memory>
            <algorithm>
            <functional>
            <thread>
            <mutex>
            <atomic>
            <chrono>
            <filesystem>
        )
        
        target_precompile_headers(${TARGET_NAME} PRIVATE ${PCH_HEADERS})
        message(STATUS "Precompiled headers configured for ${TARGET_NAME}")
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Build Cache Configuration
# -----------------------------------------------------------------------------
function(configure_build_cache)
    # Configure build cache directory
    if(NOT DEFINED CMAKE_CACHE_DIR)
        set(CMAKE_CACHE_DIR "${CMAKE_BINARY_DIR}/.cache" CACHE PATH "Build cache directory")
    endif()
    
    # Create cache directory
    file(MAKE_DIRECTORY ${CMAKE_CACHE_DIR})
    
    # Set cache-related variables
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CACHE_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CACHE_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CACHE_DIR}/bin)
    
    message(STATUS "Build cache configured at ${CMAKE_CACHE_DIR}")
endfunction()

# -----------------------------------------------------------------------------
# Main Optimization Setup Function
# -----------------------------------------------------------------------------
function(setup_build_optimizations)
    message(STATUS "Setting up build optimizations...")
    
    # Apply system-specific optimizations
    optimize_parallel_build()
    
    # Apply compiler optimizations
    apply_compiler_optimizations()
    
    # Configure build cache
    configure_build_cache()
    
    message(STATUS "Build optimizations configured successfully")
endfunction()

# -----------------------------------------------------------------------------
# Target-Specific Optimization Function
# -----------------------------------------------------------------------------
function(optimize_target TARGET_NAME)
    # Apply unity build if enabled
    configure_unity_build(${TARGET_NAME})
    
    # Setup precompiled headers if enabled
    setup_precompiled_headers(${TARGET_NAME})
    
    # Set target-specific properties for better performance
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    
    message(STATUS "Target optimizations applied to ${TARGET_NAME}")
endfunction()
