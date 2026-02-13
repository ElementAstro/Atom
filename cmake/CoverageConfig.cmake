# CoverageConfig.cmake Configuration for code coverage analysis This project is
# licensed under the terms of the GPL3 license.
#
# Author: Max Qian License: GPL3

# Function to setup coverage for a target
function(setup_target_coverage target_name)
  if(ATOM_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(${target_name} PRIVATE --coverage)
      target_link_libraries(${target_name} PRIVATE --coverage)

      # Add target-specific coverage properties
      set_target_properties(${target_name} PROPERTIES COVERAGE_ENABLED TRUE)

      message(STATUS "Coverage enabled for target: ${target_name}")
    endif()
  endif()
endfunction()

# Function to add coverage exclude patterns
function(add_coverage_exclude_patterns)
  set(COVERAGE_EXCLUDE_PATTERNS
      '/usr/*'
      '*/tests/*'
      '*/test/*'
      '*/example/*'
      '*/examples/*'
      '*/third_party/*'
      '*/external/*'
      '*/build/*'
      '*/_deps/*'
      '*/vcpkg_installed/*'
      '*/CMakeFiles/*'
      '*.pb.h'
      '*.pb.cc'
      '*_generated.h'
      '*_generated.cpp'
      PARENT_SCOPE)
endfunction()

# Function to create module-specific coverage target
function(create_module_coverage_target module_name)
  if(NOT ATOM_ENABLE_COVERAGE
     OR NOT LCOV_PROGRAM
     OR NOT GENHTML_PROGRAM)
    return()
  endif()

  string(TOUPPER ${module_name} MODULE_UPPER)
  if(NOT ATOM_BUILD_${MODULE_UPPER})
    return()
  endif()

  set(MODULE_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage/${module_name}")
  set(MODULE_COVERAGE_INFO
      "${MODULE_COVERAGE_DIR}/${module_name}_coverage.info")
  set(MODULE_COVERAGE_CLEANED
      "${MODULE_COVERAGE_DIR}/${module_name}_coverage_cleaned.info")
  set(MODULE_COVERAGE_HTML "${MODULE_COVERAGE_DIR}/html")

  file(MAKE_DIRECTORY ${MODULE_COVERAGE_DIR})

  add_coverage_exclude_patterns()

  add_custom_target(
    coverage-${module_name}
    # Reset counters
    COMMAND ${LCOV_PROGRAM} --directory . --zerocounters
    # Run module-specific tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R ".*${module_name}.*"
    # Capture coverage data
    COMMAND ${LCOV_PROGRAM} --directory . --capture --output-file
            ${MODULE_COVERAGE_INFO}
    # Clean coverage data
    COMMAND
      ${LCOV_PROGRAM} --remove ${MODULE_COVERAGE_INFO}
      ${COVERAGE_EXCLUDE_PATTERNS} --output-file ${MODULE_COVERAGE_CLEANED}
    # Generate HTML report
    COMMAND
      ${GENHTML_PROGRAM} ${MODULE_COVERAGE_CLEANED} --output-directory
      ${MODULE_COVERAGE_HTML} --title "Atom ${module_name} Coverage Report"
      --show-details --legend --demangle-cpp --function-coverage
      --branch-coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage report for ${module_name} module")

  message(STATUS "Created coverage target: coverage-${module_name}")
endfunction()

# Function to setup comprehensive coverage reporting
function(setup_coverage_reporting)
  if(NOT ATOM_ENABLE_COVERAGE)
    return()
  endif()

  # Verify compiler support
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(WARNING "Coverage analysis requires GCC or Clang compiler")
    return()
  endif()

  # Find required tools
  find_program(GCOV_PROGRAM gcov)
  find_program(LCOV_PROGRAM lcov)
  find_program(GENHTML_PROGRAM genhtml)

  if(NOT GCOV_PROGRAM)
    message(WARNING "gcov not found - coverage analysis disabled")
    return()
  endif()

  if(NOT LCOV_PROGRAM)
    message(WARNING "lcov not found - HTML reports disabled")
    return()
  endif()

  if(NOT GENHTML_PROGRAM)
    message(WARNING "genhtml not found - HTML reports disabled")
    return()
  endif()

  # Set global coverage flags
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage"
      PARENT_SCOPE)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage"
      PARENT_SCOPE)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} --coverage"
      PARENT_SCOPE)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} --coverage"
      PARENT_SCOPE)

  # Create coverage output directory
  set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage")
  file(MAKE_DIRECTORY ${COVERAGE_OUTPUT_DIR})

  message(STATUS "Coverage reporting configured successfully")
  message(STATUS "  - gcov: ${GCOV_PROGRAM}")
  message(STATUS "  - lcov: ${LCOV_PROGRAM}")
  message(STATUS "  - genhtml: ${GENHTML_PROGRAM}")
  message(STATUS "  - Output directory: ${COVERAGE_OUTPUT_DIR}")
endfunction()

# Function to print coverage summary
function(print_coverage_info)
  if(ATOM_ENABLE_COVERAGE)
    message(STATUS "")
    message(STATUS "Coverage Analysis Configuration:")
    message(STATUS "  ATOM_ENABLE_COVERAGE: ${ATOM_ENABLE_COVERAGE}")
    message(STATUS "  ATOM_COVERAGE_HTML: ${ATOM_COVERAGE_HTML}")
    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
    message(STATUS "")
    message(STATUS "Available coverage targets:")
    message(
      STATUS
        "  make coverage          - Run all tests and generate full coverage report"
    )
    message(STATUS "  make coverage-reset    - Reset coverage counters")
    message(STATUS "  make coverage-capture  - Capture coverage data")
    message(STATUS "  make coverage-html     - Generate HTML report")
    message(
      STATUS "  make coverage-<module> - Generate coverage for specific module")
    message(STATUS "")
  endif()
endfunction()
