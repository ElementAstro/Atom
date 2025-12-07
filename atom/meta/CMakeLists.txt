# CMakeLists.txt for atom-meta This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: atom-meta Description: a library for meta programming in C++
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-meta
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES global_ptr.cpp)

# Headers
set(HEADERS
    abi.hpp
    any.hpp
    anymeta.hpp
    awaitable.hpp
    bind_first.hpp
    concept.hpp
    constructor.hpp
    container_traits.hpp
    conversion.hpp
    decorate.hpp
    enum.hpp
    facade.hpp
    facade_any.hpp
    facade_proxy.hpp
    ffi.hpp
    field_count.hpp
    func_traits.hpp
    global_ptr.hpp
    god.hpp
    invoke.hpp
    member.hpp
    overload.hpp
    property.hpp
    proxy.hpp
    proxy_params.hpp
    raw_name.hpp
    refl.hpp
    refl_json.hpp
    refl_yaml.hpp
    signature.hpp
    stepper.hpp
    template_traits.hpp
    time.hpp
    type_caster.hpp
    type_info.hpp
    vany.hpp)

# Logging dependency (spdlog preferred) - must be found before creating targets
find_package(spdlog CONFIG REQUIRED)

# Create library using object library pattern for better build performance
add_library(atom-meta_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET atom-meta_object PROPERTY POSITION_INDEPENDENT_CODE 1)
# Use header-only spdlog to avoid linking conflicts with MinGW
if(TARGET spdlog::spdlog)
  target_link_libraries(atom-meta_object PUBLIC spdlog::spdlog)
else()
  target_link_libraries(atom-meta_object PUBLIC spdlog::spdlog)
  target_compile_definitions(atom-meta_object PUBLIC SPDLOG_HEADER_ONLY)
endif()

# Create static library from object library
add_library(atom-meta STATIC $<TARGET_OBJECTS:atom-meta_object>)

# Configure module with standardized settings
atom_configure_module(atom-meta)

# Use header-only spdlog to avoid linking conflicts with MinGW
if(TARGET spdlog::spdlog)
  target_link_libraries(atom-meta PUBLIC spdlog::spdlog)
else()
  target_link_libraries(atom-meta PUBLIC spdlog::spdlog)
  target_compile_definitions(atom-meta PUBLIC SPDLOG_HEADER_ONLY)
endif()

# =============================================================================
# Installation
# =============================================================================

# Install library target
install(
  TARGETS atom-meta
  EXPORT atom-meta-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-meta-targets
  FILE atom-metaTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/meta)
