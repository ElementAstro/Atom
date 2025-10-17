# CMakeLists.txt for Atom-Error This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Error Description: Atom Error Library Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-error
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES
    exception.cpp
    stacktrace.cpp
    error_code.cpp
    error_context.cpp
    error_handler.cpp
    error_recovery.cpp
    error_formatter.cpp)

# Headers
set(HEADERS
    error_code.hpp
    exception.hpp
    stacktrace.hpp
    error_context.hpp
    error_handler.hpp
    error_recovery.hpp
    error_formatter.hpp)

# Optional stacktrace library support
option(ATOM_USE_CPPTRACE "Enable cpptrace support for enhanced stack traces"
       OFF)
option(ATOM_USE_BACKWARD_CPP
       "Enable backward-cpp support for enhanced stack traces" OFF)
option(ATOM_USE_BOOST_STACKTRACE
       "Enable boost::stacktrace support for enhanced stack traces" OFF)

# Dependencies
set(LIBS)

# Platform-specific libraries
if(WIN32)
  list(APPEND LIBS dbghelp)
endif()

if(LINUX)
  list(APPEND LIBS dl)
endif()

# Optional external stacktrace libraries
set(STACKTRACE_DEFINITIONS "")

if(ATOM_USE_CPPTRACE)
  find_package(cpptrace QUIET)
  if(cpptrace_FOUND)
    list(APPEND LIBS cpptrace::cpptrace)
    list(APPEND STACKTRACE_DEFINITIONS ATOM_USE_CPPTRACE)
    message(STATUS "cpptrace support enabled")
  else()
    message(WARNING "cpptrace requested but not found")
  endif()
endif()

if(ATOM_USE_BACKWARD_CPP)
  find_package(Backward QUIET)
  if(Backward_FOUND)
    list(APPEND LIBS Backward::Backward)
    list(APPEND STACKTRACE_DEFINITIONS ATOM_USE_BACKWARD_CPP)
    message(STATUS "backward-cpp support enabled")
  else()
    message(WARNING "backward-cpp requested but not found")
  endif()
endif()

if(ATOM_USE_BOOST_STACKTRACE)
  find_package(
    Boost
    COMPONENTS stacktrace_basic stacktrace_backtrace stacktrace_addr2line
    QUIET)
  if(Boost_FOUND)
    list(APPEND LIBS Boost::stacktrace_basic)
    list(APPEND STACKTRACE_DEFINITIONS ATOM_USE_BOOST_STACKTRACE)
    message(STATUS "boost::stacktrace support enabled")
  else()
    message(WARNING "boost::stacktrace requested but not found")
  endif()
endif()

# Create library target (shared library for error module)
add_library(atom-error SHARED ${SOURCES} ${HEADERS})

# Apply stacktrace library definitions
if(STACKTRACE_DEFINITIONS)
  target_compile_definitions(atom-error PRIVATE ${STACKTRACE_DEFINITIONS})
endif()

# Configure module using standardized function
atom_configure_module(atom-error)

# Link module-specific dependencies
target_link_libraries(atom-error PRIVATE ${LIBS})

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error)
