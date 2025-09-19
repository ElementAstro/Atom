# CMakeLists.txt for Atom-Error
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Error
# Description: Atom Error Library
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-error VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources
set(SOURCES
    exception.cpp
    stacktrace.cpp
    error_code.cpp
    error_context.cpp
    error_handler.cpp
    error_recovery.cpp
    error_formatter.cpp
)

# Headers
set(HEADERS
    error_code.hpp
    exception.hpp
    stacktrace.hpp
    error_context.hpp
    error_handler.hpp
    error_recovery.hpp
    error_formatter.hpp
)

# Optional stacktrace library support
option(ATOM_USE_CPPTRACE "Enable cpptrace support for enhanced stack traces" OFF)
option(ATOM_USE_BACKWARD_CPP "Enable backward-cpp support for enhanced stack traces" OFF)
option(ATOM_USE_BOOST_STACKTRACE "Enable boost::stacktrace support for enhanced stack traces" OFF)

# Dependencies
set(LIBS
)

# Platform-specific libraries
if (WIN32)
    list(APPEND LIBS dbghelp)
endif()

if (LINUX)
    list(APPEND LIBS dl)
endif()

# Optional external stacktrace libraries
set(STACKTRACE_DEFINITIONS "")

if (ATOM_USE_CPPTRACE)
    find_package(cpptrace QUIET)
    if (cpptrace_FOUND)
        list(APPEND LIBS cpptrace::cpptrace)
        list(APPEND STACKTRACE_DEFINITIONS ATOM_USE_CPPTRACE)
        message(STATUS "cpptrace support enabled")
    else()
        message(WARNING "cpptrace requested but not found")
    endif()
endif()

if (ATOM_USE_BACKWARD_CPP)
    find_package(Backward QUIET)
    if (Backward_FOUND)
        list(APPEND LIBS Backward::Backward)
        list(APPEND STACKTRACE_DEFINITIONS ATOM_USE_BACKWARD_CPP)
        message(STATUS "backward-cpp support enabled")
    else()
        message(WARNING "backward-cpp requested but not found")
    endif()
endif()

if (ATOM_USE_BOOST_STACKTRACE)
    find_package(Boost COMPONENTS stacktrace_basic stacktrace_backtrace stacktrace_addr2line QUIET)
    if (Boost_FOUND)
        list(APPEND LIBS Boost::stacktrace_basic)
        list(APPEND STACKTRACE_DEFINITIONS ATOM_USE_BOOST_STACKTRACE)
        message(STATUS "boost::stacktrace support enabled")
    else()
        message(WARNING "boost::stacktrace requested but not found")
    endif()
endif()

# Include directories for dependencies
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)  # For atom/ includes

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME}_object PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Apply stacktrace library definitions
if (STACKTRACE_DEFINITIONS)
    target_compile_definitions(${PROJECT_NAME}_object PRIVATE ${STACKTRACE_DEFINITIONS})
endif()

# Apply stacktrace library definitions
if (STACKTRACE_DEFINITIONS)
    target_compile_definitions(${PROJECT_NAME}_object PRIVATE ${STACKTRACE_DEFINITIONS})
endif()

# Build Shared Library
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Apply stacktrace library definitions to shared library as well
if (STACKTRACE_DEFINITIONS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ${STACKTRACE_DEFINITIONS})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Register this module to the global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${PROJECT_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
