# =============================================================================
# FindGTestFixed.cmake - Improved GTest detection for MSYS2/MinGW
# =============================================================================
# This module provides enhanced GTest detection that works across different
# build environments including MSYS2, MinGW, and standard CMake configurations.
#
# Author: Max Qian License: GPL3
# =============================================================================

# Avoid repeated inclusion
if(DEFINED FIND_GTEST_FIXED_INCLUDED)
  return()
endif()
set(FIND_GTEST_FIXED_INCLUDED TRUE)

# Skip if GTest is already configured
if(TARGET GTest::gtest)
  return()
endif()

find_package(GTest QUIET CONFIG)

if(NOT GTest_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GTEST QUIET gtest)
    pkg_check_modules(GTEST_MAIN QUIET gtest_main)
    pkg_check_modules(GMOCK QUIET gmock)

    if(GTEST_FOUND)
      if(NOT TARGET GTest::gtest)
        add_library(GTest::gtest INTERFACE IMPORTED)
        target_include_directories(GTest::gtest INTERFACE ${GTEST_INCLUDE_DIRS})
        target_link_libraries(GTest::gtest INTERFACE ${GTEST_LIBRARIES})
      endif()

      if(GTEST_MAIN_FOUND AND NOT TARGET GTest::gtest_main)
        add_library(GTest::gtest_main INTERFACE IMPORTED)
        target_include_directories(GTest::gtest_main
                                   INTERFACE ${GTEST_MAIN_INCLUDE_DIRS})
        target_link_libraries(GTest::gtest_main
                              INTERFACE ${GTEST_MAIN_LIBRARIES})
      endif()

      if(GMOCK_FOUND AND NOT TARGET GTest::gmock)
        add_library(GTest::gmock INTERFACE IMPORTED)
        target_include_directories(GTest::gmock INTERFACE ${GMOCK_INCLUDE_DIRS})
        target_link_libraries(GTest::gmock INTERFACE ${GMOCK_LIBRARIES})
      endif()

      set(GTest_FOUND
          TRUE
          PARENT_SCOPE)
      message(STATUS "GTest configured via pkg-config: ${GTEST_VERSION}")
    endif()
  endif()
endif()

if(GTest_FOUND OR TARGET GTest::gtest)

  # Provide backward-compatible alias targets for projects expecting
  # GTest::GTest and GTest::Main
  if(TARGET GTest::gtest AND NOT TARGET GTest::GTest)
    add_library(GTest::GTest ALIAS GTest::gtest)
  endif()
  if(TARGET GTest::gtest_main AND NOT TARGET GTest::Main)
    add_library(GTest::Main ALIAS GTest::gtest_main)
  endif()

  message(STATUS "GTest ready for testing")
else()
  message(WARNING "GTest not found")
endif()
