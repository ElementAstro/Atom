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
    ZLIB::ZLIB
    ${CMAKE_THREAD_LIBS_INIT}
)

# Try to find minizip-ng
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MINIZIP QUIET minizip-ng)
    if(MINIZIP_FOUND)
        list(APPEND LIBS ${MINIZIP_LIBRARIES})
        message(STATUS "minizip-ng found via pkg-config")
    endif()
endif()

# Fallback: try to find minizip via find_package
if(NOT MINIZIP_FOUND)
    find_package(minizip-ng QUIET)
    if(minizip-ng_FOUND)
        list(APPEND LIBS minizip-ng::minizip-ng)
        set(MINIZIP_FOUND TRUE)
        message(STATUS "minizip-ng found via find_package")
    endif()
endif()

# If still not found, check if we can build without it
if(NOT MINIZIP_FOUND)
    message(STATUS "minizip-ng not found - compression features will be limited")
    set(ATOM_IO_NO_MINIZIP TRUE)
endif()

find_package(TBB QUIET)
if(TBB_FOUND)
    list(APPEND LIBS TBB::tbb)
    message(STATUS "TBB found and will be used for parallel algorithms")
else()
    message(STATUS "TBB not found - parallel algorithms will use standard library")
endif()

if(WIN32)
    list(APPEND LIBS ws2_32 wsock32)
endif()

# Check for ASIO support and add async sources if available
find_package(asio QUIET)
if(asio_FOUND)
    # Add async sources when ASIO is available
    list(APPEND SOURCES
        async/async_compress.cpp
        async/async_glob.cpp
        async/async_io.cpp
    )
    message(STATUS "ASIO found - async I/O features enabled")
else()
    find_path(ASIO_INCLUDE_DIR NAMES asio.hpp)
    if(ASIO_INCLUDE_DIR)
        # Add async sources when ASIO is available
        list(APPEND SOURCES
            async/async_compress.cpp
            async/async_glob.cpp
            async/async_io.cpp
        )
        message(STATUS "ASIO found via include path - async I/O features enabled")
    else()
        message(STATUS "ASIO not found - async I/O features disabled")
    endif()
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Apply ASIO configuration to targets
if(asio_FOUND)
    target_compile_definitions(${PROJECT_NAME}_object PRIVATE ASIO_STANDALONE ATOM_USE_ASIO)
    target_link_libraries(${PROJECT_NAME}_object PRIVATE asio::asio)
else()
    if(ASIO_INCLUDE_DIR)
        target_compile_definitions(${PROJECT_NAME}_object PRIVATE ASIO_STANDALONE ATOM_USE_ASIO)
        target_include_directories(${PROJECT_NAME}_object PRIVATE ${ASIO_INCLUDE_DIR})
    endif()
endif()

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS} atom-async)

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS} atom-async)
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Apply ASIO definitions to static library as well
if(asio_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ASIO_STANDALONE ATOM_USE_ASIO)
    target_link_libraries(${PROJECT_NAME} PUBLIC asio::asio)
elseif(ASIO_INCLUDE_DIR)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ASIO_STANDALONE ATOM_USE_ASIO)
    target_include_directories(${PROJECT_NAME} PUBLIC ${ASIO_INCLUDE_DIR})
endif()

# Apply minizip definitions if not found
if(ATOM_IO_NO_MINIZIP)
    target_compile_definitions(${PROJECT_NAME}_object PRIVATE ATOM_IO_NO_MINIZIP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_IO_NO_MINIZIP)
endif()

# Register as Atom module
set_property(GLOBAL APPEND PROPERTY ATOM_MODULE_TARGETS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
