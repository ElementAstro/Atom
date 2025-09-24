# CMakeLists.txt for Memory Module
# Part of the Atom Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(atom-memory VERSION 1.0.0 LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Find all source files
file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

# Determine if this is header-only
if(NOT SOURCES)
    set(IS_HEADER_ONLY TRUE)
else()
    set(IS_HEADER_ONLY FALSE)
endif()

# Create library target
if(IS_HEADER_ONLY)
    add_library(atom-memory INTERFACE)
    atom_configure_module(atom-memory HEADER_ONLY)
else()
    add_library(atom-memory STATIC ${SOURCES} ${HEADERS})
    atom_configure_module(atom-memory)
endif()

# Link module-specific dependencies
if(IS_HEADER_ONLY)
    target_link_libraries(atom-memory INTERFACE
        atom-meta  # For concept.hpp
        atom-type  # For noncopyable.hpp
    )
else()
    target_link_libraries(atom-memory PUBLIC
        atom-meta  # For concept.hpp
        atom-type  # For noncopyable.hpp
    )
endif()

# Install headers
install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/memory
)
