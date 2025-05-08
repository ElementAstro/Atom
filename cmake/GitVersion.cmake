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
                    set(VERSION_MAJOR 0)
                    set(VERSION_MINOR 0)
                    set(VERSION_PATCH 0)
                endif()
            else()
                # Use commit hash when there's no tag
                set(VERSION_STRING "0.0.0-${GIT_HASH}${GIT_DIRTY}")
                set(VERSION_MAJOR 0)
                set(VERSION_MINOR 0)
                set(VERSION_PATCH 0)
            endif()
            
            # Set variables in parent scope
            set(${ARG_VERSION_VARIABLE} "${VERSION_STRING}" PARENT_SCOPE)
            set(PROJECT_VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
            set(PROJECT_VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
            set(PROJECT_VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
            set(GIT_HASH ${GIT_HASH} PARENT_SCOPE)
            set(GIT_DIRTY_RESULT ${GIT_DIRTY_RESULT} PARENT_SCOPE)
            
            # Generate version header file
            string(TOUPPER "${ARG_PREFIX}" PREFIX_UPPER)
            
            # Configure using the existing version.h.in template
            set(VERSION_TEMPLATE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in")
            if(EXISTS "${VERSION_TEMPLATE_PATH}")
                configure_file(
                    "${VERSION_TEMPLATE_PATH}"
                    "${ARG_OUTPUT_HEADER}"
                    @ONLY
                )
                message(STATUS "Generated version header file from template: ${VERSION_TEMPLATE_PATH} -> ${ARG_OUTPUT_HEADER}")
            else()
                message(WARNING "Version template file not found: ${VERSION_TEMPLATE_PATH}")
                
                # Fall back to built-in template
                message(STATUS "Creating a default version header file at: ${ARG_OUTPUT_HEADER}")
                configure_file(
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/version.h.in"
                    "${ARG_OUTPUT_HEADER}"
                    @ONLY
                )
            endif()
            
            message(STATUS "Git version: ${VERSION_STRING} (Major: ${VERSION_MAJOR}, Minor: ${VERSION_MINOR}, Patch: ${VERSION_PATCH}, Hash: ${GIT_HASH})")
        else()
            message(WARNING "Current directory is not a Git repository, using default version")
            set(${ARG_VERSION_VARIABLE} "0.0.0-unknown" PARENT_SCOPE)
            set(PROJECT_VERSION_MAJOR 0 PARENT_SCOPE)
            set(PROJECT_VERSION_MINOR 0 PARENT_SCOPE)
            set(PROJECT_VERSION_PATCH 0 PARENT_SCOPE)
        endif()
    else()
        message(WARNING "Git not found, using default version")
        set(${ARG_VERSION_VARIABLE} "0.0.0-unknown" PARENT_SCOPE)
        set(PROJECT_VERSION_MAJOR 0 PARENT_SCOPE)
        set(PROJECT_VERSION_MINOR 0 PARENT_SCOPE)
        set(PROJECT_VERSION_PATCH 0 PARENT_SCOPE)
    endif()
endfunction()

# Function to configure both version files
function(configure_atom_version)
    # Parse arguments
    set(options "")
    set(oneValueArgs VERSION_VARIABLE)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT DEFINED ARG_VERSION_VARIABLE)
        set(ARG_VERSION_VARIABLE ATOM_VERSION)
    endif()

    # First generate the basic version header with Git info
    configure_version_from_git(
        OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/atom_version.h"
        VERSION_VARIABLE ${ARG_VERSION_VARIABLE}
        PREFIX "ATOM"
    )
    
    # Now generate the user-friendly version info header
    if(${ARG_VERSION_VARIABLE})
        set(PROJECT_VERSION ${${ARG_VERSION_VARIABLE}})
    endif()
    
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h"
        @ONLY
    )
    
    message(STATUS "Generated atom_version_info.h with version ${PROJECT_VERSION}")
endfunction()