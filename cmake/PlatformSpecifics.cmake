# cmake/PlatformSpecifics.cmake This script handles platform-specific
# configurations and compiler settings It detects the build environment and
# applies appropriate settings

include_guard(GLOBAL)

# =============================================================================
# Unified Platform Detection
# =============================================================================
# This is the single source of truth for platform detection in the Atom project.
# Other cmake files should use these variables instead of duplicating detection.

# Detect platform type
if(WIN32)
  set(ATOM_PLATFORM
      "windows"
      CACHE STRING "Detected platform")
  set(ATOM_PLATFORM_WINDOWS
      TRUE
      CACHE BOOL "Windows platform")
  set(ATOM_PLATFORM_UNIX
      FALSE
      CACHE BOOL "Unix platform")
  set(ATOM_PLATFORM_APPLE
      FALSE
      CACHE BOOL "Apple platform")
elseif(APPLE)
  set(ATOM_PLATFORM
      "macos"
      CACHE STRING "Detected platform")
  set(ATOM_PLATFORM_WINDOWS
      FALSE
      CACHE BOOL "Windows platform")
  set(ATOM_PLATFORM_UNIX
      TRUE
      CACHE BOOL "Unix platform")
  set(ATOM_PLATFORM_APPLE
      TRUE
      CACHE BOOL "Apple platform")
elseif(UNIX)
  set(ATOM_PLATFORM
      "linux"
      CACHE STRING "Detected platform")
  set(ATOM_PLATFORM_WINDOWS
      FALSE
      CACHE BOOL "Windows platform")
  set(ATOM_PLATFORM_UNIX
      TRUE
      CACHE BOOL "Unix platform")
  set(ATOM_PLATFORM_APPLE
      FALSE
      CACHE BOOL "Apple platform")
else()
  set(ATOM_PLATFORM
      "unknown"
      CACHE STRING "Detected platform")
  set(ATOM_PLATFORM_WINDOWS
      FALSE
      CACHE BOOL "Windows platform")
  set(ATOM_PLATFORM_UNIX
      FALSE
      CACHE BOOL "Unix platform")
  set(ATOM_PLATFORM_APPLE
      FALSE
      CACHE BOOL "Apple platform")
endif()

# Detect architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ATOM_ARCH
      "x64"
      CACHE STRING "Detected architecture")
  set(ATOM_ARCH_64BIT
      TRUE
      CACHE BOOL "64-bit architecture")
else()
  set(ATOM_ARCH
      "x86"
      CACHE STRING "Detected architecture")
  set(ATOM_ARCH_64BIT
      FALSE
      CACHE BOOL "64-bit architecture")
endif()

# Function to apply platform-specific compile definitions
function(atom_apply_platform_definitions)
  if(ATOM_PLATFORM_WINDOWS)
    add_definitions(-DPLATFORM_WINDOWS)
    if(MSVC)
      add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
  elseif(ATOM_PLATFORM_APPLE)
    add_definitions(-DPLATFORM_MACOS)
  elseif(ATOM_PLATFORM_UNIX)
    add_definitions(-DPLATFORM_LINUX)
  endif()

  if(ATOM_ARCH_64BIT)
    add_definitions(-DARCH_X64)
  else()
    add_definitions(-DARCH_X86)
  endif()
endfunction()

# =============================================================================
# MSYS2/MinGW Detection
# =============================================================================

# ATOM_MSYS2_ENV is set by VcpkgSetup.cmake if USE_VCPKG is ON. If USE_VCPKG is
# OFF, VcpkgSetup might not run, so check ENV{MSYSTEM} again.
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
      # Defer vcpkg-specific setup until VcpkgSetup.cmake runs (included later
      # in CMakeLists)
      message(
        STATUS
          "ATOM_VCPKG_ROOT is not defined yet; deferring vcpkg-specific setup to VcpkgSetup.cmake"
      )
      return()
    endif()
    set(CURRENT_VCPKG_ROOT ${ATOM_VCPKG_ROOT})

    if(NOT DEFINED VCPKG_TARGET_TRIPLET)
      message(
        STATUS
          "VCPKG_TARGET_TRIPLET not defined, attempting to set default for MinGW."
      )
      if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
        set(ARCH_SUFFIX "x64")
        if(EXISTS
           "${CURRENT_VCPKG_ROOT}/triplets/community/x64-mingw-dynamic.cmake")
          set(VCPKG_TARGET_TRIPLET
              "x64-mingw-dynamic"
              CACHE STRING "Vcpkg target triplet for MinGW")
        elseif(
          EXISTS
          "${CURRENT_VCPKG_ROOT}/triplets/community/x64-mingw-static.cmake")
          set(VCPKG_TARGET_TRIPLET
              "x64-mingw-static"
              CACHE STRING "Vcpkg target triplet for MinGW")
        elseif(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/x64-mingw.cmake")
          set(VCPKG_TARGET_TRIPLET
              "x64-mingw"
              CACHE STRING "Vcpkg target triplet for MinGW")
        else()
          message(
            STATUS
              "No pre-existing x64-mingw triplet found. Creating x64-mingw-dynamic.cmake."
          )
          file(MAKE_DIRECTORY "${CURRENT_VCPKG_ROOT}/triplets/community")
          file(
            WRITE
            "${CURRENT_VCPKG_ROOT}/triplets/community/x64-mingw-dynamic.cmake"
            "set(VCPKG_TARGET_ARCHITECTURE x64)\nset(VCPKG_CRT_LINKAGE dynamic)\nset(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_CMAKE_SYSTEM_NAME MinGW)\n"
          )
          set(VCPKG_TARGET_TRIPLET
              "x64-mingw-dynamic"
              CACHE STRING "Vcpkg target triplet for MinGW")
        endif()
      else() # 32-bit
        set(ARCH_SUFFIX "x86")
        if(EXISTS
           "${CURRENT_VCPKG_ROOT}/triplets/community/x86-mingw-dynamic.cmake")
          set(VCPKG_TARGET_TRIPLET
              "x86-mingw-dynamic"
              CACHE STRING "Vcpkg target triplet for MinGW")
        elseif(
          EXISTS
          "${CURRENT_VCPKG_ROOT}/triplets/community/x86-mingw-static.cmake")
          set(VCPKG_TARGET_TRIPLET
              "x86-mingw-static"
              CACHE STRING "Vcpkg target triplet for MinGW")
        elseif(EXISTS "${CURRENT_VCPKG_ROOT}/triplets/x86-mingw.cmake")
          set(VCPKG_TARGET_TRIPLET
              "x86-mingw"
              CACHE STRING "Vcpkg target triplet for MinGW")
        else()
          message(
            STATUS
              "No pre-existing x86-mingw triplet found. Creating x86-mingw-dynamic.cmake."
          )
          file(MAKE_DIRECTORY "${CURRENT_VCPKG_ROOT}/triplets/community")
          file(
            WRITE
            "${CURRENT_VCPKG_ROOT}/triplets/community/x86-mingw-dynamic.cmake"
            "set(VCPKG_TARGET_ARCHITECTURE x86)\nset(VCPKG_CRT_LINKAGE dynamic)\nset(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_CMAKE_SYSTEM_NAME MinGW)\n"
          )
          set(VCPKG_TARGET_TRIPLET
              "x86-mingw-dynamic"
              CACHE STRING "Vcpkg target triplet for MinGW")
        endif()
      endif()
      message(
        STATUS
          "Set default VCPKG_TARGET_TRIPLET for MinGW (${ARCH_SUFFIX}): ${VCPKG_TARGET_TRIPLET}"
      )
    else()
      message(
        STATUS "VCPKG_TARGET_TRIPLET is already set to: ${VCPKG_TARGET_TRIPLET}"
      )
    endif()

    if(EXISTS "${CURRENT_VCPKG_ROOT}\\vcpkg${CMAKE_EXECUTABLE_SUFFIX}"
    )# Check for base name, suffix added by CMAKE_EXECUTABLE_SUFFIX
      message(
        STATUS
          "Checking and installing vcpkg dependencies for triplet ${VCPKG_TARGET_TRIPLET}..."
      )
      set(VCPKG_EXE_BASE_PATH "${CURRENT_VCPKG_ROOT}\\vcpkg")

      if(LOCAL_MSYS2_ENV
         AND WIN32
         AND EXISTS "${VCPKG_EXE_BASE_PATH}.exe")
        set(VCPKG_COMMAND_TO_RUN cmd.exe /c "${VCPKG_EXE_BASE_PATH}.exe")
        message(
          STATUS "Using cmd.exe for vcpkg.exe in MSYS2/Windows environment.")
      else()
        set(VCPKG_COMMAND_TO_RUN
            "${VCPKG_EXE_BASE_PATH}${CMAKE_EXECUTABLE_SUFFIX}")
        message(
          STATUS "Using direct execution for vcpkg: ${VCPKG_COMMAND_TO_RUN}")
      endif()

      message(
        STATUS
          "Executing vcpkg install: ${VCPKG_COMMAND_TO_RUN} install --triplet=${VCPKG_TARGET_TRIPLET} ${VCPKG_INSTALL_OPTIONS}"
      )
      execute_process(
        COMMAND
          ${VCPKG_COMMAND_TO_RUN} install --triplet=${VCPKG_TARGET_TRIPLET}
          ${VCPKG_INSTALL_OPTIONS} --allow-unsupported
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE VCPKG_INSTALL_RESULT
        OUTPUT_VARIABLE VCPKG_INSTALL_OUTPUT
        ERROR_VARIABLE VCPKG_INSTALL_ERROR)
      if(NOT VCPKG_INSTALL_RESULT EQUAL 0)
        message(
          WARNING
            "vcpkg dependency installation failed. Return code: ${VCPKG_INSTALL_RESULT}"
        )
        message(WARNING "Output:\n${VCPKG_INSTALL_OUTPUT}")
        message(WARNING "Error:\n${VCPKG_INSTALL_ERROR}")
        # Suggest manual command
      else()
        message(
          STATUS
            "vcpkg dependencies installed successfully for triplet ${VCPKG_TARGET_TRIPLET}."
        )
      endif()
    else()
      message(
        WARNING
          "vcpkg executable not found at ${CURRENT_VCPKG_ROOT}/vcpkg${CMAKE_EXECUTABLE_SUFFIX} Skipping dependency installation."
      )
    endif()
  endif()
endif()

# NOTE: ccache/sccache support has been moved to BuildOptimization.cmake for
# cross-platform support. See atom_setup_compiler_cache() function.
