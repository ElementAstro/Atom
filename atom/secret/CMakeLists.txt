# CMakeLists.txt for Atom-Secret
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Secret
# Description: Secret Management Library for Atom
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-secret VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(SOURCES
    encryption.cpp
    password_manager.cpp
    password_utils.cpp
    serialization.cpp
    storage.cpp
)

set(HEADERS
    common.hpp
    encryption.hpp
    password_entry.hpp
    password_manager.hpp
    password_utils.hpp
    result.hpp
    serialization.hpp
    storage.hpp
)

set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
)

# Find required packages
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

# Find spdlog
find_package(spdlog QUIET)
if(spdlog_FOUND)
    list(APPEND LIBS spdlog::spdlog)
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

if (LINUX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GLIB_LIBRARIES} ${LIBSECRET_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${GLIB_INCLUDE_DIRS} ${LIBSECRET_INCLUDE_DIRS})
endif()

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
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
)

# Register this module to the global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${PROJECT_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
