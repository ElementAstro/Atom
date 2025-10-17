# CMakeLists.txt for Type Module Part of the Atom Project Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-type
  VERSION 1.0.0
  LANGUAGES C CXX)

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
  add_library(atom-type INTERFACE)
  atom_configure_module(atom-type HEADER_ONLY)
else()
  add_library(atom-type STATIC ${SOURCES} ${HEADERS})
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

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/type)
