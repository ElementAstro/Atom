# CMakeLists.txt for Atom-Containers
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Containers
# Description: High-performance container library for Atom
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-containers
  VERSION 1.0.0
  LANGUAGES CXX)

# Headers
set(HEADERS
    boost_containers.hpp
    graph.hpp
    high_performance.hpp
    intrusive.hpp
    lockfree.hpp)

# Build Interface Library (header-only)
add_library(${PROJECT_NAME} INTERFACE)

# Include directories
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Set C++ standard
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

# Installation
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/containers
)

# Export targets
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE atom::containers::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Register this module as an Atom module
set_property(GLOBAL APPEND PROPERTY ATOM_MODULE_TARGETS ${PROJECT_NAME})
