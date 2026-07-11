# CMakeLists.txt for Type Module Part of the Atom Project Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-type
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Explicit source files (avoid GLOB for better build system reliability)
set(SOURCES rjson.cpp ryaml.cpp)

# Explicit header files
set(HEADERS
    args.hpp
    argsview.hpp
    auto_table.hpp
    compat.hpp
    concurrent_map.hpp
    concurrent_set.hpp
    concurrent_vector.hpp
    cstream.hpp
    deque.hpp
    expected.hpp
    flatmap.hpp
    flatset.hpp
    indestructible.hpp
    iter.hpp
    json-schema.hpp
    json.hpp
    json_fwd.hpp
    no_offset_ptr.hpp
    noncopyable.hpp
    optional.hpp
    pod_vector.hpp
    pointer.hpp
    qvariant.hpp
    rjson.hpp
    robin_hood.hpp
    rtype.hpp
    ryaml.hpp
    small_list.hpp
    small_vector.hpp
    static_string.hpp
    static_vector.hpp
    string.hpp
    trackable.hpp
    uint.hpp
    weak_ptr.hpp)

# Determine if this is header-only
if(NOT SOURCES)
  set(IS_HEADER_ONLY TRUE)
else()
  set(IS_HEADER_ONLY FALSE)
endif()

# Create library target
if(IS_HEADER_ONLY)
  add_library(atom-type INTERFACE)
  add_library(atom::type ALIAS atom-type)
  atom_configure_module(atom-type HEADER_ONLY)
else()
  add_library(atom-type STATIC ${SOURCES} ${HEADERS})
  add_library(atom::type ALIAS atom-type)
  atom_configure_module(atom-type)
endif()

# Find optional dependencies for JSON and YAML support
find_package(nlohmann_json QUIET)
find_package(yaml-cpp QUIET)

# Link optional dependencies
if(IS_HEADER_ONLY)
  if(nlohmann_json_FOUND)
    target_link_libraries(atom-type INTERFACE nlohmann_json::nlohmann_json)
  endif()
  if(yaml-cpp_FOUND)
    target_link_libraries(atom-type INTERFACE yaml-cpp::yaml-cpp)
  endif()
else()
  if(nlohmann_json_FOUND)
    target_link_libraries(atom-type PUBLIC nlohmann_json::nlohmann_json)
  endif()
  if(yaml-cpp_FOUND)
    target_link_libraries(atom-type PUBLIC yaml-cpp::yaml-cpp)
  endif()
endif()

# Install library target
install(
  TARGETS atom-type
  EXPORT atom-type-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-type-targets
  FILE atom-typeTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/type)
