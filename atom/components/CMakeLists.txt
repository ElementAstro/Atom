# CMakeLists.txt for Atom-Component This project adheres to the GPL3 license.
#
# Project Details: Name: Atom-Component Description: Central component library
# for the Atom framework Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-component
  VERSION 1.0.0
  LANGUAGES C CXX)

# Source files
set(SOURCES component.cpp dispatch.cpp registry.cpp var.cpp)

# Header files
set(HEADERS component.hpp dispatch.hpp types.hpp var.hpp)

# Dependencies
set(LIBS loguru atom-error atom-utils ${CMAKE_THREAD_LIBS_INIT})

# Include directories
include_directories(.)

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

# Build Shared Library
add_library(${PROJECT_NAME} SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Install rules
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
