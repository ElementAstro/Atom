# CMakeLists.txt for Atom-Web This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Web Description: Web API Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-web
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

find_package(CURL QUIET)
if(CURL_FOUND)
  message(STATUS "CURL found - HTTP client features enabled")
else()
  message(STATUS "CURL not found - HTTP client features will be disabled")
endif()

# Add time subdirectory
add_subdirectory(address)
add_subdirectory(time)

# Sources
set(SOURCES
    # HTTP functionality (always-available parser)
    http/httpparser.cpp
    # MIME type functionality
    mime/minetype.cpp
    # Utility functionality
    utils/addr_info.cpp
    utils/dns.cpp
    utils/ip.cpp
    utils/network.cpp
    utils/port.cpp
    utils/socket.cpp)

# Optionally include libcurl-backed sources
if(CURL_FOUND)
  list(APPEND SOURCES http/curl.cpp http/downloader.cpp)
endif()

# Add time module sources
list(APPEND SOURCES ${TIME_SOURCES})

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    curl.hpp
    downloader.hpp
    httpparser.hpp
    minetype.hpp
    address.hpp
    time.hpp
    utils.hpp
    # Actual implementation headers (in subdirectories)
    http/curl.hpp
    http/downloader.hpp
    http/httpparser.hpp
    mime/minetype.hpp
    utils/common.hpp
    utils/addr_info.hpp
    utils/dns.hpp
    utils/ip.hpp
    utils/network.hpp
    utils/port.hpp
    utils/socket.hpp)

# Add time module headers
list(APPEND HEADERS ${TIME_HEADERS})

# Dependencies Avoid hard-coding loguru; rely on configured logging deps
set(LIBS ${CMAKE_THREAD_LIBS_INIT})

# Add fmt library for spdlog support
find_package(fmt QUIET)
if(fmt_FOUND)
  list(APPEND LIBS fmt::fmt)
endif()

# Add spdlog library (use compiled version to avoid ODR violations)
find_package(spdlog QUIET)
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

if(CURL_FOUND)
  list(APPEND LIBS ${CURL_LIBRARIES})
endif()

if(WIN32)
  list(APPEND LIBS wsock32 ws2_32)
endif()

# Create library target (including submodule objects)
add_library(
  atom-web STATIC ${SOURCES} ${HEADERS} $<TARGET_OBJECTS:atom-web-time>
                  $<TARGET_OBJECTS:atom-web-address>)
add_library(atom::web ALIAS atom-web)

# Configure module using standardized function and declare inter-module deps
# atom-web consumes atom-utils (net/socket helpers), atom-io (potential reuse),
# atom-system (process/user utilities), atom-log (logging helpers), atom-error
# (added by helper)
atom_configure_module(
  atom-web
  DEPENDENCIES
  atom-utils
  atom-io
  atom-system
  atom-log
  atom-type)

# Ensure spdlog is used as compiled library
target_compile_definitions(atom-web PRIVATE SPDLOG_COMPILED_LIB
                                            SPDLOG_FMT_EXTERNAL)

# Link module-specific dependencies
target_link_libraries(atom-web PRIVATE ${LIBS} atom-web-time)

if(CURL_FOUND)
  target_include_directories(atom-web PRIVATE ${CURL_INCLUDE_DIRS})
  target_compile_definitions(atom-web PUBLIC ATOM_WEB_HAVE_CURL=1)
else()
  target_compile_definitions(atom-web PUBLIC ATOM_WEB_HAVE_CURL=0)
endif()

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/web)
