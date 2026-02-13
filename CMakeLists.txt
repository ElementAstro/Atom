# CMakeLists.txt for Atom Project Licensed under GPL3 Author: Max Qian

cmake_minimum_required(VERSION 3.21)

# Allow configuring older third-party projects (e.g., googletest) with modern
# CMake See error: "Compatibility with CMake < 3.5 has been removed from CMake"
# Setting this minimum policy version enables compatibility for subprojects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Set a global policy to handle malformed package configurations
if(POLICY CMP0000)
  cmake_policy(SET CMP0000 NEW)
endif()

# Set minimum policy version to handle system package issues
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW)

project(
  Atom
  LANGUAGES C CXX
  VERSION 0.1.0
  DESCRIPTION "Foundational library for astronomical software"
  HOMEPAGE_URL "https://github.com/ElementAstro/Atom")

# -----------------------------------------------------------------------------
# Use shared libraries for fmt to avoid ODR violations
# -----------------------------------------------------------------------------
set(fmt_SHARED_LIBS
    ON
    CACHE BOOL "Use fmt shared library")

# -----------------------------------------------------------------------------
# Use compiled spdlog library to avoid ODR violations This ensures all modules
# use the same spdlog library (not header-only)
# -----------------------------------------------------------------------------
set(ATOM_SPDLOG_TARGET
    "spdlog::spdlog"
    CACHE STRING "spdlog target to use")
add_compile_definitions(SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL)

# -----------------------------------------------------------------------------
# Include CMake Modules
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Check if required cmake modules exist before including them
set(REQUIRED_CMAKE_MODULES
    GitVersion.cmake
    VersionConfig.cmake
    PlatformSpecifics.cmake
    CompilerOptions.cmake
    BuildOptimization.cmake
    Options.cmake
    ModuleDependencies.cmake
    ExamplesBuildOptions.cmake
    TestsBuildOptions.cmake
    ScanModule.cmake
    PackagingConfig.cmake
    ModularInstall.cmake
    CopyDependencies.cmake)

foreach(MODULE ${REQUIRED_CMAKE_MODULES})
  set(MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${MODULE}")
  if(EXISTS "${MODULE_PATH}")
    include(cmake/${MODULE})
    message(STATUS "Included CMake module: ${MODULE}")
  else()
    message(WARNING "CMake module not found: ${MODULE_PATH}")
  endif()
endforeach()

# -----------------------------------------------------------------------------
# Build Optimization Setup
# -----------------------------------------------------------------------------

# Setup build optimizations (ccache, PCH, Unity Build, fast linking) Options are
# defined in BuildOptimization.cmake: ATOM_ENABLE_CCACHE, ATOM_ENABLE_PCH,
# ATOM_ENABLE_UNITY_BUILD, ATOM_ENABLE_FAST_LINK
if(COMMAND atom_setup_build_optimizations)
  atom_setup_build_optimizations()
endif()

# Process module groups (defined in Options.cmake)
if(COMMAND atom_process_module_groups)
  atom_process_module_groups()
endif()

# Optional feature dependencies (large libraries moved to features)
option(ATOM_USE_OPENCV "Enable OpenCV for image processing" OFF)
option(ATOM_USE_TBB "Enable Intel TBB for parallel algorithms" OFF)
option(ATOM_USE_MINIZIP "Enable minizip-ng for advanced compression" OFF)
option(ATOM_USE_LIBUV "Enable libuv for async I/O" OFF)

# Build size optimization
option(ATOM_STRIP_BINARIES "Strip debug symbols from release binaries" ON)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(USE_VCPKG "Use vcpkg package manager" OFF)
# Force vcpkg ON for MSVC builds (but not MSYS2/MinGW)
if(WIN32
   AND CMAKE_CXX_COMPILER_ID MATCHES "MSVC"
   AND NOT DEFINED ENV{MSYSTEM})
  # Ensure vcpkg is enabled for MSVC builds only (not MSYS2)
  set(USE_VCPKG
      ON
      CACHE BOOL "Enable vcpkg for MSVC" FORCE)
  message(STATUS "Enabling vcpkg for MSVC builds")
endif()
option(UPDATE_VCPKG_BASELINE "Update vcpkg baseline to latest" OFF)
option(ATOM_BUILD_EXAMPLES "Build examples" OFF)
option(ATOM_BUILD_EXAMPLES_SELECTIVE
       "Enable selective building of example modules" OFF)
option(ATOM_BUILD_TESTS "Build tests" OFF)
option(ATOM_BUILD_TESTS_SELECTIVE "Enable selective building of test modules"
       OFF)
option(ATOM_BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(ATOM_BUILD_DOCS "Build documentation" OFF)
option(ATOM_ENABLE_PACKAGING "Enable CPack packaging configuration" OFF)
option(ATOM_USE_BOOST "Enable Boost high-performance data structures" OFF)
option(ATOM_USE_BOOST_LOCKFREE "Enable Boost lock-free data structures" OFF)
option(ATOM_USE_BOOST_CONTAINER "Enable Boost container library" OFF)
option(ATOM_USE_BOOST_GRAPH "Enable Boost graph library" OFF)
option(ATOM_USE_BOOST_INTRUSIVE "Enable Boost intrusive containers" OFF)
option(ATOM_USE_PYBIND11 "Enable pybind11 support"
       ${ATOM_BUILD_PYTHON_BINDINGS})
option(ATOM_USE_SSH "Enable SSH support" OFF)
option(ATOM_BUILD_ALL "Build all Atom modules" ON)

# Option: Pretty/less noisy configure & build output
option(ATOM_PRETTY_OUTPUT "Pretty, less noisy configure/build output" ON)
if(ATOM_PRETTY_OUTPUT)
  set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
  set(CMAKE_RULE_MESSAGES
      OFF
      CACHE BOOL "" FORCE)
  set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)
  set(CMAKE_COLOR_DIAGNOSTICS
      ON
      CACHE BOOL "" FORCE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fdiagnostics-color=always)
  endif()
endif()

# Module build options
foreach(
  MODULE
  ALGORITHM
  ASYNC
  COMPONENTS
  CONNECTION
  CONTAINERS
  ERROR
  IMAGE
  IO
  LOG
  MEMORY
  META
  SEARCH
  SECRET
  SERIAL
  SYSINFO
  SYSTEM
  TYPE
  UTILS
  WEB)
  option(ATOM_BUILD_${MODULE} "Build ${MODULE} module" ${ATOM_BUILD_ALL})
endforeach()

# Option to enable automatic dependency resolution
option(ATOM_AUTO_RESOLVE_DEPS "Automatically enable module dependencies" ON)

# -----------------------------------------------------------------------------
# C++ Standard
# -----------------------------------------------------------------------------
# Prefer C++23, but fall back to C++20 when compiler lacks full support
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
    set(CMAKE_CXX_STANDARD 20)
  else()
    set(CMAKE_CXX_STANDARD 23)
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # MSVC 2022 supports C++20 well, C++23 support is still experimental
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.29")
    set(CMAKE_CXX_STANDARD 20)
  else()
    set(CMAKE_CXX_STANDARD 20) # Use C++20 for now until C++23 is stable
  endif()
else()
  set(CMAKE_CXX_STANDARD 20) # Safe fallback
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Compiler Configuration
# -----------------------------------------------------------------------------
# Setup compiler-specific options based on build type and compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  message(STATUS "Configuring for MSVC compiler")
  # Work around Windows SDK winnt.h arch detection under some environments
  add_compile_definitions(ATOM_DISABLE_DBGHELP _AMD64_)
  # Call compiler configuration from compiler_options.cmake
  setup_project_defaults(
    CXX_STANDARD
    ${CMAKE_CXX_STANDARD}
    MIN_MSVC_VERSION
    19.28
    ENABLE_PCH
    PCH_HEADERS
    <iostream>
    <string>
    <vector>
    <memory>)
else()
  message(STATUS "Configuring for non-MSVC compiler: ${CMAKE_CXX_COMPILER_ID}")
  # Call compiler configuration for other compilers
  setup_project_defaults(CXX_STANDARD ${CMAKE_CXX_STANDARD} MIN_GCC_VERSION
                         10.0 MIN_CLANG_VERSION 10.0)
endif()

# -----------------------------------------------------------------------------
# Version Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(ATOM_VERSION="${PROJECT_VERSION}"
                        ATOM_VERSION_STRING="${PROJECT_VERSION}")

# Windows API version definitions
if(WIN32)
  add_compile_definitions(
    _WIN32_WINNT=0x0A00 # Windows 10
    WINVER=0x0A00
    _WIN32_WINDOWS=0x0A00
    NOMINMAX # Prevent Windows.h from defining min/max macros
    WIN32_LEAN_AND_MEAN # Exclude rarely-used stuff from Windows headers
  )
endif()

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/extra
                    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} .)

# -----------------------------------------------------------------------------
# Custom Targets
# -----------------------------------------------------------------------------
add_custom_target(
  AtomCmakeAdditionalFiles
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake
          ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake)

# -----------------------------------------------------------------------------
# Package Management
# -----------------------------------------------------------------------------
if(USE_VCPKG)
  message(STATUS "Using vcpkg package manager.")
  include(cmake/VcpkgSetup.cmake)
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
  set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
  set(CMAKE_FIND_USE_CMAKE_PATH ON)
else()
  message(STATUS "Not using vcpkg package manager.")
endif()

# -----------------------------------------------------------------------------
# Dependency Packages
# -----------------------------------------------------------------------------
message(STATUS "Finding dependency packages...")

# Use standardized dependency finding
include(cmake/FindDependencies.cmake)

# SSH support and Python bindings are now handled by FindDependencies.cmake

# -----------------------------------------------------------------------------
# Automatic Dependency Resolution
# -----------------------------------------------------------------------------
if(ATOM_AUTO_RESOLVE_DEPS)
  message(STATUS "Automatic dependency resolution enabled")
  include(cmake/ScanModule.cmake)
  atom_resolve_all_dependencies()
  # Process module dependencies to enable required modules
  if(COMMAND atom_process_module_dependencies)
    atom_process_module_dependencies()
  endif()
endif()

# -----------------------------------------------------------------------------
# Ninja Generator Support
# -----------------------------------------------------------------------------
if(CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_GENERATOR MATCHES "Ninja")
  message(
    STATUS "Ninja generator detected. Enabling Ninja-specific optimizations.")
  set(CMAKE_EXPORT_COMPILE_COMMANDS
      ON
      CACHE BOOL "Enable compile_commands.json for Ninja" FORCE)
endif()

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(atom)
if(ATOM_BUILD_EXAMPLES)
  add_subdirectory(example)
endif()
add_subdirectory(extra)
if(ATOM_BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()
if(ATOM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Documentation
# -----------------------------------------------------------------------------
if(ATOM_BUILD_DOCS)
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    message(STATUS "Doxygen found. Documentation generation enabled.")
    # Configure Doxygen options here if needed
  else()
    message(WARNING "Doxygen not found. Documentation will not be generated.")
    set(ATOM_BUILD_DOCS OFF)
  endif()
endif()

# -----------------------------------------------------------------------------
# Component Registration
# -----------------------------------------------------------------------------

# Register all Atom components for modular installation (if function exists)
if(COMMAND atom_register_component)
  # Include module dependencies data to get ATOM_ALL_MODULES
  include(cmake/ModuleDependenciesData.cmake)
  foreach(MODULE ${ATOM_ALL_MODULES})
    # Extract module name without atom- prefix
    string(REPLACE "atom-" "" MODULE_NAME ${MODULE})
    string(TOUPPER ${MODULE_NAME} MODULE_UPPER)
    atom_register_component(
      ${MODULE_NAME}
      DESCRIPTION
      "Atom ${MODULE_NAME} module"
      VERSION
      ${PROJECT_VERSION}
      DEPENDS
      ${ATOM_COMPONENT_DEPS_${MODULE_NAME}})
  endforeach()

  # Setup modular installation system
  if(COMMAND atom_setup_modular_installation)
    atom_setup_modular_installation()
  endif()
endif()

# -----------------------------------------------------------------------------
# Packaging Configuration
# -----------------------------------------------------------------------------

# Setup CPack for package generation when explicitly enabled
if(ATOM_ENABLE_PACKAGING AND COMMAND atom_setup_cpack)
  atom_setup_cpack()
endif()

# Create modular packages
if(ATOM_INSTALL_COMPONENT_PACKAGES AND COMMAND atom_create_modular_packages)
  atom_create_modular_packages()
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/atom/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
  PATTERN "**/internal" EXCLUDE
  PATTERN "**/tests" EXCLUDE
  PATTERN "**/example" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom)

# -----------------------------------------------------------------------------
# Build Size Optimization
# -----------------------------------------------------------------------------

# Strip binaries in Release/MinSizeRel builds
if(ATOM_STRIP_BINARIES AND NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Add strip flag to linker
    add_link_options(-s)
    message(STATUS "Binary stripping enabled for release builds")
  endif()
endif()

# Custom clean target for thorough cleanup
add_custom_target(
  clean-all
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/atom
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/tests
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/example
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/extra
  COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/*.a
  COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/*.lib
  COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/*.dll
  COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/*.exe
  COMMENT "Deep cleaning build artifacts...")

# -----------------------------------------------------------------------------
# Configuration Validation & Summary
# -----------------------------------------------------------------------------
# Validate cmake configuration and print build summary
if(COMMAND atom_validate_cmake_config)
  atom_validate_cmake_config()
endif()

if(COMMAND atom_print_build_summary)
  atom_print_build_summary()
endif()

# -----------------------------------------------------------------------------
# IDE Folders & Final Message
# -----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
message(STATUS "Atom configured successfully")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
