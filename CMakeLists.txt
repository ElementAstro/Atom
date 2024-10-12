# CMakeLists.txt for Atom
# This project is licensed under the terms of the GPL3 license.

# Description: The foundational library for all elemental astro projects
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(Atom LANGUAGES C CXX)

# Set policies
if(POLICY CMP0003)
    cmake_policy(SET CMP0003 NEW)
endif()
if(POLICY CMP0043)
    cmake_policy(SET CMP0043 NEW)
endif()

add_custom_target(CmakeAdditionalFiles
	SOURCES
	${lithium_src_dir}/../cmake/compiler_options.cmake)
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")
include(cmake/compiler_options.cmake)

# ------------------ CPM Begin ------------------

set(CPM_DOWNLOAD_VERSION 0.35.6)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

# ------------------ CPM End ------------------

# ------------------ Conan Begin ------------------

if (USE_CONAN)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake OPTIONAL) # Optional inclusion

# 检查是否已经安装Conan
find_program(CONAN_CMD conan)
if(NOT CONAN_CMD)
    message(FATAL_ERROR "Conan is not installed. Please install Conan (pip install conan).")
endif()

# 检测Conan默认配置文件是否存在
execute_process(
    COMMAND ${CONAN_CMD} config home
    OUTPUT_VARIABLE CONAN_HOME
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CONAN_DEFAULT_PROFILE "${CONAN_HOME}/profiles/default")

if(NOT EXISTS "${CONAN_DEFAULT_PROFILE}")
    message(STATUS "Conan default profile not found. Creating a new profile based on platform.")
    # 根据操作系统创建默认配置
    if(WIN32)
        execute_process(COMMAND ${CONAN_CMD} profile detect --force)
    elseif(UNIX)
        execute_process(COMMAND ${CONAN_CMD} profile detect --force)
    else()
        message(FATAL_ERROR "Unsupported platform for Conan profile detection.")
    endif()
endif()

# 如果conanbuildinfo.cmake不存在，执行conan install命令
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    message(STATUS "Running Conan install...")
    execute_process(
        COMMAND ${CONAN_CMD} install ${CMAKE_SOURCE_DIR} --build=missing
        RESULT_VARIABLE result
    )
    if(result)
        message(FATAL_ERROR "Conan install failed with error code: ${result}")
    endif()
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
endif()
endif()

# ------------------ Conan End ------------------

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(BUILD_SHARED_LIBS ON)

# Add subdirectories
add_subdirectory(example)
add_subdirectory(atom)
add_subdirectory(tests)

# Enable folder grouping in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
