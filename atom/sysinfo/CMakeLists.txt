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
add_subdirectory(memory)
add_subdirectory(wifi)

# Sources and Headers
set(SOURCES
    battery.cpp
    bios.cpp
    gpu.cpp
    locale.cpp
    os.cpp
    # wifi.cpp is now in its own subdirectory
    wm.cpp
)

# CPU component files
set(CPU_SOURCES
    cpu/common.cpp
    cpu/windows.cpp
    cpu/linux.cpp
    cpu/macos.cpp
    cpu/freebsd.cpp
)

set(CPU_HEADERS
    cpu/common.hpp
)

set(HEADERS
    battery.hpp
    bios.hpp
    disk.hpp
    gpu.hpp
    locale.hpp
    memory.hpp
    os.hpp
    wifi.hpp
    wm.hpp
)

set(LIBS
    loguru
    atom_sysinfo_memory
    atom_sysinfo_wifi
    ${CMAKE_THREAD_LIBS_INIT}
)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS} ${CPU_SOURCES} ${CPU_HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

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