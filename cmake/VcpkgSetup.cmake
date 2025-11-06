# cmake/VcpkgSetup.cmake This script configures vcpkg integration for Atom
# project It detects vcpkg installation and sets up appropriate triplets

if(NOT USE_VCPKG)
  message(STATUS "USE_VCPKG is OFF. Skipping vcpkg setup.")
  return()
endif()

message(STATUS "Setting up vcpkg integration...")

if(UPDATE_VCPKG_BASELINE)
  include(cmake/UpdateBaseline.cmake)
endif()
set(VCPKG_INSTALL_OPTIONS
    "--no-print-usage"
    CACHE STRING "Additional vcpkg install options")

# On MSVC builds, prefer Visual Studio generator for vcpkg port builds to avoid
# rc.exe issues
if(MSVC AND NOT DEFINED VCPKG_CMAKE_GENERATOR)
  set(VCPKG_CMAKE_GENERATOR
      "Visual Studio 17 2022"
      CACHE STRING "Generator used by vcpkg to build ports")
  message(STATUS "Configured VCPKG_CMAKE_GENERATOR=${VCPKG_CMAKE_GENERATOR}")
endif()

# Sanitize PATH to avoid MSYS2 tools interfering with MSVC/vcpkg on Windows
if(WIN32 AND MSVC)
  if(DEFINED ENV{PATH})
    string(REPLACE ";" ";" _atom_path_sep "$ENV{PATH}")
    set(_atom_new_path "")
    foreach(_p IN LISTS _atom_path_sep)
      if(NOT _p MATCHES ".*msys64.*")
        if(_atom_new_path STREQUAL "")
          set(_atom_new_path "${_p}")
        else()
          set(_atom_new_path "${_atom_new_path};${_p}")
        endif()
      endif()
    endforeach()
    if(NOT _atom_new_path STREQUAL "")
      set(ENV{PATH} "${_atom_new_path}")
      message(STATUS "Sanitized PATH for MSVC/vcpkg to remove MSYS2 entries")
    endif()
    unset(_atom_new_path)
    unset(_atom_path_sep)
  endif()
endif()

if(DEFINED ENV{MSYSTEM})
  message(STATUS "MSYS2 environment detected by VcpkgSetup: $ENV{MSYSTEM}")
  set(ATOM_MSYS2_ENV
      TRUE
      CACHE INTERNAL "Flag indicating MSYS2 environment")
else()
  set(ATOM_MSYS2_ENV
      FALSE
      CACHE INTERNAL "Flag indicating MSYS2 environment")
endif()

if(DEFINED ENV{VCPKG_ROOT})
  set(POTENTIAL_VCPKG_PATH "$ENV{VCPKG_ROOT}")
  message(
    STATUS
      "Found vcpkg from VCPKG_ROOT environment variable: ${POTENTIAL_VCPKG_PATH}"
  )
else()
  if(ATOM_MSYS2_ENV)
    if(EXISTS "$ENV{USERPROFILE}/vcpkg") # Windows paths under MSYS2
      set(POTENTIAL_VCPKG_PATH "$ENV{USERPROFILE}/vcpkg")
    elseif(EXISTS "$ENV{HOME}/vcpkg") # Unix-like paths under MSYS2
      set(POTENTIAL_VCPKG_PATH "$ENV{HOME}/vcpkg")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
    elseif(EXISTS "/mingw64/vcpkg")
      set(POTENTIAL_VCPKG_PATH "/mingw64/vcpkg")
    endif()
  elseif(WIN32)
    if(EXISTS "C:/vcpkg")
      set(POTENTIAL_VCPKG_PATH "C:/vcpkg")
    elseif(EXISTS "$ENV{USERPROFILE}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "$ENV{USERPROFILE}/vcpkg")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
    endif()
  else() # Linux/macOS
    if(EXISTS "/usr/local/vcpkg")
      set(POTENTIAL_VCPKG_PATH "/usr/local/vcpkg")
    elseif(EXISTS "$ENV{HOME}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "$ENV{HOME}/vcpkg")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
      set(POTENTIAL_VCPKG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg")
    endif()
  endif()
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED POTENTIAL_VCPKG_PATH
     AND EXISTS "${POTENTIAL_VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE
        "${POTENTIAL_VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Set vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    if(NOT DEFINED ENV{VCPKG_ROOT})
      set(ENV{VCPKG_ROOT} "${POTENTIAL_VCPKG_PATH}") # Set for vcpkg.cmake
                                                     # script
      message(
        STATUS
          "Set VCPKG_ROOT environment variable for this CMake run: ${POTENTIAL_VCPKG_PATH}"
      )
    endif()
  else()
    message(
      FATAL_ERROR
        "USE_VCPKG is ON but vcpkg toolchain was not found. Searched VCPKG_ROOT or common paths. "
        "Please install vcpkg, set VCPKG_ROOT, or provide CMAKE_TOOLCHAIN_FILE."
    )
  endif()
else()
  message(
    STATUS
      "CMAKE_TOOLCHAIN_FILE is already set: ${CMAKE_TOOLCHAIN_FILE}. Assuming vcpkg is configured."
  )
  if(NOT DEFINED ENV{VCPKG_ROOT}
     AND CMAKE_TOOLCHAIN_FILE MATCHES
         "(.*/vcpkg)/scripts/buildsystems/vcpkg.cmake")
    set(POTENTIAL_VCPKG_PATH "${CMAKE_MATCH_1}")
    set(ENV{VCPKG_ROOT} "${POTENTIAL_VCPKG_PATH}")
    message(
      STATUS
        "Inferred and set VCPKG_ROOT from CMAKE_TOOLCHAIN_FILE: ${POTENTIAL_VCPKG_PATH}"
    )
  endif()

  # Enable tests-related manifest features so gtest is installed when building
  # tests
  if(ATOM_BUILD_TESTS)
    set(ENV{VCPKG_MANIFEST_FEATURES} "tests")
    message(STATUS "VCPKG_MANIFEST_FEATURES set to 'tests' for gtest support")
  endif()

endif()

# Ensure POTENTIAL_VCPKG_PATH is set for subsequent scripts if vcpkg is used
if(DEFINED POTENTIAL_VCPKG_PATH AND EXISTS "${POTENTIAL_VCPKG_PATH}")
  set(ATOM_VCPKG_ROOT
      "${POTENTIAL_VCPKG_PATH}"
      CACHE INTERNAL "Detected vcpkg root directory")
  message(STATUS "vcpkg setup complete. Vcpkg root: ${ATOM_VCPKG_ROOT}")
elseif(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}")
  set(ATOM_VCPKG_ROOT
      "$ENV{VCPKG_ROOT}"
      CACHE INTERNAL "Detected vcpkg root directory")
  message(STATUS "vcpkg setup complete. Vcpkg root: ${ATOM_VCPKG_ROOT}")
else()
  message(
    FATAL_ERROR
      "Vcpkg root directory (ATOM_VCPKG_ROOT) could not be determined. "
      "Ensure VCPKG_ROOT is set or vcpkg is in a standard location, or CMAKE_TOOLCHAIN_FILE points to vcpkg."
  )
endif()
