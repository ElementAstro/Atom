# CMakeLists.txt for Atom-Component
# This project adheres to the GPL3 license.
#
# Project Details:
#   Name: Atom-Component
#   Description: Central component library for the Atom framework
#   Author: Max Qian
#   License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-component VERSION 1.0.0 LANGUAGES C CXX)

# Source files
set(SOURCES
    advanced_bindings.cpp
    component.cpp
    component_pool.cpp
    dispatch.cpp
    iteration.cpp
    lifecycle.cpp
    registry.cpp
    script_engine.cpp
    script_sandbox.cpp
    scripting_api.cpp
    serialization.cpp
    var.cpp
)

# Header files
set(HEADERS
    advanced_bindings.hpp
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
    type_conversion.hpp
    types.hpp
    var.hpp
)

# Optional scripting engine support
option(ATOM_ENABLE_LUA "Enable Lua scripting support" OFF)
option(ATOM_ENABLE_PYTHON "Enable Python scripting support" OFF)

# Conditional source files
set(OPTIONAL_SOURCES)
set(OPTIONAL_HEADERS)

# Lua support
if(ATOM_ENABLE_LUA)
    list(APPEND OPTIONAL_SOURCES lua_engine.cpp)
    list(APPEND OPTIONAL_HEADERS lua_engine.hpp)
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
    list(APPEND OPTIONAL_SOURCES python_engine.cpp)
    list(APPEND OPTIONAL_HEADERS python_engine.hpp)
    message(STATUS "Python scripting support enabled")

    # Find Python
    find_package(Python3 COMPONENTS Interpreter Development QUIET)
    if(Python3_FOUND)
        list(APPEND LIBS ${Python3_LIBRARIES})
        include_directories(${Python3_INCLUDE_DIRS})
        add_definitions(-DATOM_ENABLE_PYTHON=1)
    else()
        message(WARNING "Python3 not found, disabling Python support")
        set(ATOM_ENABLE_PYTHON OFF)
    endif()
endif()

# Dependencies
set(LIBS
    loguru
    atom-error
    atom-utils
    ${CMAKE_THREAD_LIBS_INIT}
)

# Include directories
include_directories(.)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS} ${OPTIONAL_SOURCES} ${OPTIONAL_HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Build Shared Library
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install headers
install(FILES ${HEADERS} ${OPTIONAL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/components
)

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

        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)