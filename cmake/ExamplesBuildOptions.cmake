# ExamplesBuildOptions.cmake
#
# This file contains all options for controlling the build of Atom examples
# Options are dynamically generated from ATOM_ALL_MODULES in
# ModuleDependenciesData.cmake
#
# Author: Max Qian License: GPL3

include_guard(GLOBAL)

# Include module dependencies data for ATOM_ALL_MODULES
include(${CMAKE_CURRENT_LIST_DIR}/ModuleDependenciesData.cmake)

# Disable example modules by default to keep CI focused on core library/tests.
set(DEFAULT_EXAMPLE_BUILD OFF)

# Global example build option
option(ATOM_EXAMPLE_BUILD_ALL "Build all example modules"
       ${DEFAULT_EXAMPLE_BUILD})

# Extra examples (not part of standard modules)
option(ATOM_EXAMPLE_BUILD_EXTRA "Build extra examples"
       ${ATOM_EXAMPLE_BUILD_ALL})

# Dynamically generate example build options from ATOM_ALL_MODULES
foreach(module ${ATOM_ALL_MODULES})
  # Convert module name (atom-xxx) to option name (ATOM_EXAMPLE_BUILD_XXX)
  string(REPLACE "atom-" "" module_name "${module}")
  string(TOUPPER "${module_name}" module_upper)

  option(ATOM_EXAMPLE_BUILD_${module_upper} "Build ${module_name} examples"
         ${ATOM_EXAMPLE_BUILD_ALL})
endforeach()

# Function to check if examples should be built for a module
function(atom_should_build_example module_name result_var)
  string(TOUPPER "${module_name}" module_upper)
  if(ATOM_EXAMPLE_BUILD_ALL OR ATOM_EXAMPLE_BUILD_${module_upper})
    set(${result_var}
        TRUE
        PARENT_SCOPE)
  else()
    set(${result_var}
        FALSE
        PARENT_SCOPE)
  endif()
endfunction()
