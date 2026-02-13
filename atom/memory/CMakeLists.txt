# CMakeLists.txt for Memory Module Part of the Atom Project Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-memory
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Explicit source files (avoid GLOB for better build system reliability)
set(SOURCES instantiations.cpp tracker.cpp)

# Explicit header files
set(HEADERS
    memory.hpp
    memory_pool.hpp
    object.hpp
    ring.hpp
    shared.hpp
    short_alloc.hpp
    tracker.hpp
    utils.hpp)

# Determine if this is header-only
if(NOT SOURCES)
  set(IS_HEADER_ONLY TRUE)
else()
  set(IS_HEADER_ONLY FALSE)
endif()

# Create library target
if(IS_HEADER_ONLY)
  add_library(atom-memory INTERFACE)
  add_library(atom::memory ALIAS atom-memory)
  atom_configure_module(atom-memory HEADER_ONLY)
else()
  add_library(atom-memory STATIC ${SOURCES} ${HEADERS})
  add_library(atom::memory ALIAS atom-memory)
  atom_configure_module(atom-memory)
endif()

# Find spdlog for shared.hpp
find_package(spdlog QUIET)

# Link module-specific dependencies
if(IS_HEADER_ONLY)
  target_link_libraries(
    atom-memory INTERFACE atom-meta # For concept.hpp
                          atom-type # For noncopyable.hpp
  )
  # Add spdlog compile definitions for header-only mode
  target_compile_definitions(atom-memory INTERFACE SPDLOG_COMPILED_LIB
                                                   SPDLOG_FMT_EXTERNAL)
  if(spdlog_FOUND)
    target_link_libraries(atom-memory INTERFACE spdlog::spdlog)
  endif()
else()
  target_link_libraries(
    atom-memory PUBLIC atom-meta # For concept.hpp
                       atom-type # For noncopyable.hpp
  )
  # Add spdlog compile definitions
  target_compile_definitions(atom-memory PUBLIC SPDLOG_COMPILED_LIB
                                                SPDLOG_FMT_EXTERNAL)
  if(spdlog_FOUND)
    target_link_libraries(atom-memory PUBLIC spdlog::spdlog)
  endif()
endif()

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/memory)
