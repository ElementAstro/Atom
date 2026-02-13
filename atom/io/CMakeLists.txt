# CMakeLists.txt for Atom-IO This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-IO Description: IO Components for Element Astro Project
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-io
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES
    # Compression functionality (split into sub-modules)
    compression/gz_compress.cpp
    compression/zip_operations.cpp
    compression/slice_compress.cpp
    compression/data_compress.cpp
    compression/backup.cpp
    # Filesystem operations
    filesystem/file_info.cpp
    filesystem/file_ops.cpp
    filesystem/file_permission.cpp
    filesystem/file_permission_change.cpp
    filesystem/directory_stack.cpp
    filesystem/directory_stack_navigation.cpp
    filesystem/directory_stack_persistence.cpp
    # Core I/O
    core/io.cpp)

# Headers
set(HEADERS
    # Barrel export header
    index.hpp
    # Implementation headers (in subdirectories)
    async/async_types.hpp
    async/async_file.hpp
    async/async_directory.hpp
    async/async_batch.hpp
    async/async_stream.hpp
    async/async_simd.hpp
    async/async_compressor.hpp
    async/async_decompressor.hpp
    async/async_zip.hpp
    async/async_compress.hpp
    async/async_glob.hpp
    async/async_io.hpp
    compression/compress.hpp
    compression/types.hpp
    compression/utils.hpp
    compression/gz_compress.hpp
    compression/zip_operations.hpp
    compression/slice_compress.hpp
    compression/data_compress.hpp
    compression/backup.hpp
    filesystem/file_info.hpp
    filesystem/file_ops.hpp
    filesystem/file_permission.hpp
    filesystem/file_permission_change.hpp
    filesystem/task.hpp
    filesystem/directory_stack.hpp
    filesystem/directory_stack_impl.hpp
    filesystem/pushd.hpp
    core/glob.hpp
    core/io.hpp
    core/types.hpp
    core/path_convert.hpp
    core/file_query.hpp
    core/file_ops.hpp
    core/directory_ops.hpp
    core/directory_walk.hpp
    core/file_split_merge.hpp
    core/path_utils.hpp)

# Dependencies
find_package(spdlog QUIET)
set(LIBS ${CMAKE_THREAD_LIBS_INIT})
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

# Add ZLIB if available (optional for MSVC)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
  list(APPEND LIBS ZLIB::ZLIB)
endif()

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
  message(
    STATUS "TBB not found - parallel algorithms will use standard library")
endif()

if(WIN32)
  list(APPEND LIBS ws2_32 wsock32)
endif()

# Check for ASIO support and add async sources if available
set(ASYNC_SOURCES
    async/async_file.cpp
    async/async_directory.cpp
    async/async_batch.cpp
    async/async_stream.cpp
    async/async_simd.cpp
    async/async_compressor.cpp
    async/async_decompressor.cpp
    async/async_zip.cpp
    async/async_glob.cpp)

find_package(asio QUIET)
if(NOT asio_FOUND)
  find_path(ASIO_INCLUDE_DIR NAMES asio.hpp)
endif()

if(asio_FOUND OR ASIO_INCLUDE_DIR)
  list(APPEND SOURCES ${ASYNC_SOURCES})
  message(STATUS "ASIO found - async I/O features enabled")
else()
  message(STATUS "ASIO not found - async I/O features disabled")
endif()

# Create library target
add_library(atom-io STATIC ${SOURCES} ${HEADERS})
add_library(atom::io ALIAS atom-io)

# Configure module using standardized function
atom_configure_module(atom-io)

# Link module-specific dependencies
target_link_libraries(atom-io PRIVATE ${LIBS} atom-async)

# Apply ASIO configuration
if(asio_FOUND)
  target_compile_definitions(atom-io PUBLIC ASIO_STANDALONE ATOM_USE_ASIO)
  target_link_libraries(atom-io PUBLIC asio::asio)
elseif(ASIO_INCLUDE_DIR)
  target_compile_definitions(atom-io PUBLIC ASIO_STANDALONE ATOM_USE_ASIO)
  target_include_directories(atom-io PUBLIC ${ASIO_INCLUDE_DIR})
endif()

# Apply minizip definitions if not found
if(ATOM_IO_NO_MINIZIP)
  target_compile_definitions(atom-io PRIVATE ATOM_IO_NO_MINIZIP)
endif()

# Install headers preserving subdirectory structure
foreach(_header ${HEADERS})
  get_filename_component(_dir ${_header} DIRECTORY)
  install(FILES ${_header}
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/io/${_dir})
endforeach()
