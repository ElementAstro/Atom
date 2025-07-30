# CMakeLists.txt for Atom-Sysinfo This project is licensed under the terms of
# the GPL3 license.
#
# Project Name: Atom-Sysinfo Description: System Information Library for Atom
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-sysinfo
  VERSION 1.0.0
  LANGUAGES C CXX)

# Add common utilities first
add_subdirectory(common)

# Add reorganized module subdirectories
add_subdirectory(src/bios)
add_subdirectory(src/memory)
add_subdirectory(src/wifi)
add_subdirectory(src/os)
add_subdirectory(src/virtual)
add_subdirectory(src/gpu)
add_subdirectory(src/printer)
add_subdirectory(src/battery)
add_subdirectory(src/wm)
add_subdirectory(src/locale)
add_subdirectory(src/serial)
add_subdirectory(src/cpu)
add_subdirectory(src/disk)

# Compatibility headers (root level headers that forward to src/)
set(COMPATIBILITY_HEADERS
    battery.hpp
    bios.hpp
    cpu.hpp
    disk.hpp
    gpu.hpp
    locale.hpp
    memory.hpp
    os.hpp
    sn.hpp
    virtual.hpp
    wifi.hpp
    wm.hpp
    sysinfo_printer.hpp
)

# No more root-level source files - everything is in src/ subdirectories
set(SOURCES)
set(HEADERS ${COMPATIBILITY_HEADERS})

set(LIBS loguru
    atom_sysinfo_bios
    atom_sysinfo_memory
    atom_sysinfo_wifi
    atom_sysinfo_os
    atom_sysinfo_virtual
    atom_sysinfo_gpu
    atom_sysinfo_printer
    atom_sysinfo_battery
    atom_sysinfo_wm
    atom_sysinfo_locale
    atom_sysinfo_sn
    atom_sysinfo_cpu
    atom_sysinfo_disk
    ${CMAKE_THREAD_LIBS_INIT})

# Build Interface Library (header-only compatibility layer)
add_library(${PROJECT_NAME} INTERFACE)

# Link all the modular libraries
target_link_libraries(${PROJECT_NAME} INTERFACE ${LIBS})

# Include directories for compatibility headers
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Installation - only install headers, not the interface library
# The interface library is just for convenience and shouldn't be exported
# Individual modules handle their own exports

# Install compatibility headers
install(FILES ${COMPATIBILITY_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo
)
