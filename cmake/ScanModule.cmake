# Function to scan source files for module declarations and generate module list
# Parameters:
#   source_dir - Directory containing source files to scan
#   return_var - Variable name to store the result
function(scan_and_generate_modules source_dir return_var)
    set(modules_name_r "")
    file(GLOB_RECURSE CPP_FILES "${source_dir}/*.cpp")

    foreach(cpp_file ${CPP_FILES})
        file(READ ${cpp_file} file_content)
        string(REGEX MATCH "ATOM_MODULE\\(([a-zA-Z0-9_]+)," match ${file_content})

        if(match)
            string(REGEX REPLACE "ATOM_MODULE\\(([a-zA-Z0-9_]+),.*" "\\1" module_name ${match})

            if(NOT module_name)
                message(WARNING "Found ATOM_MODULE macro in ${cpp_file} but could not extract module name.")
                continue()
            endif()

            set(modules_name_r ${module_name})
            message(VERBOSE "Found module '${module_name}' in ${cpp_file}")
        endif()
    endforeach()

    set(${return_var} "${modules_name_r}" PARENT_SCOPE)
endfunction()

# ScanModule.cmake
# This script helps scan and process module dependencies
# When a module is enabled, its dependencies will be automatically enabled

# Function: Scan module dependencies and enable necessary modules
function(scan_module_dependencies)
    # Find all enabled modules
    set(enabled_modules)

    # Map build options to module names
    if(ATOM_BUILD_ERROR)
        list(APPEND enabled_modules "atom-error")
        message(STATUS "Module 'atom-error' is enabled")
    endif()

    if(ATOM_BUILD_LOG)
        list(APPEND enabled_modules "atom-log")
        message(STATUS "Module 'atom-log' is enabled")
    endif()

    if(ATOM_BUILD_ALGORITHM)
        list(APPEND enabled_modules "atom-algorithm")
        message(STATUS "Module 'atom-algorithm' is enabled")
    endif()

    if(ATOM_BUILD_ASYNC)
        list(APPEND enabled_modules "atom-async")
        message(STATUS "Module 'atom-async' is enabled")
    endif()

    if(ATOM_BUILD_COMPONENTS)
        list(APPEND enabled_modules "atom-components")
        message(STATUS "Module 'atom-components' is enabled")
    endif()

    if(ATOM_BUILD_CONNECTION)
        list(APPEND enabled_modules "atom-connection")
        message(STATUS "Module 'atom-connection' is enabled")
    endif()

    if(ATOM_BUILD_CONTAINERS)
        list(APPEND enabled_modules "atom-containers")
        message(STATUS "Module 'atom-containers' is enabled")
    endif()

    if(ATOM_BUILD_IO)
        list(APPEND enabled_modules "atom-io")
        message(STATUS "Module 'atom-io' is enabled")
    endif()

    if(ATOM_BUILD_META)
        list(APPEND enabled_modules "atom-meta")
        message(STATUS "Module 'atom-meta' is enabled")
    endif()

    if(ATOM_BUILD_MEMORY)
        list(APPEND enabled_modules "atom-memory")
        message(STATUS "Module 'atom-memory' is enabled")
    endif()

    if(ATOM_BUILD_SEARCH)
        list(APPEND enabled_modules "atom-search")
        message(STATUS "Module 'atom-search' is enabled")
    endif()

    if(ATOM_BUILD_SECRET)
        list(APPEND enabled_modules "atom-secret")
        message(STATUS "Module 'atom-secret' is enabled")
    endif()

    if(ATOM_BUILD_SERIAL)
        list(APPEND enabled_modules "atom-serial")
        message(STATUS "Module 'atom-serial' is enabled")
    endif()

    if(ATOM_BUILD_SYSINFO)
        list(APPEND enabled_modules "atom-sysinfo")
        message(STATUS "Module 'atom-sysinfo' is enabled")
    endif()

    if(ATOM_BUILD_SYSTEM)
        list(APPEND enabled_modules "atom-system")
        message(STATUS "Module 'atom-system' is enabled")
    endif()

    if(ATOM_BUILD_TYPE)
        list(APPEND enabled_modules "atom-type")
        message(STATUS "Module 'atom-type' is enabled")
    endif()

    if(ATOM_BUILD_UTILS)
        list(APPEND enabled_modules "atom-utils")
        message(STATUS "Module 'atom-utils' is enabled")
    endif()

    if(ATOM_BUILD_WEB)
        list(APPEND enabled_modules "atom-web")
        message(STATUS "Module 'atom-web' is enabled")
    endif()

    # Store the enabled modules in a global property for later use
    set_property(GLOBAL PROPERTY ATOM_ENABLED_MODULES "${enabled_modules}")
    message(STATUS "Initial enabled modules: ${enabled_modules}")
endfunction()

# Function to check if a module exists (has a valid directory with CMakeLists.txt)
function(module_exists module_name result_var)
    # Convert module name (e.g., "atom-error") to directory name (e.g., "error")
    string(REPLACE "atom-" "" dir_name "${module_name}")

    # Check if directory exists and has a CMakeLists.txt file
    set(module_path "${CMAKE_CURRENT_SOURCE_DIR}/../atom/${dir_name}")
    if(EXISTS "${module_path}" AND EXISTS "${module_path}/CMakeLists.txt")
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function: Process module dependencies to ensure all required modules are enabled
function(process_module_dependencies)
    # Get list of initially enabled modules
    get_property(enabled_modules GLOBAL PROPERTY ATOM_ENABLED_MODULES)

    # Create a copy of the initial list
    set(initial_modules ${enabled_modules})

    # Validate initial modules - remove any that don't exist
    set(validated_modules "")
    foreach(module ${enabled_modules})
        module_exists(${module} MODULE_EXISTS)
        if(MODULE_EXISTS)
            list(APPEND validated_modules ${module})
        else()
            string(REPLACE "atom-" "" module_name "${module}")
            message(STATUS "Warning: Module '${module}' is enabled but its directory is missing or invalid. Disabling it.")
            # Disable the build option for this module
            string(TOUPPER "${module_name}" module_upper)
            set(ATOM_BUILD_${module_upper} OFF CACHE BOOL "Build ${module} module" FORCE)
        endif()
    endforeach()

    # Update the enabled modules list with only valid ones
    set(enabled_modules ${validated_modules})
    set_property(GLOBAL PROPERTY ATOM_ENABLED_MODULES "${enabled_modules}")

    # Process dependencies until no new modules are added
    set(process_again TRUE)
    set(iteration 0)
    set(max_iterations 10)  # Prevent infinite loops

    while(process_again AND iteration LESS max_iterations)
        set(process_again FALSE)
        set(new_modules "")

        # For each enabled module, check its dependencies
        foreach(module ${enabled_modules})
            # Convert module name to uppercase for variable lookup
            string(TOUPPER "${module}" module_upper)
            string(REPLACE "-" "_" module_var "${module_upper}")

            # Get dependencies for this module
            if(DEFINED ATOM_${module_var}_DEPENDS)
                foreach(dep ${ATOM_${module_var}_DEPENDS})
                    # Check if dependency exists before adding it
                    module_exists(${dep} DEP_EXISTS)

                    # If the dependency is not already in the enabled list and it exists, add it
                    if(NOT "${dep}" IN_LIST enabled_modules AND DEP_EXISTS)
                        list(APPEND new_modules ${dep})
                        set(process_again TRUE)
                        message(STATUS "Adding dependency '${dep}' required by module '${module}'")
                    elseif(NOT DEP_EXISTS)
                        string(REPLACE "atom-" "" dep_name "${dep}")
                        message(WARNING "Module '${module}' depends on '${dep}', but the dependency directory is missing or invalid.")
                        message(STATUS "Consider implementing the '${dep_name}' module or removing the dependency.")
                    endif()
                endforeach()
            endif()
        endforeach()

        # Add newly discovered dependencies to the enabled list
        if(new_modules)
            list(APPEND enabled_modules ${new_modules})
            list(REMOVE_DUPLICATES enabled_modules)
        endif()

        math(EXPR iteration "${iteration} + 1")
    endwhile()

    # Check if we reached max iterations
    if(iteration EQUAL max_iterations)
        message(WARNING "Reached maximum dependency resolution iterations. There may be circular dependencies.")
    endif()

    # Find any new modules that were added because of dependencies
    set(added_modules "")
    foreach(module ${enabled_modules})
        if(NOT "${module}" IN_LIST initial_modules)
            list(APPEND added_modules ${module})
        endif()
    endforeach()

    if(added_modules)
        message(STATUS "Additional modules enabled due to dependencies: ${added_modules}")
    endif()

    # Update the global property with the full list of modules to build
    set_property(GLOBAL PROPERTY ATOM_ENABLED_MODULES "${enabled_modules}")
    message(STATUS "Final list of enabled modules: ${enabled_modules}")

    # Create a property to hold all module targets
    set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "")

    # Enable the build for each required module
    foreach(module ${enabled_modules})
        # Convert module name to CMake variable format
        string(REPLACE "atom-" "" module_name "${module}")
        string(TOUPPER "${module_name}" module_upper)

        # Set the corresponding build option to ON
        set(ATOM_BUILD_${module_upper} ON CACHE BOOL "Build ${module} module" FORCE)
    endforeach()
endfunction()

# Call function to process dependencies
scan_module_dependencies()
