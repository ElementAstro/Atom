# CMakeLists.txt for Serial Module
# Part of the Atom Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)

# Define library name
set(LIB_NAME atom-serial)

# Find all source files
file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")
if(APPLE)
    file(GLOB_RECURSE MM_SOURCES "*.mm")
    list(APPEND SOURCES ${MM_SOURCES})
endif()

# Create library target
add_library(${LIB_NAME} ${SOURCES} ${HEADERS})

# Setup include directories
target_include_directories(${LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/atom/serial>
)

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
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    
    target_include_directories(${LIB_NAME} PUBLIC
        ${UDEV_INCLUDE_DIRS}
        ${LIBUSB_INCLUDE_DIRS}
    )
    
    target_link_libraries(${LIB_NAME} 
        PUBLIC
        atom-error
        atom-log
        ${UDEV_LIBRARIES}
        ${LIBUSB_LIBRARIES}
    )
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

# Add module to global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${LIB_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")

# Installation rules
install(TARGETS ${LIB_NAME}
    EXPORT ${LIB_NAME}-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/serial
)

message(STATUS "Serial module configured")
