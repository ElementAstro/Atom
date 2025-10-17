# CMakeLists.txt for Atom Project
# Licensed under GPL3
# Author: Max Qian

cmake_minimum_required(VERSION 3.21)

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
  HOMEPAGE_URL "https://github.com/ElementAstro/Atom"
)

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
    ModuleDependencies.cmake
    ExamplesBuildOptions.cmake
    TestsBuildOptions.cmake
    ScanModule.cmake
    PackagingConfig.cmake
    ModularInstall.cmake
)

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
# Options
# -----------------------------------------------------------------------------
option(USE_VCPKG "Use vcpkg package manager" OFF)
set(USE_VCPKG OFF CACHE BOOL "Force disable vcpkg" FORCE)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Temporarily disable vcpkg due to network issues
    # set(USE_VCPKG ON CACHE BOOL "Enable vcpkg for MSVC" FORCE)
    # message(STATUS "Enabling vcpkg for MSVC builds")
else()
    # Keep user's preference for other platforms
endif()
option(UPDATE_VCPKG_BASELINE "Update vcpkg baseline to latest" OFF)
option(ATOM_BUILD_EXAMPLES "Build examples" ON)
option(ATOM_BUILD_EXAMPLES_SELECTIVE "Enable selective building of example modules" OFF)
option(ATOM_BUILD_TESTS "Build tests" OFF)
option(ATOM_BUILD_TESTS_SELECTIVE "Enable selective building of test modules" OFF)
option(ATOM_BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(ATOM_BUILD_DOCS "Build documentation" OFF)
option(ATOM_USE_BOOST "Enable Boost high-performance data structures" OFF)
option(ATOM_USE_BOOST_LOCKFREE "Enable Boost lock-free data structures" OFF)
option(ATOM_USE_BOOST_CONTAINER "Enable Boost container library" OFF)
option(ATOM_USE_BOOST_GRAPH "Enable Boost graph library" OFF)
option(ATOM_USE_BOOST_INTRUSIVE "Enable Boost intrusive containers" OFF)
option(ATOM_USE_PYBIND11 "Enable pybind11 support" ${ATOM_BUILD_PYTHON_BINDINGS})
option(ATOM_USE_SSH "Enable SSH support" OFF)
option(ATOM_BUILD_ALL "Build all Atom modules" ON)

# Module build options
foreach(MODULE
    ALGORITHM ASYNC COMPONENTS CONNECTION CONTAINERS ERROR IMAGE IO LOG MEMORY
    META SEARCH SECRET SERIAL SYSINFO SYSTEM TYPE UTILS WEB)
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
    set(CMAKE_CXX_STANDARD 20)  # Use C++20 for now until C++23 is stable
  endif()
else()
  set(CMAKE_CXX_STANDARD 20)  # Safe fallback
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Compiler Configuration
# -----------------------------------------------------------------------------
# Setup compiler-specific options based on build type and compiler
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    message(STATUS "Configuring for MSVC compiler")
    # Call compiler configuration from compiler_options.cmake
    setup_project_defaults(
        CXX_STANDARD ${CMAKE_CXX_STANDARD}
        MIN_MSVC_VERSION 19.28
        ENABLE_PCH
        PCH_HEADERS <iostream> <string> <vector> <memory>
    )
else()
    message(STATUS "Configuring for non-MSVC compiler: ${CMAKE_CXX_COMPILER_ID}")
    # Call compiler configuration for other compilers
    setup_project_defaults(
        CXX_STANDARD ${CMAKE_CXX_STANDARD}
        MIN_GCC_VERSION 10.0
        MIN_CLANG_VERSION 10.0
    )
endif()

# -----------------------------------------------------------------------------
# Version Definitions
# -----------------------------------------------------------------------------
add_compile_definitions(
  ATOM_VERSION="${PROJECT_VERSION}"
  ATOM_VERSION_STRING="${PROJECT_VERSION}"
)

# Windows API version definitions
if(WIN32)
  add_compile_definitions(
    _WIN32_WINNT=0x0A00  # Windows 10
    WINVER=0x0A00
    _WIN32_WINDOWS=0x0A00
  )
endif()

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/extra
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  .
)

# -----------------------------------------------------------------------------
# Custom Targets
# -----------------------------------------------------------------------------
add_custom_target(
  AtomCmakeAdditionalFiles
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake
)

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
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    message(STATUS "Using MSVC-specific dependency finding")
    include(cmake/FindDependenciesMSVC.cmake)
else()
    include(cmake/FindDependencies.cmake)
endif()

# SSH support and Python bindings are now handled by FindDependencies.cmake

# -----------------------------------------------------------------------------
# Automatic Dependency Resolution
# -----------------------------------------------------------------------------
if(ATOM_AUTO_RESOLVE_DEPS)
    message(STATUS "Automatic dependency resolution enabled")
    include(cmake/ScanModule.cmake)
    atom_resolve_all_dependencies()
endif()

# Skip platform-specific dependencies for now
# Linux/WSL/Windows platform-specific dependencies
# if(LINUX)
#   find_package(X11 REQUIRED)
#   if(X11_FOUND)
#     include_directories(${X11_INCLUDE_DIR})
#   else()
#     message(FATAL_ERROR "X11 development files not found. Please install libx11-dev or equivalent.")
#   endif()
#   find_package(PkgConfig REQUIRED)
#   pkg_check_modules(UDEV REQUIRED libudev)
#   if(UDEV_FOUND)
#     include_directories(${UDEV_INCLUDE_DIRS})
#     link_directories(${UDEV_LIBRARY_DIRS})
#   else()
#     message(FATAL_ERROR "libudev development files not found. Please install libudev-dev or equivalent.")
#   endif()
# endif()

# Skip Boost for now
# Boost
# if(ATOM_USE_BOOST)
#   set(Boost_USE_STATIC_LIBS ON)
#   set(Boost_USE_MULTITHREADED ON)
#   set(Boost_USE_STATIC_RUNTIME OFF)
#   set(BOOST_COMPONENTS)
#   if(ATOM_USE_BOOST_CONTAINER)
#     list(APPEND BOOST_COMPONENTS container)
#   endif()
#   if(ATOM_USE_BOOST_LOCKFREE)
#     list(APPEND BOOST_COMPONENTS atomic thread)
#   endif()
#   if(ATOM_USE_BOOST_GRAPH)
#     list(APPEND BOOST_COMPONENTS graph)
#   endif()
#   # intrusive is header-only
#   find_package(Boost 1.74 REQUIRED COMPONENTS ${BOOST_COMPONENTS})
#   include_directories(${Boost_INCLUDE_DIRS})
#   message(STATUS "Found Boost: ${Boost_VERSION} (${Boost_INCLUDE_DIRS})")
# endif()

# -----------------------------------------------------------------------------
# Version Info Header
# -----------------------------------------------------------------------------
# Configure version information
configure_atom_version(VERSION_VARIABLE PROJECT_VERSION)

# Also configure the version info header
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
  @ONLY
)

# -----------------------------------------------------------------------------
# Ninja Generator Support
# -----------------------------------------------------------------------------
if(CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_GENERATOR MATCHES "Ninja")
  message(STATUS "Ninja generator detected. Enabling Ninja-specific optimizations.")
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable compile_commands.json for Ninja" FORCE)
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

# Add minimal timer test for debugging
# Temporarily commented out due to missing source files
# add_executable(test_timer_minimal_isolated test_timer_minimal_isolated.cpp)
# target_link_libraries(test_timer_minimal_isolated
#     PRIVATE
#     atom-async
#     atom-utils
#     atom-error
# )
# target_compile_features(test_timer_minimal_isolated PRIVATE cxx_std_20)
# target_include_directories(test_timer_minimal_isolated PRIVATE ${CMAKE_SOURCE_DIR})

# Add header-only test
# Temporarily commented out due to missing source files
# add_executable(test_timer_header_only test_timer_header_only.cpp)
# target_link_libraries(test_timer_header_only
#     PRIVATE
#     atom-async
#     atom-utils
#     atom-error
# )
# target_compile_features(test_timer_header_only PRIVATE cxx_std_20)
# target_include_directories(test_timer_header_only PRIVATE ${CMAKE_SOURCE_DIR})

# Add constructor-only test
# Temporarily commented out due to missing source files
# add_executable(test_timer_constructor_only test_timer_constructor_only.cpp)
# target_link_libraries(test_timer_constructor_only
#     PRIVATE
#     atom-async
#     atom-utils
#     atom-error
# )
# target_compile_features(test_timer_constructor_only PRIVATE cxx_std_20)
# target_include_directories(test_timer_constructor_only PRIVATE ${CMAKE_SOURCE_DIR})

# Basic C++ test removed during cleanup - test_basic_cpp.cpp was a temporary debugging file

# Add minimal timer test (no Atom dependencies)
# Temporarily commented out due to missing source files
# add_executable(test_minimal_timer_isolated test_minimal_timer_isolated.cpp)
# target_compile_features(test_minimal_timer_isolated PRIVATE cxx_std_20)

# Add isolated timer test (no Atom dependencies, more complete implementation)
# Temporarily commented out due to missing source files
# add_executable(test_timer_no_atom_deps test_timer_no_atom_deps.cpp)
# target_compile_features(test_timer_no_atom_deps PRIVATE cxx_std_20)

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
    foreach(MODULE ${ATOM_MODULES})
        string(TOLOWER ${MODULE} MODULE_LOWER)
        atom_register_component(${MODULE_LOWER}
            DESCRIPTION "Atom ${MODULE} module"
            VERSION ${PROJECT_VERSION}
            DEPENDS ${ATOM_MODULE_DEPS_${MODULE}}
        )
    endforeach()

    # Setup modular installation system
    if(COMMAND atom_setup_modular_installation)
        atom_setup_modular_installation()
    endif()
endif()

# -----------------------------------------------------------------------------
# Packaging Configuration
# -----------------------------------------------------------------------------

# Setup CPack for package generation (if function exists)
if(COMMAND atom_setup_cpack)
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
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
  PATTERN "**/internal" EXCLUDE
  PATTERN "**/tests" EXCLUDE
  PATTERN "**/example" EXCLUDE
)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom
)

# -----------------------------------------------------------------------------
# IDE Folders & Final Message
# -----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
message(STATUS "Atom configured successfully")
