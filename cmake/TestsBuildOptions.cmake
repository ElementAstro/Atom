# TestsBuildOptions.cmake
#
# This file contains all options for controlling the build of Atom tests Options
# are dynamically generated from ATOM_ALL_MODULES in
# ModuleDependenciesData.cmake
#
# Author: Max Qian License: GPL3

include_guard(GLOBAL)

# Include module dependencies data for ATOM_ALL_MODULES
include(${CMAKE_CURRENT_LIST_DIR}/ModuleDependenciesData.cmake)

# If selective build mode is enabled, set all test modules to OFF by default
if(ATOM_BUILD_TESTS_SELECTIVE)
  set(DEFAULT_TEST_BUILD OFF)
else()
  set(DEFAULT_TEST_BUILD ON)
endif()

# Global test build option
option(ATOM_TEST_BUILD_ALL "Build all test modules" ${DEFAULT_TEST_BUILD})

# Extra tests (not part of standard modules)
option(ATOM_TEST_BUILD_EXTRA "Build extra tests" ${ATOM_TEST_BUILD_ALL})

# Dynamically generate test build options from ATOM_ALL_MODULES
foreach(module ${ATOM_ALL_MODULES})
  # Convert module name (atom-xxx) to option name (ATOM_TEST_BUILD_XXX)
  string(REPLACE "atom-" "" module_name "${module}")
  string(TOUPPER "${module_name}" module_upper)

  option(ATOM_TEST_BUILD_${module_upper} "Build ${module_name} tests"
         ${ATOM_TEST_BUILD_ALL})
endforeach()

# Function to check if tests should be built for a module
function(atom_should_build_test module_name result_var)
  string(TOUPPER "${module_name}" module_upper)
  if(ATOM_TEST_BUILD_ALL OR ATOM_TEST_BUILD_${module_upper})
    set(${result_var}
        TRUE
        PARENT_SCOPE)
  else()
    set(${result_var}
        FALSE
        PARENT_SCOPE)
  endif()
endfunction()
