# CMakeLists.txt for Atom-Sysinfo
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Sysinfo
# Description: System Information Library for Atom
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-sysinfo VERSION 1.0.0 LANGUAGES C CXX)

# 添加子模块子目录
add_subdirectory(hardware/memory)
add_subdirectory(network/wifi)

# Hardware Sources
set(HARDWARE_SOURCES
    hardware/battery.cpp
    hardware/bios.cpp
    hardware/gpu.cpp
)

# CPU component files
set(CPU_SOURCES
    hardware/cpu/common.cpp
    hardware/cpu/windows.cpp
    hardware/cpu/linux.cpp
    hardware/cpu/macos.cpp
    hardware/cpu/freebsd.cpp
)

set(CPU_HEADERS
    hardware/cpu/common.hpp
)

# Storage Sources
set(STORAGE_SOURCES
    storage/disk/disk_device.cpp
    storage/disk/disk_info.cpp
    storage/disk/disk_monitor.cpp
    storage/disk/disk_security.cpp
    storage/disk/disk_util.cpp
)

set(STORAGE_HEADERS
    storage/disk/disk_device.hpp
    storage/disk/disk_info.hpp
    storage/disk/disk_monitor.hpp
    storage/disk/disk_security.hpp
    storage/disk/disk_types.hpp
    storage/disk/disk_util.hpp
)

# Info Sources
set(INFO_SOURCES
    info/locale.cpp
    info/os.cpp
    info/wm.cpp
    info/sn.cpp
    info/virtual.cpp
)

# Utils Sources
set(UTILS_SOURCES
    utils/sysinfo_printer.cpp
)

# Hardware Headers
set(HARDWARE_HEADERS
    hardware/battery.hpp
    hardware/bios.hpp
    hardware/cpu.hpp
    hardware/gpu.hpp
    hardware/memory.hpp
)

# Storage Headers
set(STORAGE_HEADERS
    storage/disk.hpp
)

# Network Headers
set(NETWORK_HEADERS
    network/wifi.hpp
)

# Info Headers
set(INFO_HEADERS
    info/locale.hpp
    info/os.hpp
    info/wm.hpp
    info/sn.hpp
    info/virtual.hpp
)

# Utils Headers
set(UTILS_HEADERS
    utils/sysinfo_printer.hpp
)

# Backwards compatibility headers
set(COMPAT_HEADERS
    battery.hpp
    bios.hpp
    cpu.hpp
    disk.hpp
    gpu.hpp
    locale.hpp
    memory.hpp
    os.hpp
    wifi.hpp
    wm.hpp
    sn.hpp
    virtual.hpp
    sysinfo_printer.hpp
)

set(LIBS

    atom_sysinfo_memory
    atom_sysinfo_wifi
    ${CMAKE_THREAD_LIBS_INIT}
)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT
    ${HARDWARE_SOURCES}
    ${CPU_SOURCES}
    ${STORAGE_SOURCES}
    ${INFO_SOURCES}
    ${UTILS_SOURCES}
    ${HARDWARE_HEADERS}
    ${CPU_HEADERS}
    ${STORAGE_HEADERS}
    ${NETWORK_HEADERS}
    ${INFO_HEADERS}
    ${UTILS_HEADERS}
    ${COMPAT_HEADERS}
)
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME}_object PUBLIC . hardware storage network info utils ../.. ../../..)
set_property(TARGET ${PROJECT_NAME}_object PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME}_object PROPERTY CXX_STANDARD_REQUIRED ON)

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC . hardware storage network info utils ../.. ../../..)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pdh wlanapi ws2_32 setupapi)
endif()

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Installation
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
)

# Register this module to the global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${PROJECT_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
