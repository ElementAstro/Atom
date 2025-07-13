# CompilerOptions.cmake - Compiler options configuration module
# This file contains compiler-specific settings and optimizations for the Atom project
# It handles compiler detection, feature testing, and warning/optimization settings

# Avoid repeated inclusion
if(DEFINED COMPILER_OPTIONS_INCLUDED)
    return()
endif()
set(COMPILER_OPTIONS_INCLUDED TRUE)

include(CMakeParseArguments)
include(CheckCXXCompilerFlag)

# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Check compiler version and features
function(check_compiler_requirements)
    # Parse arguments
    set(options "")
    set(oneValueArgs CXX_STANDARD MIN_GCC_VERSION MIN_CLANG_VERSION MIN_MSVC_VERSION)
    set(multiValueArgs "")

    cmake_parse_arguments(CHECK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set default values
    if(NOT DEFINED CHECK_CXX_STANDARD)
        set(CHECK_CXX_STANDARD 23)
    endif()

    if(NOT DEFINED CHECK_MIN_GCC_VERSION)
        set(CHECK_MIN_GCC_VERSION 10.0)
    endif()

    if(NOT DEFINED CHECK_MIN_CLANG_VERSION)
        set(CHECK_MIN_CLANG_VERSION 10.0)
    endif()

    if(NOT DEFINED CHECK_MIN_MSVC_VERSION)
        set(CHECK_MIN_MSVC_VERSION 19.28)
    endif()

    # Check C++ standard support
    check_cxx_compiler_flag(-std=c++20 HAS_CXX20_FLAG)
    check_cxx_compiler_flag(-std=c++23 HAS_CXX23_FLAG)

    if(CHECK_CXX_STANDARD EQUAL 23)
        if(NOT HAS_CXX23_FLAG)
            message(FATAL_ERROR "C++23 standard support is required!")
        endif()
        set(CMAKE_CXX_STANDARD 23 PARENT_SCOPE)
    elseif(CHECK_CXX_STANDARD EQUAL 20)
        if(NOT HAS_CXX20_FLAG)
            message(FATAL_ERROR "C++20 standard support is required!")
        endif()
        set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
    else()
        set(CMAKE_CXX_STANDARD ${CHECK_CXX_STANDARD} PARENT_SCOPE)
    endif()

    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    set(CMAKE_C_STANDARD 17 PARENT_SCOPE)

    # Check compiler version
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion -std=c++${CMAKE_CXX_STANDARD}
            OUTPUT_VARIABLE GCC_VERSION
        )
        string(REGEX MATCH "[0-9]+\\.[0-9]+" GCC_VERSION ${GCC_VERSION})
        if(GCC_VERSION VERSION_LESS ${CHECK_MIN_GCC_VERSION})
            message(FATAL_ERROR "Minimum required version of g++ is ${CHECK_MIN_GCC_VERSION}")
        endif()
        message(STATUS "Using g++ version ${GCC_VERSION}")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} --version
            OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
        )
        string(REGEX MATCH "clang version ([0-9]+\\.[0-9]+)" CLANG_VERSION ${CLANG_VERSION_OUTPUT})
        string(REGEX REPLACE "clang version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION ${CLANG_VERSION})
        if(CLANG_VERSION VERSION_LESS ${CHECK_MIN_CLANG_VERSION})
            message(FATAL_ERROR "Minimum required version of clang is ${CHECK_MIN_CLANG_VERSION}")
        endif()
        message(STATUS "Using clang version ${CLANG_VERSION}")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${CHECK_MIN_MSVC_VERSION})
            message(FATAL_ERROR "Minimum required version of MSVC is ${CHECK_MIN_MSVC_VERSION} (Visual Studio 2019 version 16.10)")
        endif()
        message(STATUS "Using MSVC version ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    message(STATUS "Using C++${CMAKE_CXX_STANDARD}")

    # Set special flags for Apple platforms
    if(APPLE)
        check_cxx_compiler_flag(-stdlib=libc++ HAS_LIBCXX_FLAG)
        if(HAS_LIBCXX_FLAG)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++" PARENT_SCOPE)
        endif()
    endif()

    # Set build architecture for non-Apple platforms
    if(NOT APPLE)
        set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "Build architecture for non-Apple platforms" FORCE)
    endif()
endfunction()

# Configure compiler options function
function(configure_compiler_options)
    # Parse arguments
    set(options
        ENABLE_WARNINGS TREAT_WARNINGS_AS_ERRORS
        ENABLE_OPTIMIZATIONS ENABLE_DEBUG_INFO
        ENABLE_UTF8 ENABLE_EXCEPTION_HANDLING
        ENABLE_LTO
    )
    set(oneValueArgs WARNING_LEVEL OPTIMIZATION_LEVEL)
    set(multiValueArgs ADDITIONAL_OPTIONS)

    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set default values
    if(NOT DEFINED ARGS_WARNING_LEVEL)
        set(ARGS_WARNING_LEVEL "normal")
    endif()

    if(NOT DEFINED ARGS_OPTIMIZATION_LEVEL)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(ARGS_OPTIMIZATION_LEVEL "none")
        else()
            set(ARGS_OPTIMIZATION_LEVEL "speed")
        endif()
    endif()

    set(compiler_options "")
    set(linker_options "")

    # MSVC compiler options
    if(MSVC)
        # Basic options
        list(APPEND compiler_options
            /nologo                # Suppress copyright message
        )

        # UTF-8 support
        if(ARGS_ENABLE_UTF8)
            list(APPEND compiler_options
                /source-charset:UTF-8      # Specify source file encoding as UTF-8
                /execution-charset:UTF-8   # Specify execution character set as UTF-8
            )
        endif()

        # Exception handling
        if(ARGS_ENABLE_EXCEPTION_HANDLING)
            list(APPEND compiler_options /EHsc)
        endif()

        # Warning level
        if(ARGS_ENABLE_WARNINGS)
            if(ARGS_WARNING_LEVEL STREQUAL "high")
                list(APPEND compiler_options /W4)
            elseif(ARGS_WARNING_LEVEL STREQUAL "all")
                list(APPEND compiler_options /Wall)
            else()
                list(APPEND compiler_options /W3)
            endif()

            if(ARGS_TREAT_WARNINGS_AS_ERRORS)
                list(APPEND compiler_options /WX)
            endif()
        endif()

        # Optimization level
        if(ARGS_ENABLE_OPTIMIZATIONS)
            if(ARGS_OPTIMIZATION_LEVEL STREQUAL "speed")
                list(APPEND compiler_options /O2)
            elseif(ARGS_OPTIMIZATION_LEVEL STREQUAL "size")
                list(APPEND compiler_options /O1)
            elseif(ARGS_OPTIMIZATION_LEVEL STREQUAL "full")
                list(APPEND compiler_options /Ox)
            endif()
        else()
            list(APPEND compiler_options /Od)
        endif()

        # Debug information
        if(ARGS_ENABLE_DEBUG_INFO)
            list(APPEND compiler_options /Zi)
        endif()

        # Link Time Optimization
        if(ARGS_ENABLE_LTO)
            list(APPEND compiler_options /GL)
            list(APPEND linker_options /LTCG)
        endif()

    # GCC/Clang compiler options
    elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # UTF-8 support
        if(ARGS_ENABLE_UTF8)
            list(APPEND compiler_options -fexec-charset=UTF-8)
        endif()

        # Exception handling
        if(ARGS_ENABLE_EXCEPTION_HANDLING)
            list(APPEND compiler_options -fexceptions)
        endif()

        # Warning level
        if(ARGS_ENABLE_WARNINGS)
            if(ARGS_WARNING_LEVEL STREQUAL "high")
                list(APPEND compiler_options -Wall -Wextra)
            elseif(ARGS_WARNING_LEVEL STREQUAL "all")
                list(APPEND compiler_options -Wall -Wextra -Wpedantic)
            else()
                list(APPEND compiler_options -Wall)
            endif()

            if(ARGS_TREAT_WARNINGS_AS_ERRORS)
                list(APPEND compiler_options -Werror)
            endif()
        endif()

        # Optimization level
        if(ARGS_ENABLE_OPTIMIZATIONS)
            if(ARGS_OPTIMIZATION_LEVEL STREQUAL "speed")
                list(APPEND compiler_options -O2)
            elseif(ARGS_OPTIMIZATION_LEVEL STREQUAL "size")
                list(APPEND compiler_options -Os)
            elseif(ARGS_OPTIMIZATION_LEVEL STREQUAL "full")
                list(APPEND compiler_options -O3)
            endif()
        else()
            list(APPEND compiler_options -O0)
        endif()

        # Debug information
        if(ARGS_ENABLE_DEBUG_INFO)
            list(APPEND compiler_options -g)
        endif()

        # Link Time Optimization
        if(ARGS_ENABLE_LTO)
            list(APPEND compiler_options -flto)
            list(APPEND linker_options -flto)
        endif()
    endif()

    # Add user-provided additional options
    if(ARGS_ADDITIONAL_OPTIONS)
        list(APPEND compiler_options ${ARGS_ADDITIONAL_OPTIONS})
    endif()

    # Apply compiler options
    add_compile_options(${compiler_options})

    # Apply linker options
    if(linker_options)
        string(REPLACE ";" " " linker_flags_str "${linker_options}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${linker_flags_str}" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${linker_flags_str}" PARENT_SCOPE)
    endif()

    # Print information
    message(STATUS "Configured compiler options: ${compiler_options}")
    if(linker_options)
        message(STATUS "Configured linker options: ${linker_options}")
    endif()
endfunction()

# Preset build configuration function
function(apply_build_preset PRESET_NAME)
    if(PRESET_NAME STREQUAL "DEBUG")
        configure_compiler_options(
            ENABLE_UTF8
            ENABLE_EXCEPTION_HANDLING
            ENABLE_WARNINGS
            WARNING_LEVEL "high"
            ENABLE_DEBUG_INFO
        )
        add_definitions(-DDEBUG -D_DEBUG)

    elseif(PRESET_NAME STREQUAL "RELEASE")
        configure_compiler_options(
            ENABLE_UTF8
            ENABLE_EXCEPTION_HANDLING
            ENABLE_WARNINGS
            WARNING_LEVEL "normal"
            ENABLE_OPTIMIZATIONS
            OPTIMIZATION_LEVEL "speed"
            ENABLE_LTO
        )
        add_definitions(-DNDEBUG)

    elseif(PRESET_NAME STREQUAL "MINSIZEREL")
        configure_compiler_options(
            ENABLE_UTF8
            ENABLE_EXCEPTION_HANDLING
            ENABLE_OPTIMIZATIONS
            OPTIMIZATION_LEVEL "size"
            ENABLE_LTO
        )
        add_definitions(-DNDEBUG)

    elseif(PRESET_NAME STREQUAL "RELWITHDEBINFO")
        configure_compiler_options(
            ENABLE_UTF8
            ENABLE_EXCEPTION_HANDLING
            ENABLE_OPTIMIZATIONS
            ENABLE_DEBUG_INFO
        )
        add_definitions(-DNDEBUG)

    elseif(PRESET_NAME STREQUAL "SANITIZE")
        # Enable code analysis and checking tools
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            configure_compiler_options(
                ENABLE_UTF8
                ENABLE_EXCEPTION_HANDLING
                ENABLE_WARNINGS
                WARNING_LEVEL "high"
                ENABLE_DEBUG_INFO
                ADDITIONAL_OPTIONS "-fsanitize=address" "-fsanitize=undefined"
            )
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=undefined" PARENT_SCOPE)
        endif()
        add_definitions(-DDEBUG -D_DEBUG)
    endif()
endfunction()

# Platform detection and configuration function
function(configure_platform_options)
    # Check platform type
    if(WIN32)
        add_definitions(-DPLATFORM_WINDOWS)
        if(MSVC)
            add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        endif()
    elseif(APPLE)
        add_definitions(-DPLATFORM_MACOS)
    elseif(UNIX AND NOT APPLE)
        add_definitions(-DPLATFORM_LINUX)
    endif()

    # Check architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DARCH_X64)
    else()
        add_definitions(-DARCH_X86)
    endif()
endfunction()

# Project comprehensive configuration macro
macro(setup_project_defaults)
    # Parse arguments
    set(options STATIC_RUNTIME ENABLE_PCH)
    set(oneValueArgs BUILD_PRESET CXX_STANDARD MIN_GCC_VERSION MIN_CLANG_VERSION MIN_MSVC_VERSION)
    set(multiValueArgs PCH_HEADERS)

    cmake_parse_arguments(SETUP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Check compiler requirements
    check_compiler_requirements(
        CXX_STANDARD ${SETUP_CXX_STANDARD}
        MIN_GCC_VERSION ${SETUP_MIN_GCC_VERSION}
        MIN_CLANG_VERSION ${SETUP_MIN_CLANG_VERSION}
        MIN_MSVC_VERSION ${SETUP_MIN_MSVC_VERSION}
    )

    # Configure static runtime library
    if(SETUP_STATIC_RUNTIME AND MSVC)
        set(variables
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
        )
        foreach(variable ${variables})
            if(${variable} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
            endif()
        endforeach()
    endif()

    # Apply platform options
    configure_platform_options()

    # Apply build preset
    if(DEFINED SETUP_BUILD_PRESET)
        apply_build_preset(${SETUP_BUILD_PRESET})
    else()
        # Automatically select preset based on CMake build type
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            apply_build_preset("DEBUG")
        elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
            apply_build_preset("RELEASE")
        elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
            apply_build_preset("MINSIZEREL")
        elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            apply_build_preset("RELWITHDEBINFO")
        else()
            # Default to Release settings
            apply_build_preset("RELEASE")
        endif()
    endif()

    # Configure precompiled headers
    if(SETUP_ENABLE_PCH AND DEFINED SETUP_PCH_HEADERS)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
            # Use new precompiled header features
            if(TARGET ${PROJECT_NAME})
                target_precompile_headers(${PROJECT_NAME} PRIVATE ${SETUP_PCH_HEADERS})
            else()
                message(WARNING "Project target '${PROJECT_NAME}' not found for precompiled header configuration")
            endif()
        else()
            message(WARNING "Precompiled header functionality requested, but CMake version does not support it (3.16+ required)")
        endif()
    endif()
endmacro()

if(LINUX)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_COLOR_MAKEFILE OFF)
endif()
