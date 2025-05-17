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

# Sources
set(SOURCES
    address.cpp
    curl.cpp
    downloader.cpp
    httpparser.cpp
    minetype.cpp
    utils.cpp
    time.cpp
)

# Headers
set(HEADERS
    address.hpp
    curl.hpp
    downloader.hpp
    httpparser.hpp
    minetype.hpp
    utils.hpp
    time.hpp
)

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

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
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