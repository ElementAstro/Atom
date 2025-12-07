# CMakeLists.txt for Atom-Utils This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Utils Description: A collection of useful functions Author:
# Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-utils
  VERSION 1.0.0
  LANGUAGES C CXX)

# Sources
set(SOURCES
    # Text processing
    text/string.cpp
    text/utf.cpp
    text/valid_string.cpp
    # Time utilities
    time/time.cpp
    time/stopwatcher.cpp
    time/qdatetime.cpp
    time/qtimer.cpp
    time/qtimezone.cpp
    # Process utilities
    process/qprocess.cpp
    # Conversion utilities
    conversion/convert.cpp
    conversion/to_any.cpp
    # Cryptographic utilities
    crypto/aes.cpp
    # Random number generation
    random/random.cpp
    random/uuid.cpp
    random/lcg.cpp
    # Debug utilities
    debug/error_stack.cpp
    debug/print.cpp
    # Format utilities
    format/xml.cpp
    format/difflib.cpp)

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    aes.hpp
    aligned.hpp
    anyutils.hpp
    argsview.hpp
    bit.hpp
    color_print.hpp
    container.hpp
    convert.hpp
    cstring.hpp
    difflib.hpp
    error_stack.hpp
    lcg.hpp
    leak.hpp
    linq.hpp
    print.hpp
    qdatetime.hpp
    qprocess.hpp
    qtimer.hpp
    qtimezone.hpp
    random.hpp
    ranges.hpp
    simd_wrapper.hpp
    span.hpp
    stopwatcher.hpp
    string.hpp
    switch.hpp
    time.hpp
    to_any.hpp
    to_byte.hpp
    to_string.hpp
    utf.hpp
    uuid.hpp
    valid_string.hpp
    xml.hpp
    # Actual implementation headers (in subdirectories)
    text/string.hpp
    text/utf.hpp
    text/valid_string.hpp
    text/to_string.hpp
    text/cstring.hpp
    time/time.hpp
    time/stopwatcher.hpp
    time/qdatetime.hpp
    time/qtimer.hpp
    time/qtimezone.hpp
    process/qprocess.hpp
    conversion/convert.hpp
    conversion/to_any.hpp
    conversion/to_byte.hpp
    crypto/aes.hpp
    crypto/aes_impl.hpp
    random/random.hpp
    random/uuid.hpp
    random/lcg.hpp
    container/container.hpp
    container/ranges.hpp
    container/span.hpp
    container/linq.hpp
    memory/aligned.hpp
    memory/leak.hpp
    memory/simd_wrapper.hpp
    debug/error_stack.hpp
    debug/print.hpp
    debug/color_print.hpp
    format/xml.hpp
    format/difflib.hpp
    core/anyutils.hpp
    core/argsview.hpp
    core/bit.hpp
    core/switch.hpp)

# Use standardized dependency management
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)

# Configure module with standard dependencies
atom_configure_module(${PROJECT_NAME} DEPENDENCIES atom-type atom-error)

# Setup module-specific dependencies
atom_setup_crypto_deps(${PROJECT_NAME})
atom_setup_xml_deps(${PROJECT_NAME})
atom_setup_logging_deps(${PROJECT_NAME})
atom_find_tbb(${PROJECT_NAME})

# Find OpenSSL for crypto utilities (optional for all builds)
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
  message(STATUS "OpenSSL not found - crypto utilities will be disabled")
endif()

if(OpenSSL_FOUND)
  target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::Crypto)
  target_compile_definitions(${PROJECT_NAME} PUBLIC ATOM_HAS_OPENSSL=1)
else()
  target_compile_definitions(${PROJECT_NAME} PUBLIC ATOM_HAS_OPENSSL=0)
endif()

# Find zlib for compression utilities (optional for all builds)
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
  message(STATUS "ZLIB not found - compression utilities will be disabled")
endif()

if(ZLIB_FOUND)
  target_link_libraries(${PROJECT_NAME} PUBLIC ZLIB::ZLIB)
  target_compile_definitions(${PROJECT_NAME} PUBLIC ATOM_HAS_ZLIB=1)
else()
  target_compile_definitions(${PROJECT_NAME} PUBLIC ATOM_HAS_ZLIB=0)
endif()

# Find fmt for formatting (optional for all builds)
find_package(fmt QUIET)
if(NOT fmt_FOUND)
  message(STATUS "fmt not found - formatting features will be limited")
endif()

if(fmt_FOUND)
  target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt)
endif()

# Find spdlog for logging
find_package(spdlog QUIET)
if(spdlog_FOUND)
  target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
  # Ensure spdlog is used as compiled library, not header-only
  target_compile_definitions(${PROJECT_NAME} PUBLIC SPDLOG_COMPILED_LIB
                                                    SPDLOG_FMT_EXTERNAL)
endif()

# Link to atom-error (required dependency)
if(TARGET atom-error)
  target_link_libraries(${PROJECT_NAME} PUBLIC atom-error)
endif()

# Platform-specific libraries
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PUBLIC iphlpapi)
endif()

# Copy dependencies to object library
target_link_libraries(${PROJECT_NAME}_object
                      PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},LINK_LIBRARIES>)

# Set library properties
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Installation
install(
  TARGETS ${PROJECT_NAME}
  EXPORT atom-utils-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-utils-targets
  FILE atom-utilsTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers preserving directory structure
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils)

# Install subdirectory headers
install(
  DIRECTORY text/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/text
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY time/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/time
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY process/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/process
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY conversion/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/conversion
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY crypto/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/crypto
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY random/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/random
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY debug/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/debug
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY format/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/format
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY container/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/container
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY memory/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/memory
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY core/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/utils/core
  FILES_MATCHING
  PATTERN "*.hpp")
