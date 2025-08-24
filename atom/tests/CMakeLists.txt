# CMakeLists.txt for Atom-Tests
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Tests
# Description: Test Suite for Atom
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-tests VERSION 1.0.0 LANGUAGES C CXX)

# Sources and Headers
set(SOURCES
    benchmark.cpp
    fuzz.cpp
    perf.cpp
)

set(HEADERS
    benchmark.hpp
    fuzz.hpp
    perf.hpp
    test_cli.hpp
    test_registry.hpp
    test_reporter.hpp
    test_runner.hpp
    test.hpp
)

# Find nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
    endif()
endif()

set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
)

# Add nlohmann/json if found
if(nlohmann_json_FOUND)
    list(APPEND LIBS nlohmann_json::nlohmann_json)
elseif(NLOHMANN_JSON_FOUND)
    list(APPEND LIBS ${NLOHMANN_JSON_LIBRARIES})
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Add nlohmann/json include directories if found via pkg-config
if(NLOHMANN_JSON_FOUND AND NOT nlohmann_json_FOUND)
    target_include_directories(${PROJECT_NAME}_object PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Add nlohmann/json include directories if found via pkg-config
if(NLOHMANN_JSON_FOUND AND NOT nlohmann_json_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pdh wlanapi)
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
