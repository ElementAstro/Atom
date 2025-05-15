# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom
# Description: Atom Library for all of the Element Astro Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom VERSION 1.0.0 LANGUAGES C CXX)

# Python Support
option(ATOM_BUILD_PYTHON "Build Atom with Python support" OFF)
if(ATOM_BUILD_PYTHON)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)
    if(PYTHON_FOUND)
        message(STATUS "Found Python ${PYTHON_VERSION_STRING}: ${PYTHON_EXECUTABLE}")
        find_package(pybind11 QUIET)
        if(pybind11_FOUND)
            message(STATUS "Found pybind11: ${pybind11_INCLUDE_DIRS}")
        else()
            message(FATAL_ERROR "pybind11 not found")
        endif()
    else()
        message(FATAL_ERROR "Python not found")
    endif()
endif()

if (LINUX)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
if(SYSTEMD_FOUND)
    message(STATUS "Found libsystemd: ${SYSTEMD_VERSION}")
endif()
endif()

# 需要构建的子目录列表
set(SUBDIRECTORIES)

# 检查每个模块是否需要构建并添加到列表
if(ATOM_BUILD_ALGORITHM)
    list(APPEND SUBDIRECTORIES algorithm)
    message(STATUS "Building algorithm module")
endif()

if(ATOM_BUILD_ASYNC)
    list(APPEND SUBDIRECTORIES async)
    message(STATUS "Building async module")
endif()

if(ATOM_BUILD_COMPONENTS)
    list(APPEND SUBDIRECTORIES components)
    message(STATUS "Building components module")
endif()

if(ATOM_BUILD_CONNECTION)
    list(APPEND SUBDIRECTORIES connection)
    message(STATUS "Building connection module")
endif()

if(ATOM_BUILD_ERROR)
    list(APPEND SUBDIRECTORIES error)
    message(STATUS "Building error module")
endif()

if(ATOM_BUILD_IO)
    list(APPEND SUBDIRECTORIES io)
    message(STATUS "Building io module")
endif()

if(ATOM_BUILD_LOG)
    list(APPEND SUBDIRECTORIES log)
    message(STATUS "Building log module")
endif()

if(ATOM_BUILD_META)
    list(APPEND SUBDIRECTORIES meta)
    message(STATUS "Building meta module")
endif()

if(ATOM_BUILD_SEARCH)
    list(APPEND SUBDIRECTORIES search)
    message(STATUS "Building search module")
endif()

if(ATOM_BUILD_SECRET)
    list(APPEND SUBDIRECTORIES secret)
    message(STATUS "Building secret module")
endif()

if(ATOM_BUILD_SYSINFO)
    list(APPEND SUBDIRECTORIES sysinfo)
    message(STATUS "Building sysinfo module")
endif()

if(ATOM_BUILD_SYSTEM)
    list(APPEND SUBDIRECTORIES system)
    message(STATUS "Building system module")
endif()

if(ATOM_BUILD_TESTS)
    list(APPEND SUBDIRECTORIES tests)
    message(STATUS "Building tests module")
endif()

if(ATOM_BUILD_UTILS)
    list(APPEND SUBDIRECTORIES utils)
    message(STATUS "Building utils module")
endif()

if(ATOM_BUILD_WEB)
    list(APPEND SUBDIRECTORIES web)
    message(STATUS "Building web module")
endif()

# 对子目录进行排序，确保依赖关系正确
if(DEFINED ATOM_MODULE_BUILD_ORDER)
    # 使用预定义的构建顺序
    set(ORDERED_SUBDIRS)
    foreach(module IN LISTS ATOM_MODULE_BUILD_ORDER)
        string(REPLACE "atom-" "" module_dir ${module})
        if(${module_dir} IN_LIST SUBDIRECTORIES)
            list(APPEND ORDERED_SUBDIRS ${module_dir})
            list(REMOVE_ITEM SUBDIRECTORIES ${module_dir})
        endif()
    endforeach()
    
    # 添加任何剩余的目录（未在构建顺序中指定的）
    list(APPEND ORDERED_SUBDIRS ${SUBDIRECTORIES})
    set(SUBDIRECTORIES ${ORDERED_SUBDIRS})
endif()

# 添加子目录
foreach(subdir ${SUBDIRECTORIES})
    add_subdirectory(${subdir})
    message(STATUS "Added subdirectory: ${subdir}")
endforeach()

# Sources and Headers for the main Atom library
set(ATOM_SOURCES)
set(ATOM_HEADERS)

# 如果构建了日志模块，添加相关源文件
if(ATOM_BUILD_LOG)
    list(APPEND ATOM_SOURCES log/atomlog.cpp log/logger.cpp)
    list(APPEND ATOM_HEADERS log/atomlog.hpp log/logger.hpp)
endif()

# Libraries - 只添加已构建的库
set(ATOM_LIBS)

if(ATOM_BUILD_LOG)
    list(APPEND ATOM_LIBS loguru)
endif()

if(ATOM_BUILD_META)
    list(APPEND ATOM_LIBS atom-meta)
endif()

if(ATOM_BUILD_ALGORITHM)
    list(APPEND ATOM_LIBS atom-algorithm)
endif()

if(ATOM_BUILD_ASYNC)
    list(APPEND ATOM_LIBS atom-async)
    list(APPEND ATOM_LIBS atom-async-io)
endif()

if(ATOM_BUILD_IO)
    list(APPEND ATOM_LIBS atom-io)
endif()

if(ATOM_BUILD_COMPONENTS)
    list(APPEND ATOM_LIBS atom-component)
endif()

if(ATOM_BUILD_UTILS)
    list(APPEND ATOM_LIBS atom-utils)
endif()

if(ATOM_BUILD_SEARCH)
    list(APPEND ATOM_LIBS atom-search)
endif()

if(ATOM_BUILD_WEB)
    list(APPEND ATOM_LIBS atom-web)
endif()

if(ATOM_BUILD_SYSTEM)
    list(APPEND ATOM_LIBS atom-system)
endif()

if(ATOM_BUILD_SYSINFO)
    list(APPEND ATOM_LIBS atom-sysinfo)
endif()

# 只有当存在源文件时才构建对象库
if(ATOM_SOURCES)
    # Object Library
    add_library(atom_object OBJECT ${ATOM_SOURCES} ${ATOM_HEADERS})

    if(WIN32)
        target_link_libraries(atom_object setupapi wsock32 ws2_32 shlwapi iphlpapi)
    endif()

    if(ATOM_LIBS)
        target_link_libraries(atom_object ${ATOM_LIBS})
    endif()

    # Static Library
    add_library(atom STATIC)
    set_target_properties(atom PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}atom${CMAKE_STATIC_LIBRARY_SUFFIX}"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    target_link_libraries(atom atom_object ${CMAKE_THREAD_LIBS_INIT})
    if(ATOM_LIBS)
        target_link_libraries(atom ${ATOM_LIBS})
    endif()

    # Install
    install(TARGETS atom
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT library
    )
else()
    message(STATUS "No source files for the main Atom library, skipping atom library creation")
endif()