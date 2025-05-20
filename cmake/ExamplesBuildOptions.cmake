# ExamplesBuildOptions.cmake
#
# This file contains all options for controlling the build of Atom examples
# 
# Author: Max Qian
# License: GPL3

# If selective build mode is enabled, set all example modules to OFF by default
if(ATOM_BUILD_EXAMPLES_SELECTIVE)
    set(DEFAULT_EXAMPLE_BUILD OFF)
else()
    set(DEFAULT_EXAMPLE_BUILD ON)
endif()

# Global example build option
option(ATOM_EXAMPLE_BUILD_ALL "Build all example modules" ${DEFAULT_EXAMPLE_BUILD})

# Submodule example build options
option(ATOM_EXAMPLE_BUILD_ALGORITHM "Build algorithm examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_ASYNC "Build async examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_COMPONENTS "Build components examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_CONNECTION "Build connection examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_ERROR "Build error examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_EXTRA "Build extra examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_IMAGE "Build image examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_IO "Build IO examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_LOG "Build log examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_MEMORY "Build memory examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_META "Build meta examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_SEARCH "Build search examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_SERIAL "Build serial examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_SYSTEM "Build system examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_TYPE "Build type examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_UTILS "Build utils examples" ${ATOM_EXAMPLE_BUILD_ALL})
option(ATOM_EXAMPLE_BUILD_WEB "Build web examples" ${ATOM_EXAMPLE_BUILD_ALL})
