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

if(UNIX AND NOT APPLE)
    # Enable ccache if available, with enhanced error handling and user guidance
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "ccache found: enabling compiler cache support at ${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "C compiler launcher" FORCE)
        if(NOT CMAKE_C_COMPILER_LAUNCHER STREQUAL CCACHE_PROGRAM)
            message(WARNING "Failed to set CMAKE_C_COMPILER_LAUNCHER to ccache. Please check your CMake version and permissions.")
        endif()
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "CXX compiler launcher" FORCE)
        if(NOT CMAKE_CXX_COMPILER_LAUNCHER STREQUAL CCACHE_PROGRAM)
            message(WARNING "Failed to set CMAKE_CXX_COMPILER_LAUNCHER to ccache. Please check your CMake version and permissions.")
        endif()
    else()
        message(WARNING "ccache not found: compiler cache support disabled.\nRecommendation: On Linux, you can install ccache via package manager, e.g.: sudo apt install ccache or sudo yum install ccache")
    endif()
endif()