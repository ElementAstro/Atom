# CMakeLists.txt for Atom-Web-Time This project is licensed under the terms of
# the GPL3 license.
#
# Project Name: Atom-Web-Time Description: Time Management API Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)

# Sources with full paths
set(ATOM_WEB_TIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/time_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/time_manager_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/time_utils.cpp")

# Headers with full paths
set(ATOM_WEB_TIME_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/time_manager.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/time_manager_impl.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/time_error.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/time_utils.hpp")

# Add sources to parent scope for inclusion in main library
set(TIME_SOURCES
    ${ATOM_WEB_TIME_SOURCES}
    PARENT_SCOPE)
set(TIME_HEADERS
    ${ATOM_WEB_TIME_HEADERS}
    PARENT_SCOPE)

# Build Object Library if needed separately
add_library(atom-web-time OBJECT ${ATOM_WEB_TIME_SOURCES}
                                 ${ATOM_WEB_TIME_HEADERS})
set_property(TARGET atom-web-time PROPERTY POSITION_INDEPENDENT_CODE 1)

# Dependencies (avoid hard-coding loguru; rely on higher-level logging config)
set(LIBS ${CMAKE_THREAD_LIBS_INIT})

# Add spdlog library (use compiled version to avoid ODR violations)
find_package(spdlog CONFIG QUIET)
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
  target_compile_definitions(atom-web-time PRIVATE SPDLOG_COMPILED_LIB
                                                   SPDLOG_FMT_EXTERNAL)
endif()

if(WIN32)
  list(APPEND LIBS wsock32 ws2_32)
endif()

target_link_libraries(atom-web-time PRIVATE ${LIBS})

# Include directories
target_include_directories(atom-web-time PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(atom-web-time PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
