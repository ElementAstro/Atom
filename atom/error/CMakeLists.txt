# CMakeLists.txt for atom-error module
#
# Copyright (C) 2023-2024 Max Qian <lightapt.com>
#
# Refactored modular structure with subdirectories

cmake_minimum_required(VERSION 3.21)
project(atom-error LANGUAGES CXX)

# ============================================================================
# Source Files - Organized by subdirectory
# ============================================================================

# Core module
set(CORE_SOURCES core/error_metadata.cpp)

set(CORE_HEADERS core/error_types.hpp core/error_codes.hpp
                 core/error_metadata.hpp)

# Stacktrace module
set(STACKTRACE_SOURCES
    stacktrace/stack_frame.cpp stacktrace/stacktrace_utils.cpp
    stacktrace/builtin_backend.cpp stacktrace/external_backends.cpp
    stacktrace/backend_factory.cpp stacktrace/stacktrace.cpp)

set(STACKTRACE_HEADERS
    stacktrace/stack_frame.hpp stacktrace/stacktrace_utils.hpp
    stacktrace/backend_interface.hpp stacktrace/builtin_backend.hpp
    stacktrace/external_backends.hpp stacktrace/stacktrace.hpp)

# Exception module
set(EXCEPTION_SOURCES exception/exception_base.cpp)

set(EXCEPTION_HEADERS
    exception/exception_base.hpp exception/common_exceptions.hpp
    exception/object_exceptions.hpp exception/argument_exceptions.hpp
    exception/file_exceptions.hpp exception/system_exceptions.hpp)

# Context module
set(CONTEXT_SOURCES context/error_context.cpp context/context_manager.cpp
                    context/scoped_context.cpp)

set(CONTEXT_HEADERS context/error_context.hpp context/context_manager.hpp
                    context/scoped_context.hpp)

# Handler module
set(HANDLER_SOURCES handler/error_reporter.cpp handler/error_aggregator.cpp
                    handler/global_handler.cpp)

set(HANDLER_HEADERS handler/error_reporter.hpp handler/error_aggregator.hpp
                    handler/global_handler.hpp)

# Aggregate all sources and headers
set(SOURCES ${CORE_SOURCES} ${STACKTRACE_SOURCES} ${EXCEPTION_SOURCES}
            ${CONTEXT_SOURCES} ${HANDLER_SOURCES})

set(HEADERS error.hpp ${CORE_HEADERS} ${STACKTRACE_HEADERS}
            ${EXCEPTION_HEADERS} ${CONTEXT_HEADERS} ${HANDLER_HEADERS})

# ============================================================================
# Library Target
# ============================================================================

# Use SHARED library to avoid multiple definition issues when linked by multiple
# modules
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
add_library(atom::error ALIAS ${PROJECT_NAME})

# Windows DLL settings - export all symbols by default for MinGW compatibility
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS
                                                   ON)
endif()

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Configure module using standardized function (as base module, no dependencies)
atom_configure_module(${PROJECT_NAME})

# Platform-specific settings
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp psapi)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN
                                                     NOMINMAX)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

# ============================================================================
# Optional External Stacktrace Libraries
# ============================================================================

# High-quality external backends
option(ATOM_USE_CPPTRACE "Use cpptrace for stack traces (recommended)" OFF)
option(ATOM_USE_BACKWARD_CPP "Use backward-cpp for stack traces" OFF)
option(ATOM_USE_BOOST_STACKTRACE "Use Boost.Stacktrace for stack traces" OFF)

# System-level backends
option(ATOM_USE_LIBUNWIND "Use libunwind for stack traces" OFF)
option(ATOM_USE_EXECINFO "Use execinfo/backtrace for stack traces (POSIX)" OFF)
option(ATOM_USE_LIBBACKTRACE "Use libbacktrace for stack traces (GCC)" OFF)
option(ATOM_USE_STD_STACKTRACE "Use std::stacktrace (C++23)" OFF)
option(ATOM_USE_ABSEIL_STACKTRACE "Use Abseil for stack traces" OFF)

# ----------------------------------------------------------------------------
# cpptrace - Modern C++ stack trace library (recommended)
# ----------------------------------------------------------------------------
if(ATOM_USE_CPPTRACE)
  find_package(cpptrace QUIET)
  if(cpptrace_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_CPPTRACE)
    message(STATUS "atom-error: Using cpptrace for stack traces")
  else()
    message(WARNING "cpptrace not found, falling back to builtin")
  endif()
endif()

# ----------------------------------------------------------------------------
# backward-cpp - Stack trace with source snippets
# ----------------------------------------------------------------------------
if(ATOM_USE_BACKWARD_CPP)
  find_package(Backward QUIET)
  if(Backward_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Backward::Backward)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_BACKWARD_CPP)
    message(STATUS "atom-error: Using backward-cpp for stack traces")
  else()
    message(WARNING "backward-cpp not found, falling back to builtin")
  endif()
endif()

# ----------------------------------------------------------------------------
# Boost.Stacktrace
# ----------------------------------------------------------------------------
if(ATOM_USE_BOOST_STACKTRACE)
  find_package(
    Boost
    COMPONENTS stacktrace_basic
    QUIET)
  if(Boost_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::stacktrace_basic)
    target_compile_definitions(${PROJECT_NAME}
                               PRIVATE ATOM_USE_BOOST_STACKTRACE)
    message(STATUS "atom-error: Using Boost.Stacktrace for stack traces")
  else()
    message(WARNING "Boost.Stacktrace not found, falling back to builtin")
  endif()
endif()

# ----------------------------------------------------------------------------
# libunwind - Portable unwinding library
# ----------------------------------------------------------------------------
if(ATOM_USE_LIBUNWIND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBUNWIND QUIET libunwind)
    if(LIBUNWIND_FOUND)
      target_include_directories(${PROJECT_NAME}
                                 PRIVATE ${LIBUNWIND_INCLUDE_DIRS})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUNWIND_LIBRARIES})
      target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_LIBUNWIND)
      message(STATUS "atom-error: Using libunwind for stack traces")
    else()
      message(WARNING "libunwind not found, falling back to builtin")
    endif()
  else()
    # Try to find libunwind directly
    find_library(LIBUNWIND_LIB unwind)
    find_path(LIBUNWIND_INCLUDE libunwind.h)
    if(LIBUNWIND_LIB AND LIBUNWIND_INCLUDE)
      target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUNWIND_INCLUDE})
      target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUNWIND_LIB})
      target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_LIBUNWIND)
      message(STATUS "atom-error: Using libunwind for stack traces")
    else()
      message(WARNING "libunwind not found, falling back to builtin")
    endif()
  endif()
endif()

# ----------------------------------------------------------------------------
# execinfo - POSIX backtrace (glibc, macOS, BSD)
# ----------------------------------------------------------------------------
if(ATOM_USE_EXECINFO)
  include(CheckIncludeFile)
  check_include_file(execinfo.h HAVE_EXECINFO_H)
  if(HAVE_EXECINFO_H)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_EXECINFO)
    # Some systems need -lexecinfo
    find_library(EXECINFO_LIB execinfo)
    if(EXECINFO_LIB)
      target_link_libraries(${PROJECT_NAME} PRIVATE ${EXECINFO_LIB})
    endif()
    message(STATUS "atom-error: Using execinfo for stack traces")
  else()
    message(WARNING "execinfo.h not found, falling back to builtin")
  endif()
endif()

# ----------------------------------------------------------------------------
# libbacktrace - GCC's backtrace library
# ----------------------------------------------------------------------------
if(ATOM_USE_LIBBACKTRACE)
  find_library(LIBBACKTRACE_LIB backtrace)
  find_path(LIBBACKTRACE_INCLUDE backtrace.h)
  if(LIBBACKTRACE_LIB AND LIBBACKTRACE_INCLUDE)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBBACKTRACE_INCLUDE})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBBACKTRACE_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_LIBBACKTRACE)
    message(STATUS "atom-error: Using libbacktrace for stack traces")
  else()
    message(WARNING "libbacktrace not found, falling back to builtin")
  endif()
endif()

# ----------------------------------------------------------------------------
# std::stacktrace - C++23 standard library
# ----------------------------------------------------------------------------
if(ATOM_USE_STD_STACKTRACE)
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS "-std=c++23")
  check_cxx_source_compiles(
    "
        #include <stacktrace>
        int main() {
            auto st = std::stacktrace::current();
            return st.size();
        }
    "
    HAVE_STD_STACKTRACE)
  if(HAVE_STD_STACKTRACE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_USE_STD_STACKTRACE)
    # Some compilers need -lstdc++_libbacktrace for std::stacktrace
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
    endif()
    message(STATUS "atom-error: Using std::stacktrace (C++23) for stack traces")
  else()
    message(WARNING "std::stacktrace not available, falling back to builtin")
  endif()
endif()

# ----------------------------------------------------------------------------
# Abseil - Google's C++ library
# ----------------------------------------------------------------------------
if(ATOM_USE_ABSEIL_STACKTRACE)
  find_package(absl QUIET)
  if(absl_FOUND)
    target_link_libraries(
      ${PROJECT_NAME} PRIVATE absl::debugging absl::stacktrace absl::symbolize)
    target_compile_definitions(${PROJECT_NAME}
                               PRIVATE ATOM_USE_ABSEIL_STACKTRACE)
    message(STATUS "atom-error: Using Abseil for stack traces")
  else()
    message(WARNING "Abseil not found, falling back to builtin")
  endif()
endif()

# ============================================================================
# Installation
# ============================================================================

# Install library target with export
install(
  TARGETS ${PROJECT_NAME}
  EXPORT atom-error-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install export targets
install(
  EXPORT atom-error-targets
  FILE atom-errorTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom)

# Install headers preserving directory structure
install(FILES error.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error)

install(FILES ${CORE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error/core)

install(FILES ${STACKTRACE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error/stacktrace)

install(FILES ${EXCEPTION_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error/exception)

install(FILES ${CONTEXT_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error/context)

install(FILES ${HANDLER_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/error/handler)

# ============================================================================
# Status Message
# ============================================================================

message(STATUS "atom-error module configured with:")
message(STATUS "  - Core: ${CORE_SOURCES}")
message(STATUS "  - Stacktrace: ${STACKTRACE_SOURCES}")
message(STATUS "  - Exception: ${EXCEPTION_SOURCES}")
message(STATUS "  - Context: ${CONTEXT_SOURCES}")
message(STATUS "  - Handler: ${HANDLER_SOURCES}")
