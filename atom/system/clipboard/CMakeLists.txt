# CMakeLists.txt for Atom-System Clipboard Component
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-System-Clipboard
# Description: Cross-platform clipboard operations with modern C++ features
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-system-clipboard
  VERSION 1.0.0
  LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core clipboard sources
set(CLIPBOARD_CORE_SOURCES
    clipboard.cpp
    clipboard.hpp
    clipboard.ipp
    clipboard_error.hpp)

# Platform-specific sources
set(CLIPBOARD_PLATFORM_SOURCES)

# Platform-specific configuration
if(WIN32)
    list(APPEND CLIPBOARD_PLATFORM_SOURCES platform/clipboard_windows.cpp)
    set(CLIPBOARD_PLATFORM_LIBS user32 ole32 oleaut32)
    add_compile_definitions(CLIPBOARD_PLATFORM_WINDOWS)
elseif(APPLE)
    list(APPEND CLIPBOARD_PLATFORM_SOURCES platform/clipboard_macos.cpp)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(APPKIT_FRAMEWORK AppKit)
    set(CLIPBOARD_PLATFORM_LIBS ${COCOA_FRAMEWORK} ${APPKIT_FRAMEWORK})
    add_compile_definitions(CLIPBOARD_PLATFORM_MACOS)
elseif(UNIX AND NOT APPLE)
    list(APPEND CLIPBOARD_PLATFORM_SOURCES platform/clipboard_linux.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    set(CLIPBOARD_PLATFORM_LIBS ${X11_LIBRARIES})
    set(CLIPBOARD_PLATFORM_INCLUDE_DIRS ${X11_INCLUDE_DIRS})
    add_compile_definitions(CLIPBOARD_PLATFORM_LINUX)
endif()

# Optional image support
option(CLIPBOARD_SUPPORT_OPENCV "Enable OpenCV image support" OFF)
option(CLIPBOARD_SUPPORT_CIMG "Enable CImg image support" OFF)

if(CLIPBOARD_SUPPORT_OPENCV)
    find_package(OpenCV REQUIRED)
    add_compile_definitions(CLIPBOARD_SUPPORT_OPENCV)
    list(APPEND CLIPBOARD_PLATFORM_LIBS ${OpenCV_LIBS})
    list(APPEND CLIPBOARD_PLATFORM_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
endif()

if(CLIPBOARD_SUPPORT_CIMG)
    find_path(CIMG_INCLUDE_DIR CImg.h)
    if(CIMG_INCLUDE_DIR)
        add_compile_definitions(CLIPBOARD_SUPPORT_CIMG)
        list(APPEND CLIPBOARD_PLATFORM_INCLUDE_DIRS ${CIMG_INCLUDE_DIR})
    else()
        message(WARNING "CImg.h not found, disabling CImg support")
    endif()
endif()

# All sources
set(CLIPBOARD_ALL_SOURCES ${CLIPBOARD_CORE_SOURCES} ${CLIPBOARD_PLATFORM_SOURCES})

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${CLIPBOARD_ALL_SOURCES})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Include directories
target_include_directories(${PROJECT_NAME}_object
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CLIPBOARD_PLATFORM_INCLUDE_DIRS})

# Link libraries
if(CLIPBOARD_PLATFORM_LIBS)
    target_link_libraries(${PROJECT_NAME}_object PRIVATE ${CLIPBOARD_PLATFORM_LIBS})
endif()

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)

# Link libraries to static library
if(CLIPBOARD_PLATFORM_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CLIPBOARD_PLATFORM_LIBS})
endif()

# Include directories for static library
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CLIPBOARD_PLATFORM_INCLUDE_DIRS})

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME}_object PRIVATE /W4)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME}_object PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set library properties
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
    EXPORT_NAME clipboard)

# Create alias for easier usage
add_library(atom::system::clipboard ALIAS ${PROJECT_NAME})

# Installation
install(
  TARGETS ${PROJECT_NAME}
  EXPORT atom-system-clipboard-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install headers
install(
  FILES
    clipboard.hpp
    clipboard_error.hpp
  DESTINATION include/atom/system/clipboard)

# Install export targets
install(
  EXPORT atom-system-clipboard-targets
  FILE atom-system-clipboard-targets.cmake
  NAMESPACE atom::system::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom-system-clipboard)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  atom-system-clipboard-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/atom-system-clipboard-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/atom-system-clipboard-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom-system-clipboard)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/atom-system-clipboard-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/atom-system-clipboard-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom-system-clipboard)

# Add subdirectories
if(BUILD_TESTING AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_subdirectory(tests)
endif()
