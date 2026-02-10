# =============================================================================
# Standardized Test Template for Atom Modules This file provides consistent test
# configuration across all Atom modules Usage: include(StandardTestTemplate) at
# the end of each module's test CMakeLists.txt
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# =============================================================================
# Standard Test Configuration Function
# =============================================================================
function(configure_standard_module_tests MODULE_NAME TEST_SOURCES)
  # Parse arguments
  set(options HEADER_ONLY USE_GMOCK)
  set(oneValueArgs TEST_TARGET_NAME MODULE_DEPENDENCIES)
  set(multiValueArgs EXCLUDE_SOURCES EXTRA_LIBRARIES EXTRA_INCLUDES)
  cmake_parse_arguments(STANDARD_TEST "${options}" "${oneValueArgs}"
                        "${multiValueArgs}" ${ARGN})

  # Set defaults
  if(NOT STANDARD_TEST_TEST_TARGET_NAME)
    set(STANDARD_TEST_TEST_TARGET_NAME "atom_${MODULE_NAME}_tests")
  endif()

  # Filter excluded sources
  if(STANDARD_TEST_EXCLUDE_SOURCES)
    list(REMOVE_ITEM TEST_SOURCES ${STANDARD_TEST_EXCLUDE_SOURCES})
  endif()

  # =============================================================================
  # Include Test Infrastructure
  # =============================================================================
  if(EXISTS "${CMAKE_SOURCE_DIR}/tests/tests/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/tests/tests"
                     "${CMAKE_CURRENT_BINARY_DIR}/test_infrastructure")
  endif()

  # =============================================================================
  # Verify Module Dependencies
  # =============================================================================
  set(MODULE_TARGET "atom-${MODULE_NAME}")
  if(NOT TARGET ${MODULE_TARGET})
    message(
      FATAL_ERROR
        "${MODULE_TARGET} target not found. Make sure ATOM_BUILD_${MODULE_NAME_UPPER} is enabled."
    )
  endif()

  # =============================================================================
  # Test Source Configuration for Header-Only Modules
  # =============================================================================
  if(STANDARD_TEST_HEADER_ONLY)
    # Find all test header files
    file(GLOB_RECURSE TEST_HEADERS ${PROJECT_SOURCE_DIR}/*.hpp)

    # Filter excluded headers
    if(STANDARD_TEST_EXCLUDE_SOURCES)
      foreach(EXCLUDE_PATTERN ${STANDARD_TEST_EXCLUDE_SOURCES})
        list(FILTER TEST_HEADERS EXCLUDE REGEX "${EXCLUDE_PATTERN}")
      endforeach()
    endif()

    # Create test runner source file
    set(TEST_RUNNER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/test_${MODULE_NAME}_runner.cpp")

    # Generate test runner content
    set(TEST_INCLUDES "")
    foreach(HEADER ${TEST_HEADERS})
      file(RELATIVE_PATH REL_HEADER ${PROJECT_SOURCE_DIR} ${HEADER})
      string(APPEND TEST_INCLUDES "#include \"${REL_HEADER}\"\n")
    endforeach()

    # Create a basic test runner that includes both .cpp and .hpp tests
    file(
      WRITE "${TEST_RUNNER_SOURCE}"
      "// Auto-generated test runner for ${PROJECT_NAME}
#include <gtest/gtest.h>

// Include all test headers
${TEST_INCLUDES}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
")

    # Combine all test sources
    list(APPEND TEST_SOURCES ${TEST_RUNNER_SOURCE})
  endif()

  # =============================================================================
  # Create Test Executable
  # =============================================================================
  add_executable(${STANDARD_TEST_TEST_TARGET_NAME} ${TEST_SOURCES})

  # =============================================================================
  # Link Libraries
  # =============================================================================
  # Core dependencies Find spdlog for tests that use it
  find_package(spdlog QUIET)

  # Note: atom-error is not directly linked here to avoid ODR violations Most
  # module targets already link atom-error internally
  set(CORE_LIBS atom-test-common ${MODULE_TARGET} Threads::Threads GTest::gtest
                GTest::gtest_main)

  # Add spdlog if found
  if(spdlog_FOUND)
    list(APPEND CORE_LIBS spdlog::spdlog)
  endif()

  # Add GoogleMock if requested and available
  if(STANDARD_TEST_USE_GMOCK AND TARGET GTest::gmock)
    list(APPEND CORE_LIBS GTest::gmock)
  endif()

  # Add module-specific dependencies if provided
  if(STANDARD_TEST_MODULE_DEPENDENCIES)
    list(APPEND CORE_LIBS ${STANDARD_TEST_MODULE_DEPENDENCIES})
  endif()

  # Add extra libraries if provided
  if(STANDARD_TEST_EXTRA_LIBRARIES)
    list(APPEND CORE_LIBS ${STANDARD_TEST_EXTRA_LIBRARIES})
  endif()

  target_link_libraries(${STANDARD_TEST_TEST_TARGET_NAME} PRIVATE ${CORE_LIBS})

  # =============================================================================
  # Include Directories
  # =============================================================================
  set(STANDARD_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/atom
                        ${CMAKE_SOURCE_DIR}/tests)

  if(STANDARD_TEST_EXTRA_INCLUDES)
    list(APPEND STANDARD_INCLUDES ${STANDARD_TEST_EXTRA_INCLUDES})
  endif()

  target_include_directories(${STANDARD_TEST_TEST_TARGET_NAME}
                             PRIVATE ${STANDARD_INCLUDES})

  # =============================================================================
  # Compiler Configuration
  # =============================================================================
  target_compile_features(${STANDARD_TEST_TEST_TARGET_NAME} PRIVATE cxx_std_20)

  # Use spdlog as compiled library (not header-only) to avoid ODR violations
  target_compile_definitions(${STANDARD_TEST_TEST_TARGET_NAME}
                             PRIVATE SPDLOG_COMPILED_LIB SPDLOG_FMT_EXTERNAL)

  if(MSVC)
    target_compile_options(${STANDARD_TEST_TEST_TARGET_NAME} PRIVATE /W4)
  else()
    target_compile_options(${STANDARD_TEST_TEST_TARGET_NAME}
                           PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  # =============================================================================
  # Platform-Specific Libraries
  # =============================================================================
  if(WIN32)
    target_link_libraries(${STANDARD_TEST_TEST_TARGET_NAME} PRIVATE ws2_32
                                                                    wsock32)
  endif()

  # Copy all dependent runtime DLLs next to the test executable on Windows
  if(WIN32)
    # Use CMake's built-in runtime DLL copying (CMake 3.21+)
    add_custom_command(
      TARGET ${STANDARD_TEST_TEST_TARGET_NAME}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${STANDARD_TEST_TEST_TARGET_NAME}>
        $<TARGET_FILE_DIR:${STANDARD_TEST_TEST_TARGET_NAME}>
      COMMAND_EXPAND_LISTS)

    # Also use copy_test_dlls function if available for comprehensive coverage
    if(COMMAND copy_test_dlls)
      copy_test_dlls(${STANDARD_TEST_TEST_TARGET_NAME})
    endif()
  endif()

  # =============================================================================
  # Test Registration with CTest
  # =============================================================================
  enable_testing()

  # Register tests directly to avoid executing binaries during discovery
  add_test(
    NAME ${STANDARD_TEST_TEST_TARGET_NAME}
    COMMAND ${STANDARD_TEST_TEST_TARGET_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(${STANDARD_TEST_TEST_TARGET_NAME}
                       PROPERTIES LABELS "${MODULE_NAME}" TIMEOUT 300)

  # =============================================================================
  # Custom Test Targets
  # =============================================================================
  # Add custom target for running only this module's tests
  add_custom_target(
    test_${MODULE_NAME}
    COMMAND ${CMAKE_CTEST_COMMAND} -L ${MODULE_NAME} --output-on-failure
    DEPENDS ${STANDARD_TEST_TEST_TARGET_NAME}
    COMMENT "Running ${MODULE_NAME} module tests")

  # Add performance test target
  add_custom_target(
    test_${MODULE_NAME}_performance
    COMMAND ${STANDARD_TEST_TEST_TARGET_NAME}
            --gtest_filter="*Performance*:*Benchmark*"
    DEPENDS ${STANDARD_TEST_TEST_TARGET_NAME}
    COMMENT "Running ${MODULE_NAME} performance tests")

  # =============================================================================
  # IDE Organization
  # =============================================================================
  set_property(TARGET ${STANDARD_TEST_TEST_TARGET_NAME}
               PROPERTY FOLDER "Tests/${MODULE_NAME}")
  set_property(TARGET test_${MODULE_NAME} PROPERTY FOLDER
                                                   "Tests/${MODULE_NAME}")
  set_property(TARGET test_${MODULE_NAME}_performance
               PROPERTY FOLDER "Tests/${MODULE_NAME}")

  # =============================================================================
  # Code Coverage (Optional) - Disabled on Windows due to executable size issues
  # =============================================================================
  if(CMAKE_BUILD_TYPE STREQUAL "Debug"
     AND CMAKE_COMPILER_IS_GNUCXX
     AND NOT WIN32)
    target_compile_options(${STANDARD_TEST_TEST_TARGET_NAME} PRIVATE --coverage)
    target_link_options(${STANDARD_TEST_TEST_TARGET_NAME} PRIVATE --coverage)

    add_custom_target(
      ${MODULE_NAME}_coverage
      COMMAND lcov --directory . --capture --output-file
              ${MODULE_NAME}_coverage.info
      COMMAND lcov --remove ${MODULE_NAME}_coverage.info '/usr/*' --output-file
              ${MODULE_NAME}_coverage.info
      COMMAND lcov --list ${MODULE_NAME}_coverage.info
      COMMAND genhtml -o ${MODULE_NAME}_coverage_html
              ${MODULE_NAME}_coverage.info
      DEPENDS test_${MODULE_NAME}
      COMMENT "Generating code coverage report for ${MODULE_NAME}")
  endif()

  # =============================================================================
  # Status Message
  # =============================================================================
  message(STATUS "${MODULE_NAME} tests configured successfully")
endfunction()

# =============================================================================
# Utility function for common test patterns
# =============================================================================
function(add_standard_test MODULE_NAME TEST_SOURCE_FILE)
  configure_standard_module_tests(${MODULE_NAME} ${TEST_SOURCE_FILE})
endfunction()

# =============================================================================
# Helper function to find test sources automatically
# =============================================================================
function(find_module_test_sources MODULE_NAME OUTPUT_VAR)
  file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
  set(${OUTPUT_VAR}
      ${TEST_SOURCES}
      PARENT_SCOPE)
endfunction()
