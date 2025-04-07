# CMakeLists.txt for Atom-Algorithm
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Algorithm
# Description: A collection of algorithms
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-algorithm VERSION 1.0.0 LANGUAGES C CXX)

# Automatically collect source and header files
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

set(LIBS
    loguru
    atom-error
)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_object ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)