# CMakeLists.txt for Lithium Builtin Test
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium Test
# Description:  All tests for Lithium
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(lithium.test C CXX)

# Conditionally add test subdirectories based on build options

# Algorithm tests
if(ATOM_TEST_BUILD_ALGORITHM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/algorithm/CMakeLists.txt")
  add_subdirectory(algorithm)
endif()

# Async tests
if(ATOM_TEST_BUILD_ASYNC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/async/CMakeLists.txt")
  add_subdirectory(async)
endif()

# Components tests
if(ATOM_TEST_BUILD_COMPONENTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components/CMakeLists.txt")
  add_subdirectory(components)
endif()

# Connection tests
if(ATOM_TEST_BUILD_CONNECTION AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/connection/CMakeLists.txt")
  add_subdirectory(connection)
endif()

# Extra tests
if(ATOM_TEST_BUILD_EXTRA AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extra/CMakeLists.txt")
  add_subdirectory(extra)
endif()

# Image tests
if(ATOM_TEST_BUILD_IMAGE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/image/CMakeLists.txt")
  add_subdirectory(image)
endif()

# IO tests
if(ATOM_TEST_BUILD_IO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/io/CMakeLists.txt")
  add_subdirectory(io)
endif()

# Memory tests
if(ATOM_TEST_BUILD_MEMORY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/memory/CMakeLists.txt")
  add_subdirectory(memory)
endif()

# Meta tests
if(ATOM_TEST_BUILD_META AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/meta/CMakeLists.txt")
  add_subdirectory(meta)
endif()

# Search tests
if(ATOM_TEST_BUILD_SEARCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/search/CMakeLists.txt")
  add_subdirectory(search)
endif()

# Secret tests
if(ATOM_TEST_BUILD_SECRET AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/secret/CMakeLists.txt")
  add_subdirectory(secret)
endif()

# Serial tests
if(ATOM_TEST_BUILD_SERIAL AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/serial/CMakeLists.txt")
  add_subdirectory(serial)
endif()

# Sysinfo tests
if(ATOM_TEST_BUILD_SYSINFO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sysinfo/CMakeLists.txt")
  add_subdirectory(sysinfo)
endif()

# System tests
if(ATOM_TEST_BUILD_SYSTEM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/system/CMakeLists.txt")
  add_subdirectory(system)
endif()

# Type tests
if(ATOM_TEST_BUILD_TYPE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/type/CMakeLists.txt")
  add_subdirectory(type)
endif()

# Utils tests
if(ATOM_TEST_BUILD_UTILS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utils/CMakeLists.txt")
  add_subdirectory(utils)
endif()

# Web tests
if(ATOM_TEST_BUILD_WEB AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/web/CMakeLists.txt")
  add_subdirectory(web)
endif()

# Set test targets grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(TEST_GROUP_NAME "Tests")

# Recursively set all test targets to the test group
get_property(TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
foreach(TARGET ${TARGETS})
    set_property(TARGET ${TARGET} PROPERTY FOLDER ${TEST_GROUP_NAME})
endforeach()
