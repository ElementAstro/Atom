# CMakeLists.txt for Atom-Utils
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Utils
# Description: A collection of useful functions
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-utils VERSION 1.0.0 LANGUAGES C CXX)

# Sources
set(SOURCES
    aes.cpp
    convert.cpp
    difflib.cpp
    error_stack.cpp
    print.cpp
    qdatetime.cpp
    qprocess.cpp
    qtimer.cpp
    qtimezone.cpp
    random.cpp
    string.cpp
    stopwatcher.cpp
    time.cpp
    to_any.cpp
    uuid.cpp
    valid_string.cpp
    xml.cpp
)

# Headers
set(HEADERS
    aes.hpp
    argsview.hpp
    convert.hpp
    difflib.hpp
    error_stack.hpp
    qdatetime.hpp
    qprocess.hpp
    qtimer.hpp
    qtimezone.hpp
    random.hpp
    string.hpp
    stopwatcher.hpp
    switch.hpp
    time.hpp
    utf.cpp
    uuid.hpp
    valid_string.hpp
    xml.hpp
)

set(LIBS
    atom-io
    atom-error
    loguru
    tinyxml2
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_THREAD_LIBS_INIT}
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