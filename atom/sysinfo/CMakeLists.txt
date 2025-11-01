# CMakeLists.txt for Atom-Sysinfo This project is licensed under the terms of
# the GPL3 license.
#
# Project Name: Atom-Sysinfo Description: System Information Library for Atom
# Author: Max Qian License: GPL3

# Only set project if this is the top-level CMakeLists.txt
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.21)
  project(
    atom-sysinfo
    VERSION 1.0.0
    LANGUAGES C CXX)
endif()

# Guard against multiple inclusions
if(TARGET atom-sysinfo)
  return()
endif()

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# 添加子模块子目录 - temporarily disabled to fix duplicate target issues
# add_subdirectory(hardware/memory) add_subdirectory(network/wifi)

# Hardware Sources
set(HARDWARE_SOURCES
    hardware/battery.cpp
    hardware/bios.cpp
    hardware/gpu.cpp
    # Memory module sources (integrated to fix duplicate targets)
    hardware/memory/memory.cpp
    hardware/memory/common.cpp
    hardware/memory/windows.cpp
    hardware/memory/linux.cpp
    hardware/memory/macos.cpp)

# CPU component files
set(CPU_SOURCES
    hardware/cpu/common.cpp hardware/cpu/windows.cpp hardware/cpu/linux.cpp
    hardware/cpu/macos.cpp hardware/cpu/freebsd.cpp)

set(CPU_HEADERS hardware/cpu/common.hpp)

# Storage Sources
set(STORAGE_SOURCES
    storage/disk/disk_device.cpp storage/disk/disk_info.cpp
    storage/disk/disk_monitor.cpp storage/disk/disk_security.cpp
    storage/disk/disk_util.cpp)

# Network Sources
set(NETWORK_SOURCES # WiFi module sources (integrated to fix duplicate targets)
    network/wifi/wifi.cpp network/wifi/common.cpp network/wifi/windows.cpp
    network/wifi/linux.cpp network/wifi/macos.cpp)

set(STORAGE_HEADERS
    storage/disk/disk_device.hpp storage/disk/disk_info.hpp
    storage/disk/disk_monitor.hpp storage/disk/disk_security.hpp
    storage/disk/disk_types.hpp storage/disk/disk_util.hpp)

# Info Sources
set(INFO_SOURCES info/locale.cpp info/os.cpp info/wm.cpp info/sn.cpp
                 info/virtual.cpp)

# Utils Sources
set(UTILS_SOURCES utils/sysinfo_printer.cpp)

# Hardware Headers
set(HARDWARE_HEADERS hardware/battery.hpp hardware/bios.hpp hardware/cpu.hpp
                     hardware/gpu.hpp hardware/memory.hpp)

# Storage Headers
set(STORAGE_HEADERS storage/disk.hpp)

# Network Headers
set(NETWORK_HEADERS network/wifi.hpp)

# Info Headers
set(INFO_HEADERS info/locale.hpp info/os.hpp info/wm.hpp info/sn.hpp
                 info/virtual.hpp)

# Utils Headers
set(UTILS_HEADERS utils/sysinfo_printer.hpp)

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
    sysinfo_printer.hpp)

set(LIBS
    # Temporarily disabled to fix duplicate target issues
    # atom_sysinfo_memory
    # atom_sysinfo_wifi
    ${CMAKE_THREAD_LIBS_INIT})

# Create library target
add_library(
  atom-sysinfo STATIC
  ${HARDWARE_SOURCES}
  ${CPU_SOURCES}
  ${STORAGE_SOURCES}
  ${NETWORK_SOURCES}
  ${INFO_SOURCES}
  ${UTILS_SOURCES}
  ${HARDWARE_HEADERS}
  ${CPU_HEADERS}
  ${STORAGE_HEADERS}
  ${NETWORK_HEADERS}
  ${INFO_HEADERS}
  ${UTILS_HEADERS}
  ${COMPAT_HEADERS})

# Configure module using standardized function
atom_configure_module(atom-sysinfo)

# Link module-specific dependencies
target_link_libraries(atom-sysinfo PRIVATE ${LIBS})

# Link required libraries for sysinfo functionality
find_package(fmt QUIET)
find_package(spdlog QUIET)

set(SYSINFO_LIBS atom-error)
if(fmt_FOUND)
  list(APPEND SYSINFO_LIBS fmt::fmt)
  message(STATUS "fmt found - enhanced formatting enabled in sysinfo")
else()
  message(STATUS "fmt not found - using standard formatting in sysinfo")
endif()

if(spdlog_FOUND)
  list(APPEND SYSINFO_LIBS spdlog::spdlog)
  message(STATUS "spdlog found - enhanced logging enabled in sysinfo")
else()
  message(STATUS "spdlog not found - using standard logging in sysinfo")
endif()

target_link_libraries(atom-sysinfo PRIVATE ${SYSINFO_LIBS})

# Platform-specific libraries
if(WIN32)
  target_link_libraries(
    atom-sysinfo
    PRIVATE pdh
            wlanapi
            ws2_32
            setupapi
            iphlpapi
            icmp
            dwmapi
            comdlg32
            comctl32
            ole32
            oleaut32
            powrprof
            wbemuuid)
endif()

# Install headers
install(
  FILES ${HARDWARE_HEADERS}
        ${CPU_HEADERS}
        ${STORAGE_HEADERS}
        ${NETWORK_HEADERS}
        ${INFO_HEADERS}
        ${UTILS_HEADERS}
        ${COMPAT_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo)
