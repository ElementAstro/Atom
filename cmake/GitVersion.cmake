# GitVersion.cmake
function(configure_version_from_git)
    # Parse arguments
    set(options "")
    set(oneValueArgs OUTPUT_HEADER VERSION_VARIABLE PREFIX)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Set default values
    if(NOT DEFINED ARG_OUTPUT_HEADER)
        set(ARG_OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")
    endif()
    
    if(NOT DEFINED ARG_VERSION_VARIABLE)
        set(ARG_VERSION_VARIABLE PROJECT_VERSION)
    endif()
    
    if(NOT DEFINED ARG_PREFIX)
        set(ARG_PREFIX "${PROJECT_NAME}")
    endif()
    
    # Get Git information
    find_package(Git QUIET)
    if(GIT_FOUND)
        # Check if in a Git repository
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_REPO_CHECK
            OUTPUT_QUIET
            ERROR_QUIET
        )
        
        if(GIT_REPO_CHECK EQUAL 0)
            # Get the most recent tag
            execute_process(
                COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_TAG_RESULT
                OUTPUT_VARIABLE GIT_TAG
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Get the current commit short hash
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_HASH_RESULT
                OUTPUT_VARIABLE GIT_HASH
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Get the number of commits since the most recent tag
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-list --count ${GIT_TAG}..HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_COUNT_RESULT
                OUTPUT_VARIABLE GIT_COUNT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            # Check if the working directory is clean
            execute_process(
                COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_DIRTY_RESULT
            )
            
            if(NOT GIT_DIRTY_RESULT EQUAL 0)
                set(GIT_DIRTY "-dirty")
            else()
                set(GIT_DIRTY "")
            endif()
            
            # Build version string
            if(GIT_TAG_RESULT EQUAL 0)
                # Parse tag version number (assuming format vX.Y.Z or X.Y.Z)
                string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)$" VERSION_MATCH ${GIT_TAG})
                if(VERSION_MATCH)
                    set(VERSION_MAJOR ${CMAKE_MATCH_1})
                    set(VERSION_MINOR ${CMAKE_MATCH_2})
                    set(VERSION_PATCH ${CMAKE_MATCH_3})
                    
                    # If there are additional commits, increment the patch version
                    if(GIT_COUNT GREATER 0)
                        math(EXPR VERSION_PATCH "${VERSION_PATCH}+${GIT_COUNT}")
                    endif()
                    
                    set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${GIT_DIRTY}")
                else()
                    set(VERSION_STRING "${GIT_TAG}-${GIT_HASH}${GIT_DIRTY}")
                endif()
            else()
                # Use commit hash when there's no tag
                set(VERSION_STRING "0.0.0-${GIT_HASH}${GIT_DIRTY}")
            endif()
            
            # Set variables
            set(${ARG_VERSION_VARIABLE} "${VERSION_STRING}" PARENT_SCOPE)
            
            # Generate version header file
            string(TOUPPER "${ARG_PREFIX}" PREFIX_UPPER)
            
            # Check if version.h.in exists, create it if it doesn't
            set(VERSION_TEMPLATE_PATH "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/version.h.in")
            if(NOT EXISTS "${VERSION_TEMPLATE_PATH}")
                message(STATUS "Creating version.h.in template file")
                # Create the template file with English comments
                file(WRITE "${VERSION_TEMPLATE_PATH}" [=[
// Auto-generated version header file - Do not modify manually
#ifndef @PREFIX_UPPER@_VERSION_H
#define @PREFIX_UPPER@_VERSION_H

#define @PREFIX_UPPER@_VERSION "@VERSION_STRING@"
#define @PREFIX_UPPER@_VERSION_MAJOR @VERSION_MAJOR@
#define @PREFIX_UPPER@_VERSION_MINOR @VERSION_MINOR@
#define @PREFIX_UPPER@_VERSION_PATCH @VERSION_PATCH@
#define @PREFIX_UPPER@_VERSION_HASH "@GIT_HASH@"
#define @PREFIX_UPPER@_VERSION_DIRTY @GIT_DIRTY_RESULT@

#endif // @PREFIX_UPPER@_VERSION_H
]=])
            endif()
            
            configure_file(
                "${VERSION_TEMPLATE_PATH}"
                "${ARG_OUTPUT_HEADER}"
                @ONLY
            )
            
            message(STATUS "Git version: ${VERSION_STRING}")
        else()
            message(WARNING "Current directory is not a Git repository, using default version")
            set(${ARG_VERSION_VARIABLE} "0.0.0-unknown" PARENT_SCOPE)
        endif()
    else()
        message(WARNING "Git not found, using default version")
        set(${ARG_VERSION_VARIABLE} "0.0.0-unknown" PARENT_SCOPE)
    endif()
endfunction()