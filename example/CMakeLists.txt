cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(AtomExamples)

# 示例构建选项在 cmake/ExamplesBuildOptions.cmake 中定义

# 有条件地添加子目录
if(ATOM_EXAMPLE_BUILD_ALGORITHM)
    add_subdirectory(algorithm)
endif()

if(ATOM_EXAMPLE_BUILD_ASYNC)
    add_subdirectory(async)
endif()

if(ATOM_EXAMPLE_BUILD_COMPONENTS)
    add_subdirectory(components)
endif()

if(ATOM_EXAMPLE_BUILD_CONNECTION)
    add_subdirectory(connection)
endif()

if(ATOM_EXAMPLE_BUILD_ERROR)
    add_subdirectory(error)
endif()

if(ATOM_EXAMPLE_BUILD_EXTRA)
    add_subdirectory(extra)
endif()

if(ATOM_EXAMPLE_BUILD_IMAGE)
    add_subdirectory(image)
endif()

if(ATOM_EXAMPLE_BUILD_IO)
    add_subdirectory(io)
endif()

if(ATOM_EXAMPLE_BUILD_LOG)
    add_subdirectory(log)
endif()

if(ATOM_EXAMPLE_BUILD_MEMORY)
    add_subdirectory(memory)
endif()

if(ATOM_EXAMPLE_BUILD_META)
    add_subdirectory(meta)
endif()

if(ATOM_EXAMPLE_BUILD_SEARCH)
    add_subdirectory(search)
endif()

if(ATOM_EXAMPLE_BUILD_SERIAL)
    add_subdirectory(serial)
endif()

if(ATOM_EXAMPLE_BUILD_SYSTEM)
    add_subdirectory(system)
endif()

if(ATOM_EXAMPLE_BUILD_TYPE)
    add_subdirectory(type)
endif()

if(ATOM_EXAMPLE_BUILD_UTILS)
    add_subdirectory(utils)
endif()

if(ATOM_EXAMPLE_BUILD_WEB)
    add_subdirectory(web)
endif()

# Set example targets grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(EXAMPLE_GROUP_NAME "Examples")

# Recursively set all example targets to the example group
get_property(TARGETS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
foreach(TARGET ${TARGETS})
    set_property(TARGET ${TARGET} PROPERTY FOLDER ${EXAMPLE_GROUP_NAME})
endforeach()
