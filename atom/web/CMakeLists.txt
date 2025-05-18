# CMakeLists.txt for Atom-Web
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Web
# Description: Web API
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-web VERSION 1.0.0 LANGUAGES C CXX)

find_package(CURL REQUIRED)

# Add time subdirectory
add_subdirectory(address)
add_subdirectory(time)

# Sources
set(SOURCES
    curl.cpp
    downloader.cpp
    httpparser.cpp
    minetype.cpp
    utils/addr_info.cpp
    utils/dns.cpp
    utils/ip.cpp
    utils/network.cpp
    utils/port.cpp
    utils/socket.cpp
)

# Add time module sources
list(APPEND SOURCES ${TIME_SOURCES})

# Headers
set(HEADERS
    curl.hpp
    downloader.hpp
    httpparser.hpp
    minetype.hpp
    utils.hpp
    utils/common.hpp
    utils/addr_info.hpp
    utils/dns.hpp
    utils/ip.hpp
    utils/network.hpp
    utils/port.hpp
    utils/socket.hpp
)

# Add time module headers
list(APPEND HEADERS ${TIME_HEADERS})

# Dependencies
set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
    ${CURL_LIBRARIES}
)

if(WIN32)
   list(APPEND LIBS wsock32 ws2_32)
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Link with time module
target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS} atom-web-time)

# Build Static Library
add_library(${PROJECT_NAME} STATIC 
    $<TARGET_OBJECTS:${PROJECT_NAME}_object>
    $<TARGET_OBJECTS:atom-web-time>
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Set library properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Installation
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)