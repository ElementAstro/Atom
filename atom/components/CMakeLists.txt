# CMakeLists.txt for Atom-Component This project adheres to the GPL3 license.
#
# Project Details: Name: Atom-Component Description: Central component library
# for the Atom framework Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-component
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Source files
set(SOURCES
    # Core components
    core/component.cpp
    core/component_pool.cpp
    core/registry.cpp
    # Scripting components
    scripting/bindings.cpp
    scripting/script_engine.cpp
    scripting/script_sandbox.cpp
    scripting/scripting_api.cpp
    # Lifecycle components
    lifecycle/dispatch.cpp
    lifecycle/iteration.cpp
    lifecycle/lifecycle.cpp
    # Data components
    data/serialization.cpp
    data/var.cpp)

# Header files
set(HEADERS
    # Backwards compatibility headers (in root)
    component.hpp
    component_pool.hpp
    dispatch.hpp
    iteration.hpp
    lifecycle.hpp
    lua_engine.hpp
    python_engine.hpp
    script_engine.hpp
    script_sandbox.hpp
    scripting_api.hpp
    serialization.hpp
    types.hpp
    var.hpp
    module_macro.hpp
    package.hpp
    # Actual implementation headers (in subdirectories)
    core/component.hpp
    core/component_pool.hpp
    core/registry.hpp
    core/types.hpp
    core/module_macro.hpp
    core/package.hpp
    scripting/bindings.hpp
    scripting/script_engine.hpp
    scripting/script_sandbox.hpp
    scripting/scripting_api.hpp
    lifecycle/dispatch.hpp
    lifecycle/iteration.hpp
    lifecycle/lifecycle.hpp
    data/serialization.hpp
    data/var.hpp)

# Optional scripting engine support
option(ATOM_ENABLE_LUA "Enable Lua scripting support" OFF)
option(ATOM_ENABLE_PYTHON "Enable Python scripting support" OFF)

# Component event system (emitEvent/on/once/off + registry-level pub/sub)
option(ATOM_COMPONENTS_ENABLE_EVENTS "Enable component event system" ON)

# Conditional source files
set(OPTIONAL_SOURCES)
set(OPTIONAL_HEADERS)

# Lua support
if(ATOM_ENABLE_LUA)
  list(APPEND OPTIONAL_SOURCES scripting/lua_engine.cpp)
  list(APPEND OPTIONAL_HEADERS scripting/lua_engine.hpp)
  message(STATUS "Lua scripting support enabled")

  # Find Lua
  find_package(Lua QUIET)
  if(LUA_FOUND)
    list(APPEND LIBS ${LUA_LIBRARIES})
    include_directories(${LUA_INCLUDE_DIR})
    add_definitions(-DATOM_ENABLE_LUA=1)
  else()
    message(WARNING "Lua not found, disabling Lua support")
    set(ATOM_ENABLE_LUA OFF)
  endif()
endif()

# Python support
if(ATOM_ENABLE_PYTHON)
  list(APPEND OPTIONAL_SOURCES scripting/python_engine.cpp)
  list(APPEND OPTIONAL_HEADERS scripting/python_engine.hpp)
  message(STATUS "Python scripting support enabled")

  # Find Python
  find_package(
    Python3
    COMPONENTS Interpreter Development
    QUIET)
  if(Python3_FOUND)
    list(APPEND LIBS ${Python3_LIBRARIES})
    include_directories(${Python3_INCLUDE_DIRS})
    add_definitions(-DATOM_ENABLE_PYTHON=1)
  else()
    message(WARNING "Python3 not found, disabling Python support")
    set(ATOM_ENABLE_PYTHON OFF)
  endif()
endif()

# Dependencies - link to the import libraries
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/atom/error)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/atom/utils)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/atom/memory)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/atom/type)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/atom/log)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build/extra)

# Find fmt if not already found
if(NOT TARGET fmt::fmt)
  find_package(fmt QUIET)
endif()

# Find spdlog for logging
find_package(spdlog QUIET)

set(LIBS atom-error $<$<TARGET_EXISTS:fmt::fmt>:fmt::fmt>
         ${CMAKE_THREAD_LIBS_INIT})

if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

# Create library target (shared library for components)
add_library(atom-component SHARED ${SOURCES} ${HEADERS} ${OPTIONAL_SOURCES}
                                  ${OPTIONAL_HEADERS})
add_library(atom::component ALIAS atom-component)

# Configure module using standardized function
atom_configure_module(atom-component)

# Link module-specific dependencies
target_link_libraries(atom-component PRIVATE ${LIBS})

# Set C++20 for the target
target_compile_features(atom-component PUBLIC cxx_std_20)

if(ATOM_COMPONENTS_ENABLE_EVENTS)
  target_compile_definitions(atom-component PUBLIC ENABLE_EVENT_SYSTEM=1)
endif()

# Install library target
install(
  TARGETS atom-component
  EXPORT atom-component-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-component-targets
  FILE atom-componentTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers
install(FILES ${HEADERS} ${OPTIONAL_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components)

# Install headers Install backwards compatibility headers
install(
  FILES component.hpp
        component_pool.hpp
        dispatch.hpp
        iteration.hpp
        lifecycle.hpp
        lua_engine.hpp
        python_engine.hpp
        script_engine.hpp
        script_sandbox.hpp
        scripting_api.hpp
        serialization.hpp
        types.hpp
        var.hpp
        module_macro.hpp
        package.hpp
        ${OPTIONAL_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components)

# Install implementation headers in subdirectories
install(
  DIRECTORY core/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components/core
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY scripting/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components/scripting
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY lifecycle/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components/lifecycle
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY data/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components/data
  FILES_MATCHING
  PATTERN "*.hpp")

# Testing
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS AND ATOM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  if(EXISTS ${DOXYGEN_IN})
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(
      docs
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM)
  endif()
endif()

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
