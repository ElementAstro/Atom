# CMakeLists.txt for Atom-IO
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-IO
# Description: IO Components for Element Astro Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-io VERSION 1.0.0 LANGUAGES C CXX)

# Sources
set(SOURCES
    compress.cpp
    file_permission.cpp
    io.cpp
    pushd.cpp
)

# Headers
set(HEADERS
    compress.hpp
    file_permission.hpp
    glob.hpp
    io.hpp
    pushd.hpp
)

# Dependencies
set(LIBS
    loguru
    MINIZIP::minizip
    ZLIB::ZLIB
    ${CMAKE_THREAD_LIBS_INIT}
)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Async IO
set(ASYNC_SOURCES
    async_compress.cpp
    async_glob.cpp
    async_io.cpp
)

set(ASYNC_HEADERS
    async_compress.hpp
    async_glob.hpp
    async_io.hpp
)

set(ASYNC_LIBS
    atom-io
    atom-async
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
)

add_library(atom-async-io_object OBJECT ${ASYNC_SOURCES} ${ASYNC_HEADERS})
set_property(TARGET atom-async-io_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(atom-async-io_object PRIVATE ${ASYNC_LIBS})

add_library(atom-async-io STATIC $<TARGET_OBJECTS:atom-async-io_object>)
target_link_libraries(atom-async-io PRIVATE ${ASYNC_LIBS})
target_include_directories(atom-async-io PUBLIC .)

set_target_properties(atom-async-io PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME atom-async-io
)