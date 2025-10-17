# CMakeLists.txt for Atom Extra Components
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom Extra Components
# Description: Additional utility libraries and wrappers for the Atom framework
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-extra LANGUAGES CXX)

# Set C++ standard for all extra components
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include current directory for all targets
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Function to add subdirectories recursively
function(add_extra_subdirectories_recursively start_dir)
    file(GLOB entries "${start_dir}/*")
    foreach(entry ${entries})
        if(IS_DIRECTORY ${entry} AND EXISTS "${entry}/CMakeLists.txt")
            get_filename_component(dir_name ${entry} NAME)
            message(STATUS "Adding extra component subdirectory: ${dir_name}")
            add_subdirectory(${dir_name})
        endif()
    endforeach()
endfunction()

# Add all component subdirectories
add_extra_subdirectories_recursively(${CMAKE_CURRENT_SOURCE_DIR})

# Create a combined extra library (optional)
option(ATOM_BUILD_EXTRA_UNIFIED_LIBRARY "Build a unified extra library containing all extra components" OFF)

if(ATOM_BUILD_EXTRA_UNIFIED_LIBRARY)
    # Collect all extra targets
    set(EXTRA_TARGETS
        atom-extra-asio
        atom-extra-beast
        atom-extra-boost
        atom-extra-curl
        atom-extra-dotenv
        atom-extra-iconv
        atom-extra-inicpp
        atom-extra-injection
        atom-extra-pugixml
        atom-extra-spdlog
        atom-extra-uv
    )

    # Create unified library
    add_library(atom-extra-unified INTERFACE)

    # Link all extra components
    foreach(target ${EXTRA_TARGETS})
        if(TARGET ${target})
            target_link_libraries(atom-extra-unified INTERFACE ${target})
        endif()
    endforeach()

    # Set properties
    target_compile_features(atom-extra-unified INTERFACE cxx_std_20)

    message(STATUS "Created unified extra library: atom-extra-unified")
endif()
