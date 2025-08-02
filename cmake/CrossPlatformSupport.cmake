# cmake/CrossPlatformSupport.cmake
# Enhanced cross-platform support module
# Provides comprehensive platform detection and configuration

# Avoid repeated inclusion
if(DEFINED CROSS_PLATFORM_SUPPORT_INCLUDED)
    return()
endif()
set(CROSS_PLATFORM_SUPPORT_INCLUDED TRUE)

include(CheckCXXCompilerFlag)
include(CheckIncludeFileCXX)
include(CheckLibraryExists)

# -----------------------------------------------------------------------------
# Platform Detection and Configuration
# -----------------------------------------------------------------------------
function(detect_platform_capabilities)
    # Enhanced platform detection
    set(ATOM_PLATFORM_WINDOWS FALSE PARENT_SCOPE)
    set(ATOM_PLATFORM_LINUX FALSE PARENT_SCOPE)
    set(ATOM_PLATFORM_MACOS FALSE PARENT_SCOPE)
    set(ATOM_PLATFORM_WSL FALSE PARENT_SCOPE)
    set(ATOM_PLATFORM_MINGW FALSE PARENT_SCOPE)
    set(ATOM_PLATFORM_CYGWIN FALSE PARENT_SCOPE)

    if(WIN32)
        set(ATOM_PLATFORM_WINDOWS TRUE PARENT_SCOPE)
        if(MINGW)
            set(ATOM_PLATFORM_MINGW TRUE PARENT_SCOPE)
        elseif(CYGWIN)
            set(ATOM_PLATFORM_CYGWIN TRUE PARENT_SCOPE)
        endif()
    elseif(APPLE)
        set(ATOM_PLATFORM_MACOS TRUE PARENT_SCOPE)
    elseif(UNIX)
        set(ATOM_PLATFORM_LINUX TRUE PARENT_SCOPE)

        # Detect WSL
        if(EXISTS "/proc/version")
            file(READ "/proc/version" PROC_VERSION)
            if(PROC_VERSION MATCHES "Microsoft|WSL")
                set(ATOM_PLATFORM_WSL TRUE PARENT_SCOPE)
            endif()
        endif()
    endif()

    # Architecture detection
    set(ATOM_ARCH_X64 FALSE PARENT_SCOPE)
    set(ATOM_ARCH_X86 FALSE PARENT_SCOPE)
    set(ATOM_ARCH_ARM64 FALSE PARENT_SCOPE)
    set(ATOM_ARCH_ARM FALSE PARENT_SCOPE)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(ATOM_ARCH_ARM64 TRUE PARENT_SCOPE)
        else()
            set(ATOM_ARCH_X64 TRUE PARENT_SCOPE)
        endif()
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
            set(ATOM_ARCH_ARM TRUE PARENT_SCOPE)
        else()
            set(ATOM_ARCH_X86 TRUE PARENT_SCOPE)
        endif()
    endif()

    message(STATUS "Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
endfunction()

# -----------------------------------------------------------------------------
# Compiler Feature Detection
# -----------------------------------------------------------------------------
function(detect_compiler_features)
    # C++23 features
    check_cxx_compiler_flag(-std=c++23 HAS_CXX23)
    check_cxx_compiler_flag(-std=c++20 HAS_CXX20)

    # Optimization flags
    check_cxx_compiler_flag(-march=native HAS_MARCH_NATIVE)
    check_cxx_compiler_flag(-mtune=native HAS_MTUNE_NATIVE)
    check_cxx_compiler_flag(-flto HAS_LTO)
    check_cxx_compiler_flag(-ffast-math HAS_FAST_MATH)

    # Warning flags
    check_cxx_compiler_flag(-Wall HAS_WALL)
    check_cxx_compiler_flag(-Wextra HAS_WEXTRA)
    check_cxx_compiler_flag(-Wpedantic HAS_WPEDANTIC)

    # Sanitizer flags
    check_cxx_compiler_flag(-fsanitize=address HAS_ASAN)
    check_cxx_compiler_flag(-fsanitize=undefined HAS_UBSAN)
    check_cxx_compiler_flag(-fsanitize=thread HAS_TSAN)

    # Set global properties
    set_property(GLOBAL PROPERTY ATOM_HAS_CXX23 ${HAS_CXX23})
    set_property(GLOBAL PROPERTY ATOM_HAS_CXX20 ${HAS_CXX20})
    set_property(GLOBAL PROPERTY ATOM_HAS_MARCH_NATIVE ${HAS_MARCH_NATIVE})
    set_property(GLOBAL PROPERTY ATOM_HAS_LTO ${HAS_LTO})
    set_property(GLOBAL PROPERTY ATOM_HAS_ASAN ${HAS_ASAN})
    set_property(GLOBAL PROPERTY ATOM_HAS_UBSAN ${HAS_UBSAN})

    message(STATUS "Compiler features detected: C++23=${HAS_CXX23}, LTO=${HAS_LTO}, ASAN=${HAS_ASAN}")
endfunction()

# -----------------------------------------------------------------------------
# Platform-Specific Library Detection
# -----------------------------------------------------------------------------
function(detect_platform_libraries)
    # Windows-specific libraries
    if(ATOM_PLATFORM_WINDOWS)
        find_library(WS2_32_LIBRARY ws2_32)
        find_library(WSOCK32_LIBRARY wsock32)
        find_library(IPHLPAPI_LIBRARY iphlpapi)
        find_library(PSAPI_LIBRARY psapi)

        set(ATOM_PLATFORM_LIBRARIES
            ${WS2_32_LIBRARY}
            ${WSOCK32_LIBRARY}
            ${IPHLPAPI_LIBRARY}
            ${PSAPI_LIBRARY}
            PARENT_SCOPE
        )
    endif()

    # Linux-specific libraries
    if(ATOM_PLATFORM_LINUX)
        find_library(PTHREAD_LIBRARY pthread)
        find_library(DL_LIBRARY dl)
        find_library(RT_LIBRARY rt)

        set(ATOM_PLATFORM_LIBRARIES
            ${PTHREAD_LIBRARY}
            ${DL_LIBRARY}
            ${RT_LIBRARY}
            PARENT_SCOPE
        )
    endif()

    # macOS-specific libraries
    if(ATOM_PLATFORM_MACOS)
        find_library(COREFOUNDATION_LIBRARY CoreFoundation)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(SECURITY_LIBRARY Security)

        set(ATOM_PLATFORM_LIBRARIES
            ${COREFOUNDATION_LIBRARY}
            ${IOKIT_LIBRARY}
            ${SECURITY_LIBRARY}
            PARENT_SCOPE
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Cross-Platform Path Utilities
# -----------------------------------------------------------------------------
function(normalize_path INPUT_PATH OUTPUT_VAR)
    # Convert path separators to platform-appropriate format
    if(WIN32)
        string(REPLACE "/" "\\" NORMALIZED_PATH "${INPUT_PATH}")
    else()
        string(REPLACE "\\" "/" NORMALIZED_PATH "${INPUT_PATH}")
    endif()
    set(${OUTPUT_VAR} "${NORMALIZED_PATH}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Platform-Specific Compiler Flags
# -----------------------------------------------------------------------------
function(apply_platform_compiler_flags TARGET_NAME)
    if(ATOM_PLATFORM_WINDOWS)
        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE
                /W4                    # High warning level
                /permissive-           # Disable non-conforming code
                /Zc:__cplusplus        # Enable correct __cplusplus macro
                /utf-8                 # UTF-8 source and execution character sets
            )

            # Release-specific flags
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Release>:/Ox>      # Maximum optimization
                $<$<CONFIG:Release>:/Ob2>     # Inline expansion
                $<$<CONFIG:Release>:/Oi>      # Enable intrinsic functions
                $<$<CONFIG:Release>:/Ot>      # Favor fast code
                $<$<CONFIG:Release>:/GL>      # Whole program optimization
            )

            # Debug-specific flags
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Debug>:/Od>        # Disable optimization
                $<$<CONFIG:Debug>:/Zi>        # Debug information
                $<$<CONFIG:Debug>:/RTC1>      # Runtime checks
            )
        endif()
    else()
        # GCC/Clang flags
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall -Wextra -Wpedantic
            -fstack-protector-strong
        )

        # Release-specific flags
        get_property(HAS_MARCH_NATIVE GLOBAL PROPERTY ATOM_HAS_MARCH_NATIVE)
        if(HAS_MARCH_NATIVE)
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Release>:-march=native>
                $<$<CONFIG:Release>:-mtune=native>
            )
        endif()

        target_compile_options(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-DNDEBUG>
        )

        # Debug-specific flags
        target_compile_options(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Debug>:-O0>
            $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Debug>:-DDEBUG>
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Platform-Specific Linker Flags
# -----------------------------------------------------------------------------
function(apply_platform_linker_flags TARGET_NAME)
    if(ATOM_PLATFORM_WINDOWS)
        if(MSVC)
            target_link_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Release>:/LTCG>        # Link-time code generation
                $<$<CONFIG:Release>:/OPT:REF>     # Remove unreferenced functions
                $<$<CONFIG:Release>:/OPT:ICF>     # Identical COMDAT folding
            )
        endif()
    else()
        # Strip symbols in release builds
        target_link_options(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Release>:-s>
        )

        # Enable LTO if available
        get_property(HAS_LTO GLOBAL PROPERTY ATOM_HAS_LTO)
        if(HAS_LTO)
            target_link_options(${TARGET_NAME} PRIVATE
                $<$<CONFIG:Release>:-flto>
            )
        endif()
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Cross-Platform Target Configuration
# -----------------------------------------------------------------------------
function(configure_cross_platform_target TARGET_NAME)
    # Apply platform-specific compiler flags
    apply_platform_compiler_flags(${TARGET_NAME})

    # Apply platform-specific linker flags
    apply_platform_linker_flags(${TARGET_NAME})

    # Link platform-specific libraries
    if(ATOM_PLATFORM_LIBRARIES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${ATOM_PLATFORM_LIBRARIES})
    endif()

    # Set target properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )

    # Platform-specific properties
    if(ATOM_PLATFORM_WINDOWS)
        set_target_properties(${TARGET_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()

    message(STATUS "Cross-platform configuration applied to ${TARGET_NAME}")
endfunction()

# -----------------------------------------------------------------------------
# Main Setup Function
# -----------------------------------------------------------------------------
function(setup_cross_platform_support)
    message(STATUS "Setting up cross-platform support...")

    # Detect platform capabilities
    detect_platform_capabilities()

    # Detect compiler features
    detect_compiler_features()

    # Detect platform-specific libraries
    detect_platform_libraries()

    message(STATUS "Cross-platform support configured")
endfunction()
