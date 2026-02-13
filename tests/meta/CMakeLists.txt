# CMakeLists.txt for Atom Meta Tests (Custom Configuration) This project is
# licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Meta-Tests Description: Test Suite for Atom Meta Module
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-meta-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# =============================================================================
# Find Required Packages
# =============================================================================
find_package(GTest QUIET)
find_package(Threads REQUIRED)

if(NOT GTEST_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0)
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)
else()
  include(GoogleTest)
endif()

# =============================================================================
# Include Test Infrastructure (Optional for Custom Modules)
# =============================================================================
# Include the standardized test infrastructure if available
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
  add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests"
                   "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
endif()

# =============================================================================
# Verify Module Dependencies
# =============================================================================
# Ensure the target module is available
if(NOT TARGET atom-meta)
  message(
    FATAL_ERROR
      "atom-meta target not found. Make sure ATOM_BUILD_META is enabled.")
endif()

# =============================================================================
# Test Source Configuration (Custom Multi-Executable)
# =============================================================================
# Find all test source files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Exclude problematic test files on MinGW (template metaprogramming tests with
# macro issues)
if(MINGW OR MSYS)
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_any\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_anymeta\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_conversion\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_constructor\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_container_traits\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_template_traits\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_type_info\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/functional/test_invoke\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/functional/test_signature\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/functional/test_overload\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/interop/test_abi\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/interop/test_type_caster\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/proxy/test_vany\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/proxy/test_facade_any\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/proxy/test_facade_proxy\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/proxy/test_proxy\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/proxy/test_proxy_params\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX
       ".*/reflection/test_field_count\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_concept\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_decorate\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_enum\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_god\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_global_ptr\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/reflection/test_raw_name\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/reflection/test_member\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/reflection/test_refl\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/proxy/test_facade\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_property\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/utils/test_stepper\\.cpp$")
  list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/core/test_func_traits\\.cpp$")
  message(
    STATUS
      "MinGW/MSYS2 detected: Excluding some meta tests with template macro issues"
  )
endif()

# =============================================================================
# Create Test Executables
# =============================================================================
# Create separate executables for each test file
foreach(TEST_SOURCE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})

  # Add FFI library for FFI tests (Custom Configuration)
  if(${TEST_NAME} MATCHES ".*ffi.*")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFI REQUIRED libffi)

    target_link_libraries(
      ${TEST_NAME}
      PRIVATE # Test infrastructure (if available)
              $<TARGET_NAME_IF_EXISTS:atom-test-common>
              # Module under test and dependencies
              atom-meta
              atom-error
              atom-utils
              # GoogleTest
              gtest
              gtest_main
              gmock
              gmock_main
              # FFI libraries
              ${FFI_LIBRARIES}
              # System libraries
              Threads::Threads)
    target_include_directories(${TEST_NAME} PRIVATE ${FFI_INCLUDE_DIRS})
  else()
    target_link_libraries(
      ${TEST_NAME}
      PRIVATE # Test infrastructure (if available)
              $<TARGET_NAME_IF_EXISTS:atom-test-common>
              # Module under test and dependencies
              atom-meta
              atom-error
              atom-utils
              # GoogleTest
              gtest
              gtest_main
              gmock
              gmock_main
              # System libraries
              Threads::Threads)
    # Skip gtest discovery for FFI tests due to runtime dependency issues The
    # test can still be run manually
  endif()

  # Register tests with CTest (for non-FFI tests) - use simple registration
  if(NOT ${TEST_NAME} MATCHES ".*ffi.*")
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "meta" TIMEOUT 300)
  endif()

  # Copy DLLs on Windows
  if(WIN32)
    if(COMMAND copy_test_dlls)
      copy_test_dlls(${TEST_NAME})
    endif()

    add_custom_command(
      TARGET ${TEST_NAME}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${TEST_NAME}> $<TARGET_FILE_DIR:${TEST_NAME}>
      COMMAND_EXPAND_LISTS)
  endif()
endforeach()

# =============================================================================
# Custom Test Targets
# =============================================================================
# Create a meta target that depends on all test executables
add_custom_target(atom_meta_tests DEPENDS)
foreach(TEST_SOURCE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_dependencies(atom_meta_tests ${TEST_NAME})
endforeach()

# Add custom target for running only meta tests
add_custom_target(
  test_meta
  COMMAND ${CMAKE_CTEST_COMMAND} -L meta --output-on-failure
  DEPENDS atom_meta_tests
  COMMENT "Running meta module tests")
