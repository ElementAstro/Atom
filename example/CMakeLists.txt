cmake_minimum_required(VERSION 3.21)

# 设置项目名称
project(AtomExamples)

# Set C++ standard (inherit from top-level if set, otherwise use 23)
if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile options
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Include example build options if not already included
if(NOT DEFINED ATOM_EXAMPLE_BUILD_ALL)
  include(${CMAKE_SOURCE_DIR}/cmake/ExamplesBuildOptions.cmake)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# 有条件地添加子目录
if(ATOM_EXAMPLE_BUILD_ALGORITHM
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/algorithm/CMakeLists.txt")
  add_subdirectory(algorithm)
endif()

if(ATOM_EXAMPLE_BUILD_ASYNC
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/async/CMakeLists.txt")
  add_subdirectory(async)
endif()

if(ATOM_EXAMPLE_BUILD_COMPONENTS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components/CMakeLists.txt")
  add_subdirectory(components)
endif()

if(ATOM_EXAMPLE_BUILD_CONNECTION
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/connection/CMakeLists.txt")
  add_subdirectory(connection)
endif()

if(ATOM_EXAMPLE_BUILD_CONTAINERS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/containers/CMakeLists.txt")
  add_subdirectory(containers)
endif()

if(ATOM_EXAMPLE_BUILD_ERROR
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/error/CMakeLists.txt")
  add_subdirectory(error)
endif()

if(ATOM_EXAMPLE_BUILD_EXTRA
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extra/CMakeLists.txt")
  add_subdirectory(extra)
endif()

if(ATOM_EXAMPLE_BUILD_IMAGE
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/image/CMakeLists.txt")
  add_subdirectory(image)
endif()

if(ATOM_EXAMPLE_BUILD_IO AND EXISTS
                             "${CMAKE_CURRENT_SOURCE_DIR}/io/CMakeLists.txt")
  add_subdirectory(io)
endif()

if(ATOM_EXAMPLE_BUILD_LOG AND EXISTS
                              "${CMAKE_CURRENT_SOURCE_DIR}/log/CMakeLists.txt")
  add_subdirectory(log)
endif()

if(ATOM_EXAMPLE_BUILD_MEMORY
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/memory/CMakeLists.txt")
  add_subdirectory(memory)
endif()

if(ATOM_EXAMPLE_BUILD_META
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/meta/CMakeLists.txt")
  add_subdirectory(meta)
endif()

if(ATOM_EXAMPLE_BUILD_SEARCH
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/search/CMakeLists.txt")
  add_subdirectory(search)
endif()

if(ATOM_EXAMPLE_BUILD_SECRET
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/secret/CMakeLists.txt")
  add_subdirectory(secret)
endif()

if(ATOM_EXAMPLE_BUILD_SERIAL
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/serial/CMakeLists.txt")
  add_subdirectory(serial)
endif()

if(ATOM_EXAMPLE_BUILD_SYSINFO
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sysinfo/CMakeLists.txt")
  add_subdirectory(sysinfo)
endif()

if(ATOM_EXAMPLE_BUILD_SYSTEM
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/system/CMakeLists.txt")
  add_subdirectory(system)
endif()

if(ATOM_EXAMPLE_BUILD_TYPE
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/type/CMakeLists.txt")
  add_subdirectory(type)
endif()

if(ATOM_EXAMPLE_BUILD_UTILS
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utils/CMakeLists.txt")
  add_subdirectory(utils)
endif()

if(ATOM_EXAMPLE_BUILD_WEB AND EXISTS
                              "${CMAKE_CURRENT_SOURCE_DIR}/web/CMakeLists.txt")
  add_subdirectory(web)
endif()

# Set example targets grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(EXAMPLE_GROUP_NAME "Examples")

# Recursively set all example targets to the example group
get_property(
  TARGETS
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY BUILDSYSTEM_TARGETS)
foreach(TARGET ${TARGETS})
  set_property(TARGET ${TARGET} PROPERTY FOLDER ${EXAMPLE_GROUP_NAME})
endforeach()
