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
    # Async operations
    async/async_compress.cpp
    async/async_glob.cpp
    async/async_io.cpp

    # Compression functionality
    compression/compress.cpp

    # Filesystem operations
    filesystem/file_info.cpp
    filesystem/file_permission.cpp
    filesystem/pushd.cpp

    # Core I/O
    core/io.cpp
)

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    async_compress.hpp
    async_glob.hpp
    async_io.hpp
    compress.hpp
    file_info.hpp
    file_permission.hpp
    glob.hpp
    io.hpp
    pushd.hpp

    # Actual implementation headers (in subdirectories)
    async/async_compress.hpp
    async/async_glob.hpp
    async/async_io.hpp

    compression/compress.hpp

    filesystem/file_info.hpp
    filesystem/file_permission.hpp
    filesystem/pushd.hpp

    core/glob.hpp
    core/io.hpp
)

# Dependencies
set(LIBS
    loguru
    MINIZIP::minizip
    ZLIB::ZLIB
    ${CMAKE_THREAD_LIBS_INIT}
)

find_package(TBB REQUIRED)
if(TBB_FOUND)
    list(APPEND LIBS TBB::tbb)
endif()

if(WIN32)
    list(APPEND LIBS ws2_32 wsock32)
endif()

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
