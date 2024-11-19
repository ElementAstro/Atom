# CMakeLists.txt for Atom-System
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-System
# Description: A collection of useful system functions
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-system VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(SOURCES
    command.cpp
    crash_quotes.cpp
    crash.cpp
    env.cpp
    lregistry.cpp
    pidwatcher.cpp
    power.cpp
    process.cpp
    software.cpp
    storage.cpp
    user.cpp
    wregistry.cpp
)

set(HEADERS
    command.hpp
    crash_quotes.hpp
    crash.hpp
    env.hpp
    pidwatcher.hpp
    platform.hpp
    power.hpp
    process.hpp
    software.hpp
    storage.hpp
    user.hpp
    wregistry.hpp
)

set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
    atom-sysinfo
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
    target_link_libraries(${PROJECT_NAME} PRIVATE pdh wlanapi userenv)
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