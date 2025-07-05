# TestsBuildOptions.cmake
#
# This file contains all options for controlling the build of Atom tests
#
# Author: Max Qian
# License: GPL3

# If selective build mode is enabled, set all test modules to OFF by default
if(ATOM_BUILD_TESTS_SELECTIVE)
    set(DEFAULT_TEST_BUILD OFF)
else()
    set(DEFAULT_TEST_BUILD ON)
endif()

# Global test build option
option(ATOM_TEST_BUILD_ALL "Build all test modules" ${DEFAULT_TEST_BUILD})

# Submodule test build options
option(ATOM_TEST_BUILD_ALGORITHM "Build algorithm tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_ASYNC "Build async tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_COMPONENTS "Build components tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_CONNECTION "Build connection tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_EXTRA "Build extra tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_IMAGE "Build image tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_IO "Build IO tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_MEMORY "Build memory tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_META "Build meta tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_SEARCH "Build search tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_SECRET "Build secret tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_SERIAL "Build serial tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_SYSINFO "Build sysinfo tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_SYSTEM "Build system tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_TYPE "Build type tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_UTILS "Build utils tests" ${ATOM_TEST_BUILD_ALL})
option(ATOM_TEST_BUILD_WEB "Build web tests" ${ATOM_TEST_BUILD_ALL})
