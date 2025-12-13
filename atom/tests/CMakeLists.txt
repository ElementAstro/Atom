# CMakeLists.txt for Atom-Tests This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Tests Description: Test Suite for Atom Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-tests
  VERSION 1.0.0
  LANGUAGES C CXX)

# Sources (implementation files)
set(SOURCES performance/benchmark.cpp performance/fuzz.cpp performance/perf.cpp)

# Main entry point header
set(MAIN_HEADERS atom_test.hpp)

# Core testing framework headers
set(CORE_HEADERS core/test.hpp core/test_runner.hpp core/test_cli.hpp
                 core/test_registry.hpp)

# Assertion and matcher headers
set(ASSERTION_HEADERS assertions/matchers.hpp assertions/death_test.hpp
                      assertions/string_matchers.hpp)

# Test fixture headers
set(FIXTURE_HEADERS fixtures/test_fixture.hpp fixtures/test_parameterized.hpp
                    fixtures/typed_test.hpp)

# Mocking framework headers
set(MOCKING_HEADERS mocking/test_mock.hpp mocking/mock_matchers.hpp)

# Test utility headers
set(UTILITY_HEADERS utilities/test_skip.hpp utilities/test_data.hpp
                    utilities/test_environment.hpp)

# Reporter headers
set(REPORTER_HEADERS reporters/test_reporter.hpp
                     reporters/test_reporter_charts.hpp)

# Performance testing headers
set(PERFORMANCE_HEADERS performance/benchmark.hpp performance/fuzz.hpp
                        performance/perf.hpp)

# Combine all headers
set(HEADERS
    ${MAIN_HEADERS}
    ${CORE_HEADERS}
    ${ASSERTION_HEADERS}
    ${FIXTURE_HEADERS}
    ${MOCKING_HEADERS}
    ${UTILITY_HEADERS}
    ${REPORTER_HEADERS}
    ${PERFORMANCE_HEADERS})

# Find nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
  endif()
endif()

find_package(spdlog QUIET)
set(LIBS ${CMAKE_THREAD_LIBS_INIT})
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

# Add nlohmann/json if found
if(nlohmann_json_FOUND)
  list(APPEND LIBS nlohmann_json::nlohmann_json)
elseif(NLOHMANN_JSON_FOUND)
  list(APPEND LIBS ${NLOHMANN_JSON_LIBRARIES})
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Add nlohmann/json include directories if found via pkg-config
if(NLOHMANN_JSON_FOUND AND NOT nlohmann_json_FOUND)
  target_include_directories(${PROJECT_NAME}_object
                             PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

# Add nlohmann/json include directories if found via pkg-config
if(NLOHMANN_JSON_FOUND AND NOT nlohmann_json_FOUND)
  target_include_directories(${PROJECT_NAME}
                             PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

# Platform-specific libraries
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE pdh wlanapi)
endif()

# Set library properties
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Installation
install(
  TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION include/${PROJECT_NAME})

# Install main headers
install(FILES ${MAIN_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests)

# Install core headers
install(FILES ${CORE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/core)

# Install assertion headers
install(FILES ${ASSERTION_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/assertions)

# Install fixture headers
install(FILES ${FIXTURE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/fixtures)

# Install mocking headers
install(FILES ${MOCKING_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/mocking)

# Install utility headers
install(FILES ${UTILITY_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/utilities)

# Install reporter headers
install(FILES ${REPORTER_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/reporters)

# Install performance headers
install(FILES ${PERFORMANCE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/tests/performance)
