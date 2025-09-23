# CMakeLists.txt for Atom-System
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-System
# Description: A collection of useful system functions
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-system VERSION 1.0.0 LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBUSB QUIET libusb-1.0)

if(LIBUSB_FOUND)
    message(STATUS "Found libusb-1.0: ${LIBUSB_VERSION}")
else()
    message(STATUS "libusb-1.0 not found - USB device support will be disabled")
endif()

# Sources and Headers
set(SOURCES
    # Core files
    core/priority.cpp

    # Process management
    process/command.cpp
    process/pidwatcher.cpp
    process/process_manager.cpp
    process/process.cpp

    # Hardware
    hardware/device.cpp
    hardware/gpio.cpp
    hardware/voltage.cpp

    # Power management
    power/power.cpp

    # System information
    info/env.cpp
    info/software.cpp
    info/stat.cpp
    info/user.cpp

    # Registry
    registry/lregistry.cpp
    registry/wregistry.cpp

    # Network
    network/network_manager.cpp
    network/virtual_network.cpp

    # Storage
    storage/storage.cpp

    # Signals
    signals/signal.cpp

    # Debug
    debug/crash_quotes.cpp
    debug/crash.cpp
    debug/nodebugger.cpp

    # Scheduling
    scheduling/crontab.cpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND SOURCES
        clipboard/clipboard.cpp
        clipboard/clipboard_windows.cpp
        hardware/voltage_windows.cpp
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND SOURCES
        clipboard/clipboard.cpp
        clipboard/clipboard_linux.cpp
        hardware/voltage_linux.cpp
    )
elseif(APPLE)
    list(APPEND SOURCES
        clipboard/clipboard.cpp
        clipboard/clipboard_macos.cpp
    )
endif()

set(HEADERS
    # Backwards compatibility headers (in root)
    _constant.hpp
    clipboard.hpp
    clipboard.ipp
    clipboard_error.hpp
    command.hpp
    crash_quotes.hpp
    crash.hpp
    crontab.hpp
    device.hpp
    env.hpp
    gpio.hpp
    lregistry.hpp
    network_manager.hpp
    nodebugger.hpp
    pidwatcher.hpp
    platform.hpp
    power.hpp
    priority.hpp
    process.hpp
    process_info.hpp
    process_manager.hpp
    signal.hpp
    signal_monitor.hpp
    signal_utils.hpp
    software.hpp
    stat.hpp
    storage.hpp
    user.hpp
    virtual_network.hpp
    voltage.hpp
    voltage_linux.hpp
    voltage_windows.hpp
    wregistry.hpp

    # Actual implementation headers (in subdirectories)
    core/_constant.hpp
    core/platform.hpp
    core/priority.hpp

    process/command.hpp
    process/pidwatcher.hpp
    process/process.hpp
    process/process_info.hpp
    process/process_manager.hpp

    hardware/device.hpp
    hardware/gpio.hpp
    hardware/voltage.hpp
    hardware/voltage_linux.hpp
    hardware/voltage_windows.hpp

    power/power.hpp

    info/env.hpp
    info/software.hpp
    info/stat.hpp
    info/user.hpp

    registry/lregistry.hpp
    registry/wregistry.hpp

    network/network_manager.hpp
    network/virtual_network.hpp

    storage/storage.hpp

    clipboard/clipboard.hpp
    clipboard/clipboard.ipp
    clipboard/clipboard_error.hpp

    signals/signal.hpp
    signals/signal_monitor.hpp
    signals/signal_utils.hpp

    debug/crash.hpp
    debug/crash_quotes.hpp
    debug/nodebugger.hpp

    scheduling/crontab.hpp
)

set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
    atom-sysinfo
    atom-meta
    atom-utils
    ${LIBUSB_LIBRARIES}
)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pdh wlanapi userenv version)
endif()

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Add shortcut subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shortcut/CMakeLists.txt")
    add_subdirectory(shortcut)
endif()

# Installation
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
)
