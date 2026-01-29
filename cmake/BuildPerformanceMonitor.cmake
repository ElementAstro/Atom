# cmake/BuildPerformanceMonitor.cmake Build performance monitoring and reporting
# module Tracks build times, resource usage, and provides optimization
# recommendations

# Avoid repeated inclusion
if(DEFINED BUILD_PERFORMANCE_MONITOR_INCLUDED)
  return()
endif()
set(BUILD_PERFORMANCE_MONITOR_INCLUDED TRUE)

# -----------------------------------------------------------------------------
# Performance Monitoring Variables
# -----------------------------------------------------------------------------
set(ATOM_BUILD_START_TIME
    ""
    CACHE INTERNAL "Build start time")
set(ATOM_BUILD_METRICS_FILE
    "${CMAKE_BINARY_DIR}/build_metrics.json"
    CACHE INTERNAL "Build metrics file")
set(ATOM_ENABLE_BUILD_METRICS
    ON
    CACHE BOOL "Enable build performance metrics collection")

# -----------------------------------------------------------------------------
# Build Time Tracking Functions
# -----------------------------------------------------------------------------
function(start_build_timer)
  if(ATOM_ENABLE_BUILD_METRICS)
    string(TIMESTAMP ATOM_BUILD_START_TIME "%s" UTC)
    set(ATOM_BUILD_START_TIME
        ${ATOM_BUILD_START_TIME}
        CACHE INTERNAL "Build start time" FORCE)
    message(STATUS "Build performance monitoring started")
  endif()
endfunction()

function(end_build_timer)
  if(ATOM_ENABLE_BUILD_METRICS AND ATOM_BUILD_START_TIME)
    string(TIMESTAMP BUILD_END_TIME "%s" UTC)
    math(EXPR BUILD_DURATION "${BUILD_END_TIME} - ${ATOM_BUILD_START_TIME}")

    # Convert to human readable format
    math(EXPR BUILD_MINUTES "${BUILD_DURATION} / 60")
    math(EXPR BUILD_SECONDS "${BUILD_DURATION} % 60")

    message(STATUS "Build completed in ${BUILD_MINUTES}m ${BUILD_SECONDS}s")

    # Store metrics
    set(BUILD_METRICS_JSON
        "{
    \"build_duration_seconds\": ${BUILD_DURATION},
    \"build_start_time\": ${ATOM_BUILD_START_TIME},
    \"build_end_time\": ${BUILD_END_TIME},
    \"cmake_version\": \"${CMAKE_VERSION}\",
    \"generator\": \"${CMAKE_GENERATOR}\",
    \"build_type\": \"${CMAKE_BUILD_TYPE}\",
    \"system\": \"${CMAKE_SYSTEM_NAME}\",
    \"processor\": \"${CMAKE_SYSTEM_PROCESSOR}\"
}")

    file(WRITE ${ATOM_BUILD_METRICS_FILE} ${BUILD_METRICS_JSON})
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Target Build Time Tracking
# -----------------------------------------------------------------------------
function(track_target_build_time TARGET_NAME)
  if(ATOM_ENABLE_BUILD_METRICS)
    # Add custom commands to track individual target build times
    add_custom_command(
      TARGET ${TARGET_NAME}
      PRE_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Building target: ${TARGET_NAME}"
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "Start time: $<SHELL_PATH:$<TARGET_PROPERTY:${TARGET_NAME},BINARY_DIR>>"
      VERBATIM)

    add_custom_command(
      TARGET ${TARGET_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Completed target: ${TARGET_NAME}"
      VERBATIM)
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Compilation Database Analysis
# -----------------------------------------------------------------------------
function(analyze_compilation_database)
  if(ATOM_ENABLE_BUILD_METRICS AND EXISTS
                                   "${CMAKE_BINARY_DIR}/compile_commands.json")
    # Count compilation units
    file(READ "${CMAKE_BINARY_DIR}/compile_commands.json"
         COMPILE_COMMANDS_CONTENT)
    string(REGEX MATCHALL "\"command\":" COMMAND_MATCHES
                 "${COMPILE_COMMANDS_CONTENT}")
    list(LENGTH COMMAND_MATCHES COMPILATION_UNITS_COUNT)

    message(STATUS "Compilation units: ${COMPILATION_UNITS_COUNT}")

    # Analyze include patterns (simplified)
    string(REGEX MATCHALL "-I[^ ]*" INCLUDE_DIRS "${COMPILE_COMMANDS_CONTENT}")
    list(REMOVE_DUPLICATES INCLUDE_DIRS)
    list(LENGTH INCLUDE_DIRS UNIQUE_INCLUDE_DIRS)

    message(STATUS "Unique include directories: ${UNIQUE_INCLUDE_DIRS}")
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Build Size Analysis
# -----------------------------------------------------------------------------
function(analyze_build_size)
  if(ATOM_ENABLE_BUILD_METRICS)
    # Calculate build directory size
    if(EXISTS "${CMAKE_BINARY_DIR}")
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E env du -sh "${CMAKE_BINARY_DIR}"
        OUTPUT_VARIABLE BUILD_SIZE_OUTPUT
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

      if(BUILD_SIZE_OUTPUT)
        string(REGEX MATCH "^[0-9.]+[KMGT]?" BUILD_SIZE "${BUILD_SIZE_OUTPUT}")
        message(STATUS "Build directory size: ${BUILD_SIZE}")
      endif()
    endif()

    # Analyze binary sizes
    file(
      GLOB_RECURSE
      BINARY_FILES
      "${CMAKE_BINARY_DIR}/*.exe"
      "${CMAKE_BINARY_DIR}/*.dll"
      "${CMAKE_BINARY_DIR}/*.so"
      "${CMAKE_BINARY_DIR}/*.dylib"
      "${CMAKE_BINARY_DIR}/*.a")

    set(TOTAL_BINARY_SIZE 0)
    foreach(BINARY_FILE ${BINARY_FILES})
      file(SIZE "${BINARY_FILE}" BINARY_SIZE)
      math(EXPR TOTAL_BINARY_SIZE "${TOTAL_BINARY_SIZE} + ${BINARY_SIZE}")
    endforeach()

    if(TOTAL_BINARY_SIZE GREATER 0)
      math(EXPR TOTAL_BINARY_SIZE_MB "${TOTAL_BINARY_SIZE} / 1024 / 1024")
      message(STATUS "Total binary size: ${TOTAL_BINARY_SIZE_MB} MB")
    endif()
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Performance Recommendations
# -----------------------------------------------------------------------------
function(generate_performance_recommendations)
  if(ATOM_ENABLE_BUILD_METRICS)
    message(STATUS "=== Build Performance Recommendations ===")

    # Check for ccache
    if(NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
      message(STATUS "Recommendation: Enable ccache for faster rebuilds")
      message(STATUS "  Add -DATOM_ENABLE_CCACHE=ON to CMake configuration")
    endif()

    # Check for Ninja generator
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
      message(STATUS "Recommendation: Use Ninja generator for faster builds")
      message(STATUS "  Add -G Ninja to CMake configuration")
    endif()

    # Check for LTO in release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release"
       AND NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION)
      message(
        STATUS "Recommendation: Enable LTO for smaller, faster release builds")
      message(STATUS "  Add -DATOM_ENABLE_LTO=ON to CMake configuration")
    endif()

    # Check for precompiled headers
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
      message(
        STATUS
          "Recommendation: Enable precompiled headers for faster compilation")
      message(
        STATUS
          "  Add -DATOM_ENABLE_PRECOMPILED_HEADERS=ON to CMake configuration")
    endif()

    # Check for unity builds
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
      message(
        STATUS "Recommendation: Consider unity builds for faster compilation")
      message(
        STATUS "  Add -DATOM_ENABLE_UNITY_BUILD=ON to CMake configuration")
    endif()

    message(STATUS "==========================================")
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Build Report Generation
# -----------------------------------------------------------------------------
function(generate_build_report)
  if(ATOM_ENABLE_BUILD_METRICS)
    message(STATUS "=== Build Performance Report ===")

    # System information
    message(STATUS "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "CMake: ${CMAKE_VERSION}")
    message(STATUS "Generator: ${CMAKE_GENERATOR}")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

    # Compiler information
    message(
      STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    message(
      STATUS
        "CXX Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

    # Build configuration
    if(CMAKE_C_COMPILER_LAUNCHER)
      message(STATUS "C Compiler Launcher: ${CMAKE_C_COMPILER_LAUNCHER}")
    endif()
    if(CMAKE_CXX_COMPILER_LAUNCHER)
      message(STATUS "CXX Compiler Launcher: ${CMAKE_CXX_COMPILER_LAUNCHER}")
    endif()

    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
      message(STATUS "LTO: Enabled")
    else()
      message(STATUS "LTO: Disabled")
    endif()

    # Analyze compilation database
    analyze_compilation_database()

    # Analyze build size
    analyze_build_size()

    # Generate recommendations
    generate_performance_recommendations()

    message(STATUS "================================")
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Automatic Performance Monitoring Setup
# -----------------------------------------------------------------------------
function(setup_performance_monitoring)
  if(ATOM_ENABLE_BUILD_METRICS)
    # Start build timer
    start_build_timer()

    # Add build completion hook
    add_custom_target(
      build_performance_report ALL
      COMMAND ${CMAKE_COMMAND} -P
              "${CMAKE_CURRENT_LIST_DIR}/BuildPerformanceReport.cmake"
      COMMENT "Generating build performance report")

    # Make it run last
    set_target_properties(
      build_performance_report PROPERTIES EXCLUDE_FROM_ALL FALSE
                                          EXCLUDE_FROM_DEFAULT_BUILD FALSE)

    message(STATUS "Build performance monitoring enabled")
  endif()
endfunction()

# -----------------------------------------------------------------------------
# Performance Report Script Generation
# -----------------------------------------------------------------------------
function(create_performance_report_script)
  set(REPORT_SCRIPT_CONTENT
      "
# Build Performance Report Script
cmake_minimum_required(VERSION 3.21)

# Include this module
include(\"${CMAKE_CURRENT_LIST_FILE}\")

# Generate the report
end_build_timer()
generate_build_report()
")

  file(WRITE "${CMAKE_BINARY_DIR}/BuildPerformanceReport.cmake"
       "${REPORT_SCRIPT_CONTENT}")
endfunction()

# -----------------------------------------------------------------------------
# Main Setup Function
# -----------------------------------------------------------------------------
macro(enable_build_performance_monitoring)
  if(ATOM_ENABLE_BUILD_METRICS)
    setup_performance_monitoring()
    create_performance_report_script()
  endif()
endmacro()
