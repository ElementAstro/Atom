# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom
# Description: Atom Library for all of the Element Astro Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom VERSION 1.0.0 LANGUAGES C CXX)

# =============================================================================
# Python Support Configuration
# =============================================================================

option(ATOM_BUILD_PYTHON "Build Atom with Python support" OFF)
if(ATOM_BUILD_PYTHON)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)
    if(PYTHON_FOUND)
        message(STATUS "Found Python ${PYTHON_VERSION_STRING}: ${PYTHON_EXECUTABLE}")
        find_package(pybind11 QUIET)
        if(pybind11_FOUND)
            message(STATUS "Found pybind11: ${pybind11_INCLUDE_DIRS}")
        else()
            message(FATAL_ERROR "pybind11 not found")
        endif()
    else()
        message(FATAL_ERROR "Python not found")
    endif()
endif()

# =============================================================================
# Platform-Specific Dependencies
# =============================================================================

if(UNIX AND NOT APPLE)
    # Linux-specific dependencies
    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    if(SYSTEMD_FOUND)
        message(STATUS "Found libsystemd: ${SYSTEMD_VERSION}")
    endif()
endif()

# =============================================================================
# Subdirectory Management
# =============================================================================

# Function to check if a module directory is valid
function(check_module_directory module_name dir_name result_var)
    set(module_path "${CMAKE_CURRENT_SOURCE_DIR}/${dir_name}")
    if(EXISTS "${module_path}" AND EXISTS "${module_path}/CMakeLists.txt")
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
        if(NOT EXISTS "${module_path}")
            message(STATUS "Module directory for '${module_name}' does not exist: ${module_path}")
        elseif(NOT EXISTS "${module_path}/CMakeLists.txt")
            message(STATUS "Module directory '${module_path}' exists but lacks CMakeLists.txt")
        endif()
    endif()
endfunction()

# List of subdirectories to build
set(SUBDIRECTORIES)

# Check if each module needs to be built and add to the list
if(ATOM_BUILD_ALGORITHM)
    check_module_directory("algorithm" "algorithm" ALGORITHM_VALID)
    if(ALGORITHM_VALID)
        list(APPEND SUBDIRECTORIES algorithm)
        message(STATUS "Building algorithm module")
    else()
        message(STATUS "Skipping algorithm module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_ASYNC)
    check_module_directory("async" "async" ASYNC_VALID)
    if(ASYNC_VALID)
        list(APPEND SUBDIRECTORIES async)
        message(STATUS "Building async module")
    else()
        message(STATUS "Skipping async module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_COMPONENTS)
    check_module_directory("components" "components" COMPONENTS_VALID)
    if(COMPONENTS_VALID)
        list(APPEND SUBDIRECTORIES components)
        message(STATUS "Building components module")
    else()
        message(STATUS "Skipping components module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_CONNECTION)
    check_module_directory("connection" "connection" CONNECTION_VALID)
    if(CONNECTION_VALID)
        list(APPEND SUBDIRECTORIES connection)
        message(STATUS "Building connection module")
    else()
        message(STATUS "Skipping connection module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_CONTAINERS)
    check_module_directory("containers" "containers" CONTAINERS_VALID)
    if(CONTAINERS_VALID)
        list(APPEND SUBDIRECTORIES containers)
        message(STATUS "Building containers module")
    else()
        message(STATUS "Skipping containers module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_ERROR)
    check_module_directory("error" "error" ERROR_VALID)
    if(ERROR_VALID)
        list(APPEND SUBDIRECTORIES error)
        message(STATUS "Building error module")
    else()
        message(STATUS "Skipping error module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_IO)
    check_module_directory("io" "io" IO_VALID)
    if(IO_VALID)
        list(APPEND SUBDIRECTORIES io)
        message(STATUS "Building io module")
    else()
        message(STATUS "Skipping io module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_LOG)
    check_module_directory("log" "log" LOG_VALID)
    if(LOG_VALID)
        list(APPEND SUBDIRECTORIES log)
        message(STATUS "Building log module")
    else()
        message(STATUS "Skipping log module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_MEMORY)
    check_module_directory("memory" "memory" MEMORY_VALID)
    if(MEMORY_VALID)
        list(APPEND SUBDIRECTORIES memory)
        message(STATUS "Building memory module")
    else()
        message(STATUS "Skipping memory module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_META)
    check_module_directory("meta" "meta" META_VALID)
    if(META_VALID)
        list(APPEND SUBDIRECTORIES meta)
        message(STATUS "Building meta module")
    else()
        message(STATUS "Skipping meta module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_SEARCH)
    check_module_directory("search" "search" SEARCH_VALID)
    if(SEARCH_VALID)
        list(APPEND SUBDIRECTORIES search)
        message(STATUS "Building search module")
    else()
        message(STATUS "Skipping search module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_SECRET)
    check_module_directory("secret" "secret" SECRET_VALID)
    if(SECRET_VALID)
        list(APPEND SUBDIRECTORIES secret)
        message(STATUS "Building secret module")
    else()
        message(STATUS "Skipping secret module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_SERIAL)
    check_module_directory("serial" "serial" SERIAL_VALID)
    if(SERIAL_VALID)
        list(APPEND SUBDIRECTORIES serial)
        message(STATUS "Building serial module")
    else()
        message(STATUS "Skipping serial module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_SYSINFO)
    check_module_directory("sysinfo" "sysinfo" SYSINFO_VALID)
    if(SYSINFO_VALID)
        list(APPEND SUBDIRECTORIES sysinfo)
        message(STATUS "Building sysinfo module")
    else()
        message(STATUS "Skipping sysinfo module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_SYSTEM)
    check_module_directory("system" "system" SYSTEM_VALID)
    if(SYSTEM_VALID)
        list(APPEND SUBDIRECTORIES system)
        message(STATUS "Building system module")
    else()
        message(STATUS "Skipping system module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_TYPE)
    check_module_directory("type" "type" TYPE_VALID)
    if(TYPE_VALID)
        list(APPEND SUBDIRECTORIES type)
        message(STATUS "Building type module")
    else()
        message(STATUS "Skipping type module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_UTILS)
    check_module_directory("utils" "utils" UTILS_VALID)
    if(UTILS_VALID)
        list(APPEND SUBDIRECTORIES utils)
        message(STATUS "Building utils module")
    else()
        message(STATUS "Skipping utils module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_WEB)
    check_module_directory("web" "web" WEB_VALID)
    if(WEB_VALID)
        list(APPEND SUBDIRECTORIES web)
        message(STATUS "Building web module")
    else()
        message(STATUS "Skipping web module due to missing or invalid directory")
    endif()
endif()

if(ATOM_BUILD_TESTS)
    list(APPEND SUBDIRECTORIES tests)
    message(STATUS "Building tests")
endif()

# =============================================================================
# Dependency Resolution
# =============================================================================

# Process module dependencies
scan_module_dependencies()
process_module_dependencies()

# =============================================================================
# Add Subdirectories
# =============================================================================

# Add all modules to build
foreach(dir ${SUBDIRECTORIES})
    set(subdir_path "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
    if(EXISTS "${subdir_path}" AND EXISTS "${subdir_path}/CMakeLists.txt")
        add_subdirectory(${dir})
    else()
        message(STATUS "Skipping directory '${dir}' as it does not exist or does not contain CMakeLists.txt")
    endif()
endforeach()

# =============================================================================
# Create Combined Library
# =============================================================================

# Option to create a unified Atom library
option(ATOM_BUILD_UNIFIED_LIBRARY "Build a unified Atom library containing all modules" ON)

if(ATOM_BUILD_UNIFIED_LIBRARY)
    # Get all targets that are atom modules
    get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
    
    if(ATOM_MODULE_TARGETS)
        message(STATUS "Creating unified Atom library with modules: ${ATOM_MODULE_TARGETS}")
        
        # Create unified target
        add_library(atom-unified INTERFACE)
        
        # Link all module targets
        target_link_libraries(atom-unified INTERFACE ${ATOM_MODULE_TARGETS})
        
        # Install unified target
        install(TARGETS atom-unified
                EXPORT atom-unified-targets
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
endif()

message(STATUS "Atom modules configuration completed successfully")