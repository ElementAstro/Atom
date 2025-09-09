# FindDependencies.cmake
# Standardized dependency finding for the Atom project
# This module provides consistent dependency finding across all modules

include(FindPackageHandleStandardArgs)

# Set policy for consistent behavior
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# =============================================================================
# Utility Functions
# =============================================================================

# Function to find a dependency with multiple fallback methods
function(atom_find_dependency dep_name)
    set(options REQUIRED QUIET)
    set(oneValueArgs VERSION COMPONENT)
    set(multiValueArgs COMPONENTS PATHS PKG_CONFIG_NAME HINTS)
    cmake_parse_arguments(AFD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    string(TOUPPER ${dep_name} DEP_UPPER)
    set(found_var "${DEP_UPPER}_FOUND")

    # Skip if already found
    if(${found_var})
        return()
    endif()

    # Method 1: Try find_package first
    if(AFD_COMPONENTS)
        if(AFD_QUIET)
            find_package(${dep_name} ${AFD_VERSION} QUIET COMPONENTS ${AFD_COMPONENTS})
        else()
            find_package(${dep_name} ${AFD_VERSION} COMPONENTS ${AFD_COMPONENTS})
        endif()
    else()
        if(AFD_QUIET)
            find_package(${dep_name} ${AFD_VERSION} QUIET)
        else()
            find_package(${dep_name} ${AFD_VERSION})
        endif()
    endif()

    # Method 2: Try pkg-config if find_package failed
    if(NOT ${found_var} AND AFD_PKG_CONFIG_NAME)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            if(AFD_VERSION)
                pkg_check_modules(${DEP_UPPER} QUIET ${AFD_PKG_CONFIG_NAME}>=${AFD_VERSION})
            else()
                pkg_check_modules(${DEP_UPPER} QUIET ${AFD_PKG_CONFIG_NAME})
            endif()
        endif()
    endif()

    # Method 3: Manual search for header-only libraries
    if(NOT ${found_var} AND AFD_PATHS)
        find_path(${DEP_UPPER}_INCLUDE_DIR
            NAMES ${AFD_PATHS}
            PATHS
                /usr/include
                /usr/local/include
                /mingw64/include
                ${CMAKE_PREFIX_PATH}/include
                ${AFD_HINTS}
        )
        if(${DEP_UPPER}_INCLUDE_DIR)
            set(${found_var} TRUE PARENT_SCOPE)
            message(STATUS "Found ${dep_name} headers at: ${${DEP_UPPER}_INCLUDE_DIR}")
        endif()
    endif()

    # Handle results
    if(${found_var})
        if(NOT AFD_QUIET)
            if(${dep_name}_VERSION)
                message(STATUS "Found ${dep_name}: ${${dep_name}_VERSION}")
            elseif(${DEP_UPPER}_VERSION)
                message(STATUS "Found ${dep_name}: ${${DEP_UPPER}_VERSION}")
            else()
                message(STATUS "Found ${dep_name}")
            endif()
        endif()
    else()
        if(AFD_REQUIRED)
            message(FATAL_ERROR "${dep_name} is required but was not found")
        else()
            message(WARNING "${dep_name} not found - related features may be limited")
        endif()
    endif()
endfunction()

# Function to setup a dependency target
function(atom_setup_dependency_target dep_name target_name)
    string(TOUPPER ${dep_name} DEP_UPPER)

    if(NOT TARGET ${target_name})
        # Create imported target if it doesn't exist
        if(${DEP_UPPER}_FOUND)
            add_library(${target_name} INTERFACE IMPORTED)

            # Set include directories
            if(${DEP_UPPER}_INCLUDE_DIRS)
                target_include_directories(${target_name} INTERFACE ${${DEP_UPPER}_INCLUDE_DIRS})
            elseif(${DEP_UPPER}_INCLUDE_DIR)
                target_include_directories(${target_name} INTERFACE ${${DEP_UPPER}_INCLUDE_DIR})
            endif()

            # Set libraries
            if(${DEP_UPPER}_LIBRARIES)
                target_link_libraries(${target_name} INTERFACE ${${DEP_UPPER}_LIBRARIES})
            elseif(${dep_name}_LIBRARIES)
                target_link_libraries(${target_name} INTERFACE ${${dep_name}_LIBRARIES})
            endif()

            # Set compile flags
            if(${DEP_UPPER}_CFLAGS_OTHER)
                target_compile_options(${target_name} INTERFACE ${${DEP_UPPER}_CFLAGS_OTHER})
            endif()
        endif()
    endif()
endfunction()

# =============================================================================
# Core Dependencies
# =============================================================================

# OpenSSL - Always required
atom_find_dependency(OpenSSL REQUIRED)

# ZLIB - Always required
atom_find_dependency(ZLIB REQUIRED)

# SQLite3 - Core database functionality
atom_find_dependency(SQLite3 QUIET)

# fmt - String formatting
atom_find_dependency(fmt QUIET PKG_CONFIG_NAME fmt)

# =============================================================================
# Header-only Dependencies
# =============================================================================

# Asio - Networking (header-only, standalone)
atom_find_dependency(asio QUIET PATHS asio.hpp HINTS /mingw64/include /usr/include /usr/local/include)
if(ASIO_FOUND)
    add_definitions(-DASIO_STANDALONE)
    atom_setup_dependency_target(asio asio::asio)
endif()

# =============================================================================
# Optional Dependencies
# =============================================================================

# SSH support
if(ATOM_USE_SSH)
    atom_find_dependency(libssh REQUIRED PKG_CONFIG_NAME libssh)
    if(LIBSSH_FOUND)
        add_definitions(-DENABLE_SSH -DENABLE_LIBSSH)
        atom_setup_dependency_target(libssh libssh::libssh)
    endif()
endif()

# Python bindings
if(ATOM_BUILD_PYTHON_BINDINGS)
    atom_find_dependency(Python REQUIRED COMPONENTS Interpreter Development)
    atom_find_dependency(pybind11 REQUIRED)
endif()

# Testing framework
if(ATOM_BUILD_TESTS)
    atom_find_dependency(GTest QUIET PKG_CONFIG_NAME gtest)
    if(NOT GTEST_FOUND)
        # Fallback to manual GTest setup if needed
        message(STATUS "GTest not found via standard methods, tests may not build")
    endif()
endif()

# =============================================================================
# Boost Dependencies (Optional)
# =============================================================================

if(ATOM_USE_BOOST)
    set(BOOST_COMPONENTS)

    if(ATOM_USE_BOOST_LOCKFREE)
        list(APPEND BOOST_COMPONENTS atomic thread)
    endif()

    if(ATOM_USE_BOOST_GRAPH)
        list(APPEND BOOST_COMPONENTS graph)
    endif()

    if(ATOM_USE_BOOST_CONTAINER)
        list(APPEND BOOST_COMPONENTS container)
    endif()

    if(BOOST_COMPONENTS)
        atom_find_dependency(Boost QUIET VERSION 1.74 COMPONENTS ${BOOST_COMPONENTS})
    else()
        atom_find_dependency(Boost QUIET VERSION 1.74)
    endif()
endif()

# =============================================================================
# Platform-specific Dependencies
# =============================================================================

if(WIN32)
    # Windows-specific libraries are handled by target_link_libraries in individual modules
    message(STATUS "Windows platform detected - platform-specific dependencies will be handled per module")
endif()

if(UNIX AND NOT APPLE)
    # Linux-specific dependencies (commented out for now as per original CMakeLists.txt)
    # atom_find_dependency(X11 QUIET)
    # atom_find_dependency(PkgConfig REQUIRED)
    # if(PkgConfig_FOUND)
    #     pkg_check_modules(UDEV QUIET libudev)
    # endif()
endif()

# =============================================================================
# Summary
# =============================================================================

message(STATUS "=== Dependency Summary ===")
message(STATUS "OpenSSL: ${OpenSSL_FOUND}")
message(STATUS "ZLIB: ${ZLIB_FOUND}")
message(STATUS "SQLite3: ${SQLite3_FOUND}")
message(STATUS "fmt: ${fmt_FOUND}")
message(STATUS "Asio: ${ASIO_FOUND}")

if(ATOM_USE_SSH)
    message(STATUS "LibSSH: ${LIBSSH_FOUND}")
endif()

if(ATOM_BUILD_PYTHON_BINDINGS)
    message(STATUS "Python: ${Python_FOUND}")
    message(STATUS "pybind11: ${pybind11_FOUND}")
endif()

if(ATOM_BUILD_TESTS)
    message(STATUS "GTest: ${GTEST_FOUND}")
endif()

if(ATOM_USE_BOOST)
    message(STATUS "Boost: ${Boost_FOUND}")
endif()

message(STATUS "==========================")
