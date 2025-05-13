# CMakeLists.txt for Atom-Algorithm
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Algorithm
# Description: A collection of algorithms
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-algorithm VERSION 1.0.0 LANGUAGES C CXX)

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Find TBB package
find_package(TBB REQUIRED)

# Get dependencies from module_dependencies.cmake
if(NOT DEFINED ATOM_ALGORITHM_DEPENDS)
    set(ATOM_ALGORITHM_DEPENDS atom-error)
endif()

# Verify if dependency modules are built
foreach(dep ${ATOM_ALGORITHM_DEPENDS})
    string(REPLACE "atom-" "ATOM_BUILD_" dep_var_name ${dep})
    string(TOUPPER ${dep_var_name} dep_var_name)
    if(NOT DEFINED ${dep_var_name} OR NOT ${dep_var_name})
        message(WARNING "Module ${PROJECT_NAME} depends on ${dep}, but that module is not enabled for building")
        # Auto dependency building can be added here if needed
    endif()
endforeach()

# Automatically collect source files and headers
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

set(LIBS ${ATOM_ALGORITHM_DEPENDS})

# Add OpenSSL to the list of libraries
list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto TBB::tbb loguru)

# Build object library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build static library
add_library(${PROJECT_NAME} STATIC)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_object ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Add OpenSSL include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)