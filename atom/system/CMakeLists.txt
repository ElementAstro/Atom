# CMakeLists.txt for Atom-System This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-System Description: A collection of useful system functions
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-system
  VERSION 1.0.0
  LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

if(LIBUSB_FOUND)
  message(STATUS "Found libusb-1.0: ${LIBUSB_VERSION}")
endif()

# Sources and Headers
set(SOURCES
    command.cpp
    command/executor.cpp
    command/process_manager.cpp
    command/advanced_executor.cpp
    command/utils.cpp
    command/history.cpp
    crash_quotes.cpp
    crash.cpp
    crontab.cpp
    crontab/cron_job.cpp
    crontab/cron_validation.cpp
    crontab/cron_system.cpp
    crontab/cron_storage.cpp
    crontab/cron_manager.cpp
    device.cpp
    env.cpp
    gpio.cpp
    lregistry.cpp
    network_manager.cpp
    pidwatcher.cpp
    power.cpp
    priority.cpp
    process_manager.cpp
    process.cpp
    signal.cpp
    software.cpp
    storage.cpp
    user.cpp
    wregistry.cpp)

set(HEADERS
    command.hpp
    command/executor.hpp
    command/process_manager.hpp
    command/advanced_executor.hpp
    command/utils.hpp
    command/history.hpp
    crash_quotes.hpp
    crash.hpp
    crontab.hpp
    crontab/cron_job.hpp
    crontab/cron_validation.hpp
    crontab/cron_system.hpp
    crontab/cron_storage.hpp
    crontab/cron_manager.hpp
    env.hpp
    gpio.hpp
    lregistry.hpp
    network_manager.hpp
    pidwatcher.hpp
    platform.hpp
    power.hpp
    process.hpp
    software.hpp
    storage.hpp
    user.hpp
    wregistry.hpp)

set(LIBS loguru ${CMAKE_THREAD_LIBS_INIT} atom-sysinfo atom-meta
         ${LIBUSB_LIBRARIES})

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
  target_link_libraries(${PROJECT_NAME} PRIVATE pdh wlanapi userenv)
endif()

# Set library properties
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Installation
install(
  TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION include/${PROJECT_NAME})
