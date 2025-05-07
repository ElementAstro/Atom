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
        endif()
    endforeach()
    set(${return_var} "${module_name_r}" PARENT_SCOPE)
endfunction()

# ScanModule.cmake
# This script helps scan and process module dependencies
# When a module is enabled, its dependencies will be automatically enabled

# Function: Scan module dependencies and enable necessary modules
function(scan_module_dependencies)
    # Find all enabled modules
    set(enabled_modules)
    
    if(ATOM_BUILD_ERROR)
        list(APPEND enabled_modules "atom-error")
    endif()
    
    if(ATOM_BUILD_LOG)
        list(APPEND enabled_modules "atom-log")
    endif()
    
    if(ATOM_BUILD_ALGORITHM)
        list(APPEND enabled_modules "atom-algorithm")
    endif()
    
    if(ATOM_BUILD_ASYNC)
        list(APPEND enabled_modules "atom-async")
    endif()
    
    if(ATOM_BUILD_COMPONENTS)
        list(APPEND enabled_modules "atom-components")
    endif()
    
    if(ATOM_BUILD_CONNECTION)
        list(APPEND enabled_modules "atom-connection")
    endif()
    
    if(ATOM_BUILD_IO)
        list(APPEND enabled_modules "atom-io")
    endif()
    
    if(ATOM_BUILD_META)
        list(APPEND enabled_modules "atom-meta")
    endif()
    
    if(ATOM_BUILD_SEARCH)
        list(APPEND enabled_modules "atom-search")
    endif()
    
    if(ATOM_BUILD_SECRET)
        list(APPEND enabled_modules "atom-secret")
    endif()
    
    if(ATOM_BUILD_SYSINFO)
        list(APPEND enabled_modules "atom-sysinfo")
    endif()
    
    if(ATOM_BUILD_SYSTEM)
        list(APPEND enabled_modules "atom-system")
    endif()
    
    if(ATOM_BUILD_UTILS)
        list(APPEND enabled_modules "atom-utils")
    endif()
    
    if(ATOM_BUILD_WEB)
        list(APPEND enabled_modules "atom-web")
    endif()
    
    # Process dependencies until no new modules are added
    set(finished FALSE)
    while(NOT finished)
        set(finished TRUE)
        set(new_modules)
        
        foreach(module ${enabled_modules})
            string(REPLACE "-" "_" module_var ${module})
            string(TOUPPER ${module_var} module_var)
            
            # Check this module's dependencies
            if(DEFINED ATOM_${module_var}_DEPENDS)
                foreach(dep ${ATOM_${module_var}_DEPENDS})
                    if(NOT ${dep} IN_LIST enabled_modules AND NOT ${dep} IN_LIST new_modules)
                        list(APPEND new_modules ${dep})
                        set(finished FALSE)
                        
                        # Convert module name to option variable name
                        string(REPLACE "atom-" "ATOM_BUILD_" dep_option ${dep})
                        string(TOUPPER ${dep_option} dep_option)
                        
                        # Enable dependency module
                        set(${dep_option} ON PARENT_SCOPE)
                        message(STATUS "Automatically enabling dependency module: ${dep}")
                    endif()
                endforeach()
            endif()
        endforeach()
        
        # Add newly discovered dependency modules
        list(APPEND enabled_modules ${new_modules})
    endwhile()
endfunction()

# Call function to process dependencies
scan_module_dependencies()
