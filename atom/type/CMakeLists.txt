# CMakeLists.txt for Atom-Type
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Type
# Description: All of the self-implement types
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-type VERSION 1.0.0 LANGUAGES C CXX)

# Sources
set(SOURCES
    message.cpp
)

# Headers
set(HEADERS
    args.hpp
    argsview.hpp
    flatset.hpp
    json.hpp
    message.hpp
    pointer.hpp
    small_vector.hpp
    string.hpp
)

# Dependencies
set(LIBS
    atom-utils
)

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