# CMakeLists.txt for atom-meta
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: atom-meta
# Description: a library for meta programming in C++
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-meta VERSION 1.0.0 LANGUAGES C CXX)

# Sources
set(SOURCES
    global_ptr.cpp
)

# Headers
set(HEADERS
    global_ptr.hpp
)

# Dependencies
set(LIBS
)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)