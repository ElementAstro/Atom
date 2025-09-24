# CMakeLists.txt for Serial Module
# Part of the Atom Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(atom-serial VERSION 1.0.0 LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Define library name
set(LIB_NAME atom-serial)

# Check for USB support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBUSB QUIET libusb-1.0)
endif()

# Sources
set(SOURCES
    # Core files
    core/scanner.cpp
    core/serial_port.cpp

    # Bluetooth files
    bluetooth/bluetooth_serial.cpp
)

# Add USB files only if libusb is available
if(LIBUSB_FOUND)
    list(APPEND SOURCES usb/usb.cpp)
    message(STATUS "USB support enabled for atom-serial")
else()
    message(STATUS "USB support disabled for atom-serial (libusb not found)")
endif()

# Add platform-specific sources
if(APPLE)
    list(APPEND SOURCES bluetooth/bluetooth_serial_mac.mm)
endif()

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    scanner.hpp
    serial_port.hpp
    bluetooth_serial.hpp
    bluetooth_serial_mac.hpp
    bluetooth_serial_unix.hpp
    bluetooth_serial_win.hpp
    serial_port_unix.hpp
    serial_port_win.hpp
    # Actual implementation headers (in subdirectories)
    core/scanner.hpp
    core/serial_port.hpp

    bluetooth/bluetooth_serial.hpp
    bluetooth/bluetooth_serial_mac.hpp
    bluetooth/bluetooth_serial_unix.hpp
    bluetooth/bluetooth_serial_win.hpp

    platform/serial_port_unix.hpp
    platform/serial_port_win.hpp
)

# Add USB headers only if libusb is available
if(LIBUSB_FOUND)
    list(APPEND HEADERS usb.hpp usb/usb.hpp)
endif()

# Create library target
add_library(${LIB_NAME} ${SOURCES} ${HEADERS})

# Configure module using standardized function
atom_configure_module(${LIB_NAME})

# Set platform-specific dependencies
if(WIN32)
    target_link_libraries(${LIB_NAME}
        PUBLIC
        atom-error
        atom-log
        SetupAPI
        Cfgmgr32
    )
elseif(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(${LIB_NAME}
        PUBLIC
        atom-error
        atom-log
        ${IOKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
else() # Linux/Unix
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UDEV REQUIRED libudev)

    target_include_directories(${LIB_NAME} PUBLIC
        ${UDEV_INCLUDE_DIRS}
    )

    target_link_libraries(${LIB_NAME}
        PUBLIC
        atom-error
        atom-log
        ${UDEV_LIBRARIES}
    )

    # Add USB support if available
    if(LIBUSB_FOUND)
        target_include_directories(${LIB_NAME} PUBLIC ${LIBUSB_INCLUDE_DIRS})
        target_link_libraries(${LIB_NAME} PUBLIC ${LIBUSB_LIBRARIES})
        target_compile_definitions(${LIB_NAME} PUBLIC ATOM_SERIAL_HAS_USB)
    endif()
endif()

# Add Bluetooth support if available
if(WIN32)
    target_link_libraries(${LIB_NAME} PUBLIC BluetoothApis)
elseif(APPLE)
    # macOS Bluetooth support is already via IOKit and Foundation
elseif(UNIX)
    pkg_check_modules(BLUEZ QUIET bluez)
    if(BLUEZ_FOUND)
        target_include_directories(${LIB_NAME} PUBLIC ${BLUEZ_INCLUDE_DIRS})
        target_link_libraries(${LIB_NAME} PUBLIC ${BLUEZ_LIBRARIES})
        target_compile_definitions(${LIB_NAME} PUBLIC HAVE_BLUEZ)
    endif()
endif()

# Install headers
install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/serial
)

message(STATUS "Serial module configured")
