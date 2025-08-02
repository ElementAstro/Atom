# cmake/PlatformSpecifics.cmake
# This script handles platform-specific configurations and compiler settings
# It detects the build environment and applies appropriate settings

# ATOM_MSYS2_ENV is set by VcpkgSetup.cmake if USE_VCPKG is ON.
# If USE_VCPKG is OFF, VcpkgSetup might not run, so check ENV{MSYSTEM} again.
if(NOT DEFINED ATOM_MSYS2_ENV)
    if(DEFINED ENV{MSYSTEM})
        set(LOCAL_MSYS2_ENV TRUE)
    else()
        set(LOCAL_MSYS2_ENV FALSE)
    endif()
else()
    set(LOCAL_MSYS2_ENV ${ATOM_MSYS2_ENV})
endif()

if(LOCAL_MSYS2_ENV)
    message(STATUS "Applying MSYS2 specific configurations.")
    add_compile_definitions(MSYS2_BUILD)
endif()

if(MINGW OR LOCAL_MSYS2_ENV)
    message(STATUS "MinGW/MSYS2 environment detected for platform specifics.")
    add_compile_definitions(MINGW_BUILD)

    if(USE_VCPKG)
        if(NOT DEFINED ATOM_VCPKG_ROOT)
            message(FATAL_ERROR "ATOM_VCPKG_ROOT is not defined for MinGW/MSYS2 vcpkg setup. VcpkgSetup.cmake might not have run correctly.")
        endif()
        set(CURRENT_VCPKG_ROOT ${ATOM_VCPKG_ROOT})

        if(NOT DEFINED VCPKG_TARGET_TRIPLET)
            message(STATUS "VCPKG_TARGET_TRIPLET not defined, attempting to set default for MinGW.")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
                set(ARCH_SUFFIX "x64")
                if(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/community/x64-mingw-dynamic.cmake")
                    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "Vcpkg target triplet for MinGW")
                elseif(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/community/x64-mingw-static.cmake")
                    set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "Vcpkg target triplet for MinGW")
                elseif(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/x64-mingw.cmake")
                     set(VCPKG_TARGET_TRIPLET "x64-mingw" CACHE STRING "Vcpkg target triplet for MinGW")
                else()
                    message(STATUS "No pre-existing x64-mingw triplet found. Creating x64-mingw-dynamic.cmake.")
                    file(MAKE_DIRECTORY "${CURRENT_VCPKG_ROOT}/triplets/community")
                    file(WRITE "${CURRENT_VCPKG_ROOT}/triplets/community/x64-mingw-dynamic.cmake"
                        "set(VCPKG_TARGET_ARCHITECTURE x64)\nset(VCPKG_CRT_LINKAGE dynamic)\nset(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_CMAKE_SYSTEM_NAME MinGW)\n")
                    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "Vcpkg target triplet for MinGW")
                endif()
            else() # 32-bit
                set(ARCH_SUFFIX "x86")
                if(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/community/x86-mingw-dynamic.cmake")
                    set(VCPKG_TARGET_TRIPLET "x86-mingw-dynamic" CACHE STRING "Vcpkg target triplet for MinGW")
                elseif(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/community/x86-mingw-static.cmake")
                    set(VCPKG_TARGET_TRIPLET "x86-mingw-static" CACHE STRING "Vcpkg target triplet for MinGW")
                elseif(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/x86-mingw.cmake")
                    set(VCPKG_TARGET_TRIPLET "x86-mingw" CACHE STRING "Vcpkg target triplet for MinGW")
                else()
                    message(STATUS "No pre-existing x86-mingw triplet found. Creating x86-mingw-dynamic.cmake.")
                    file(MAKE_DIRECTORY "${CURRENT_VCPKG_ROOT}/triplets/community")
                    file(WRITE "${CURRENT_VCPKG_ROOT}/triplets/community/x86-mingw-dynamic.cmake"
                        "set(VCPKG_TARGET_ARCHITECTURE x86)\nset(VCPKG_CRT_LINKAGE dynamic)\nset(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_CMAKE_SYSTEM_NAME MinGW)\n")
                    set(VCPKG_TARGET_TRIPLET "x86-mingw-dynamic" CACHE STRING "Vcpkg target triplet for MinGW")
                endif()
            endif()
            message(STATUS "Set default VCPKG_TARGET_TRIPLET for MinGW (${ARCH_SUFFIX}): ${VCPKG_TARGET_TRIPLET}")
        else()
            message(STATUS "VCPKG_TARGET_TRIPLET is already set to: ${VCPKG_TARGET_TRIPLET}")
        endif()

        if(EXISTS "${CURRENT_VCPKG_ROOT}\\vcpkg${CMAKE_EXECUTABLE_SUFFIX}") # Check for base name, suffix added by CMAKE_EXECUTABLE_SUFFIX
            message(STATUS "Checking and installing vcpkg dependencies for triplet ${VCPKG_TARGET_TRIPLET}...")
            set(VCPKG_EXE_BASE_PATH "${CURRENT_VCPKG_ROOT}\\vcpkg")

            if(LOCAL_MSYS2_ENV AND WIN32 AND EXISTS "${VCPKG_EXE_BASE_PATH}.exe")
                set(VCPKG_COMMAND_TO_RUN cmd.exe /c "${VCPKG_EXE_BASE_PATH}.exe")
                message(STATUS "Using cmd.exe for vcpkg.exe in MSYS2/Windows environment.")
            else()
                set(VCPKG_COMMAND_TO_RUN "${VCPKG_EXE_BASE_PATH}${CMAKE_EXECUTABLE_SUFFIX}")
                message(STATUS "Using direct execution for vcpkg: ${VCPKG_COMMAND_TO_RUN}")
            endif()

            message(STATUS "Executing vcpkg install: ${VCPKG_COMMAND_TO_RUN} install --triplet=${VCPKG_TARGET_TRIPLET} ${VCPKG_INSTALL_OPTIONS}")
            execute_process(
                COMMAND ${VCPKG_COMMAND_TO_RUN} install --triplet=${VCPKG_TARGET_TRIPLET} ${VCPKG_INSTALL_OPTIONS} --allow-unsupported
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                RESULT_VARIABLE VCPKG_INSTALL_RESULT
                OUTPUT_VARIABLE VCPKG_INSTALL_OUTPUT
                ERROR_VARIABLE VCPKG_INSTALL_ERROR
            )
            if(NOT VCPKG_INSTALL_RESULT EQUAL 0)
                message(WARNING "vcpkg dependency installation failed. Return code: ${VCPKG_INSTALL_RESULT}")
                message(WARNING "Output:\n${VCPKG_INSTALL_OUTPUT}")
                message(WARNING "Error:\n${VCPKG_INSTALL_ERROR}")
                # Suggest manual command
            else()
                message(STATUS "vcpkg dependencies installed successfully for triplet ${VCPKG_TARGET_TRIPLET}.")
            endif()
        else()
            message(WARNING "vcpkg executable not found at ${CURRENT_VCPKG_ROOT}/vcpkg${CMAKE_EXECUTABLE_SUFFIX} Skipping dependency installation.")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Cross-Platform Compiler Cache Setup
# -----------------------------------------------------------------------------
if(ATOM_ENABLE_CCACHE)
    # Try to find ccache or sccache
    find_program(CCACHE_PROGRAM ccache)
    find_program(SCCACHE_PROGRAM sccache)

    set(CACHE_PROGRAM "")
    if(CCACHE_PROGRAM)
        set(CACHE_PROGRAM ${CCACHE_PROGRAM})
        set(CACHE_NAME "ccache")
    elseif(SCCACHE_PROGRAM)
        set(CACHE_PROGRAM ${SCCACHE_PROGRAM})
        set(CACHE_NAME "sccache")
    endif()

    if(CACHE_PROGRAM)
        message(STATUS "${CACHE_NAME} found: enabling compiler cache support at ${CACHE_PROGRAM}")

        # Configure cache settings
        if(CACHE_NAME STREQUAL "ccache")
            # Set ccache configuration for optimal performance
            set(ENV{CCACHE_MAXSIZE} "5G")
            set(ENV{CCACHE_COMPRESS} "1")
            set(ENV{CCACHE_COMPRESSLEVEL} "6")
            set(ENV{CCACHE_SLOPPINESS} "file_macro,locale,time_macros")
        endif()

        set(CMAKE_C_COMPILER_LAUNCHER ${CACHE_PROGRAM} CACHE STRING "C compiler launcher" FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CACHE_PROGRAM} CACHE STRING "CXX compiler launcher" FORCE)

        # Verify the setup worked
        if(NOT CMAKE_C_COMPILER_LAUNCHER STREQUAL CACHE_PROGRAM)
            message(WARNING "Failed to set CMAKE_C_COMPILER_LAUNCHER to ${CACHE_NAME}. Please check your CMake version and permissions.")
        endif()
        if(NOT CMAKE_CXX_COMPILER_LAUNCHER STREQUAL CACHE_PROGRAM)
            message(WARNING "Failed to set CMAKE_CXX_COMPILER_LAUNCHER to ${CACHE_NAME}. Please check your CMake version and permissions.")
        endif()
    else()
        message(WARNING "No compiler cache found (ccache/sccache): compilation caching disabled.")
        if(UNIX AND NOT APPLE)
            message(STATUS "Recommendation: Install ccache via package manager, e.g.: sudo apt install ccache or sudo yum install ccache")
        elseif(APPLE)
            message(STATUS "Recommendation: Install ccache via Homebrew: brew install ccache")
        elseif(WIN32)
            message(STATUS "Recommendation: Install sccache from https://github.com/mozilla/sccache/releases")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Platform-Specific Optimizations
# -----------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    # Linux-specific optimizations
    add_compile_definitions(PLATFORM_LINUX)

    # Enable GNU-specific optimizations
    if(CMAKE_COMPILER_IS_GNUCXX)
        add_compile_options(-fstack-protector-strong)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_compile_options(-march=native -mtune=native)
        endif()
    endif()

elseif(APPLE)
    # macOS-specific optimizations
    add_compile_definitions(PLATFORM_MACOS)

    # Set minimum macOS version for better compatibility
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")

    # Use libc++ on macOS
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

elseif(WIN32)
    # Windows-specific optimizations
    add_compile_definitions(PLATFORM_WINDOWS)
    add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10
    add_compile_definitions(NOMINMAX) # Prevent min/max macro conflicts
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

    # Enable parallel compilation on MSVC
    if(MSVC)
        add_compile_options(/MP)
        # Use faster PDB generation
        add_compile_options(/Zi)
        add_link_options(/DEBUG:FASTLINK)
    endif()
endif()
