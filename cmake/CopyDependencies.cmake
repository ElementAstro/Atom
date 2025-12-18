# CopyDependencies.cmake
#
# This module provides functions to copy runtime dependencies (DLLs) to
# executable directories on Windows, ensuring tests and examples can run.
#
# Author: Max Qian License: GPL3

include_guard(GLOBAL)

# =============================================================================
# Global DLL tracking
# =============================================================================
set(ATOM_RUNTIME_DLLS
    ""
    CACHE INTERNAL "List of runtime DLLs to copy")

# =============================================================================
# Function: atom_find_runtime_dlls Description: Find all runtime DLLs from
# linked targets
# =============================================================================
function(atom_find_runtime_dlls TARGET_NAME OUTPUT_VAR)
  set(dll_list "")

  if(NOT WIN32)
    set(${OUTPUT_VAR}
        ""
        PARENT_SCOPE)
    return()
  endif()

  # Get linked libraries
  get_target_property(linked_libs ${TARGET_NAME} LINK_LIBRARIES)
  if(NOT linked_libs)
    set(${OUTPUT_VAR}
        ""
        PARENT_SCOPE)
    return()
  endif()

  foreach(lib ${linked_libs})
    if(TARGET ${lib})
      get_target_property(lib_type ${lib} TYPE)
      if(lib_type STREQUAL "SHARED_LIBRARY")
        list(APPEND dll_list "$<TARGET_FILE:${lib}>")
      elseif(lib_type STREQUAL "IMPORTED_LIBRARY" OR lib_type STREQUAL
                                                     "UNKNOWN_LIBRARY")
        get_target_property(lib_location ${lib} IMPORTED_LOCATION)
        if(lib_location AND lib_location MATCHES "\\.dll$")
          list(APPEND dll_list "${lib_location}")
        endif()
      endif()
    endif()
  endforeach()

  set(${OUTPUT_VAR}
      ${dll_list}
      PARENT_SCOPE)
endfunction()

# =============================================================================
# Function: atom_copy_runtime_dlls Description: Add post-build commands to copy
# DLLs to target directory
# =============================================================================
function(atom_copy_runtime_dlls TARGET_NAME)
  if(NOT WIN32)
    return()
  endif()

  # Parse arguments
  cmake_parse_arguments(COPY_DLLS "" "" "DLLS;TARGETS" ${ARGN})

  # Collect DLLs from specified targets
  set(all_dlls ${COPY_DLLS_DLLS})

  foreach(dep_target ${COPY_DLLS_TARGETS})
    if(TARGET ${dep_target})
      get_target_property(target_type ${dep_target} TYPE)
      if(target_type STREQUAL "SHARED_LIBRARY")
        list(APPEND all_dlls "$<TARGET_FILE:${dep_target}>")
      endif()
    endif()
  endforeach()

  # Add post-build commands for each DLL
  foreach(dll ${all_dlls})
    add_custom_command(
      TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}"
              "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
      COMMENT "Copying ${dll} to ${TARGET_NAME} directory"
      VERBATIM)
  endforeach()
endfunction()

# =============================================================================
# Function: atom_setup_runtime_dependencies Description: Setup runtime
# dependencies for a target (comprehensive)
# =============================================================================
function(atom_setup_runtime_dependencies TARGET_NAME)
  if(NOT WIN32)
    return()
  endif()

  # Parse arguments
  cmake_parse_arguments(SETUP "" "" "EXTRA_DLLS" ${ARGN})

  # List of atom library targets that might be shared
  set(ATOM_SHARED_TARGETS
      atom-algorithm
      atom-async
      atom-components
      atom-connection
      atom-containers
      atom-error
      atom-image
      atom-io
      atom-log
      atom-memory
      atom-meta
      atom-search
      atom-secret
      atom-serial
      atom-sysinfo
      atom-system
      atom-type
      atom-utils
      atom-web)

  # Collect all shared library DLLs
  set(dlls_to_copy "")

  foreach(atom_target ${ATOM_SHARED_TARGETS})
    if(TARGET ${atom_target})
      get_target_property(target_type ${atom_target} TYPE)
      if(target_type STREQUAL "SHARED_LIBRARY")
        list(APPEND dlls_to_copy "$<TARGET_FILE:${atom_target}>")
        add_dependencies(${TARGET_NAME} ${atom_target})
      endif()
    endif()
  endforeach()

  # Add third-party DLLs (spdlog, fmt, etc.)
  if(TARGET spdlog::spdlog)
    get_target_property(spdlog_type spdlog::spdlog TYPE)
    if(spdlog_type STREQUAL "SHARED_LIBRARY")
      list(APPEND dlls_to_copy "$<TARGET_FILE:spdlog::spdlog>")
    endif()
  endif()

  if(TARGET fmt::fmt)
    get_target_property(fmt_type fmt::fmt TYPE)
    if(fmt_type STREQUAL "SHARED_LIBRARY")
      list(APPEND dlls_to_copy "$<TARGET_FILE:fmt::fmt>")
    endif()
  endif()

  # Add TBB if available
  if(TARGET TBB::tbb)
    get_target_property(tbb_type TBB::tbb TYPE)
    if(tbb_type STREQUAL "SHARED_LIBRARY")
      list(APPEND dlls_to_copy "$<TARGET_FILE:TBB::tbb>")
    endif()
  endif()

  # Add GTest/GMock if available and shared
  if(TARGET GTest::gtest)
    get_target_property(gtest_type GTest::gtest TYPE)
    if(gtest_type STREQUAL "SHARED_LIBRARY")
      list(APPEND dlls_to_copy "$<TARGET_FILE:GTest::gtest>")
    endif()
  endif()

  if(TARGET GTest::gtest_main)
    get_target_property(gtest_main_type GTest::gtest_main TYPE)
    if(gtest_main_type STREQUAL "SHARED_LIBRARY")
      list(APPEND dlls_to_copy "$<TARGET_FILE:GTest::gtest_main>")
    endif()
  endif()

  if(TARGET GTest::gmock)
    get_target_property(gmock_type GTest::gmock TYPE)
    if(gmock_type STREQUAL "SHARED_LIBRARY")
      list(APPEND dlls_to_copy "$<TARGET_FILE:GTest::gmock>")
    endif()
  endif()

  # Add extra DLLs specified by caller
  list(APPEND dlls_to_copy ${SETUP_EXTRA_DLLS})

  # Copy all DLLs from targets
  foreach(dll ${dlls_to_copy})
    add_custom_command(
      TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}"
              "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
      COMMENT "Copying runtime dependency to ${TARGET_NAME} directory"
      VERBATIM)
  endforeach()

  # Copy MSYS2/MinGW runtime DLLs from system bin directory
  if(MINGW)
    set(MINGW_BIN_DIR "$ENV{MINGW_PREFIX}/bin")
    if(NOT MINGW_BIN_DIR OR NOT EXISTS "${MINGW_BIN_DIR}")
      set(MINGW_BIN_DIR "D:/msys64/mingw64/bin")
    endif()

    # Use glob patterns to find DLLs dynamically (avoids hardcoding version
    # numbers)
    set(MINGW_DLL_PATTERNS "libfmt*.dll" "libspdlog*.dll" "libtbb*.dll"
                           "libgtest*.dll" "libgmock*.dll")

    foreach(PATTERN ${MINGW_DLL_PATTERNS})
      file(GLOB FOUND_DLLS "${MINGW_BIN_DIR}/${PATTERN}")
      foreach(DLL_PATH ${FOUND_DLLS})
        get_filename_component(DLL_NAME "${DLL_PATH}" NAME)
        add_custom_command(
          TARGET ${TARGET_NAME}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL_PATH}"
                  "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
          COMMENT "Copying ${DLL_NAME} to ${TARGET_NAME} directory"
          VERBATIM)
      endforeach()
    endforeach()
  endif()
endfunction()

# =============================================================================
# Function: atom_copy_vcpkg_dlls Description: Copy vcpkg-installed DLLs to
# target directory
# =============================================================================
function(atom_copy_vcpkg_dlls TARGET_NAME)
  if(NOT WIN32)
    return()
  endif()

  if(NOT DEFINED VCPKG_INSTALLED_DIR)
    return()
  endif()

  # Determine vcpkg binary directory
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VCPKG_BIN_DIR
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
  else()
    set(VCPKG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
  endif()

  if(NOT EXISTS "${VCPKG_BIN_DIR}")
    return()
  endif()

  # Find all DLLs in vcpkg bin directory
  file(GLOB VCPKG_DLLS "${VCPKG_BIN_DIR}/*.dll")

  foreach(dll ${VCPKG_DLLS})
    add_custom_command(
      TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}"
              "$<TARGET_FILE_DIR:${TARGET_NAME}>/"
      COMMENT "Copying vcpkg DLL: ${dll}"
      VERBATIM)
  endforeach()
endfunction()

# =============================================================================
# Function: atom_create_dll_copy_target Description: Create a custom target to
# copy all DLLs to a specific directory
# =============================================================================
function(atom_create_dll_copy_target TARGET_NAME DESTINATION_DIR)
  if(NOT WIN32)
    return()
  endif()

  add_custom_target(
    ${TARGET_NAME} ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DESTINATION_DIR}"
    COMMENT "Creating DLL destination directory: ${DESTINATION_DIR}")

  # List of atom library targets
  set(ATOM_SHARED_TARGETS
      atom-algorithm
      atom-async
      atom-components
      atom-connection
      atom-containers
      atom-error
      atom-image
      atom-io
      atom-log
      atom-memory
      atom-meta
      atom-search
      atom-secret
      atom-serial
      atom-sysinfo
      atom-system
      atom-type
      atom-utils
      atom-web)

  foreach(atom_target ${ATOM_SHARED_TARGETS})
    if(TARGET ${atom_target})
      get_target_property(target_type ${atom_target} TYPE)
      if(target_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(
          TARGET ${TARGET_NAME}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "$<TARGET_FILE:${atom_target}>" "${DESTINATION_DIR}/"
          COMMENT "Copying ${atom_target} to ${DESTINATION_DIR}"
          VERBATIM)
        add_dependencies(${TARGET_NAME} ${atom_target})
      endif()
    endif()
  endforeach()

  # Copy third-party DLLs
  if(TARGET spdlog::spdlog)
    get_target_property(spdlog_type spdlog::spdlog TYPE)
    if(spdlog_type STREQUAL "SHARED_LIBRARY")
      add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:spdlog::spdlog>" "${DESTINATION_DIR}/"
        COMMENT "Copying spdlog to ${DESTINATION_DIR}"
        VERBATIM)
    endif()
  endif()

  if(TARGET fmt::fmt)
    get_target_property(fmt_type fmt::fmt TYPE)
    if(fmt_type STREQUAL "SHARED_LIBRARY")
      add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:fmt::fmt>"
                "${DESTINATION_DIR}/"
        COMMENT "Copying fmt to ${DESTINATION_DIR}"
        VERBATIM)
    endif()
  endif()
endfunction()

# =============================================================================
# Macro: atom_add_test_with_dlls Description: Add a test executable with
# automatic DLL copying
# =============================================================================
macro(atom_add_test_with_dlls TEST_NAME)
  cmake_parse_arguments(TEST "" "" "SOURCES;LIBS" ${ARGN})

  add_executable(${TEST_NAME} ${TEST_SOURCES})
  target_link_libraries(${TEST_NAME} PRIVATE ${TEST_LIBS} GTest::gtest
                                             GTest::gtest_main Threads::Threads)

  if(WIN32)
    atom_setup_runtime_dependencies(${TEST_NAME})
    atom_copy_vcpkg_dlls(${TEST_NAME})
  endif()

  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

# =============================================================================
# Macro: atom_add_example_with_dlls Description: Add an example executable with
# automatic DLL copying
# =============================================================================
macro(atom_add_example_with_dlls EXAMPLE_NAME)
  cmake_parse_arguments(EXAMPLE "" "" "SOURCES;LIBS" ${ARGN})

  add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCES})
  target_link_libraries(${EXAMPLE_NAME} PRIVATE ${EXAMPLE_LIBS})

  if(WIN32)
    atom_setup_runtime_dependencies(${EXAMPLE_NAME})
    atom_copy_vcpkg_dlls(${EXAMPLE_NAME})
  endif()
endmacro()

message(STATUS "CopyDependencies.cmake loaded - DLL copy functions available")
