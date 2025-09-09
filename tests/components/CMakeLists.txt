cmake_minimum_required(VERSION 3.20)

project(atom_iocomponent.test)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest QUIET)

if(NOT GTEST_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)
else()
  include(GoogleTest)
endif()

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../)

# Create comprehensive component tests
set(TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/simple_test.cpp
    ${PROJECT_SOURCE_DIR}/comprehensive_component_test.cpp
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# Link with atom-component library and its dependencies
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    gtest
    gtest_main
    atom-component
    atom-error
    atom-utils
)

# Set C++20 standard
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Add compiler flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Pass macro definitions for optional features
if(ATOM_ENABLE_LUA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_ENABLE_LUA=1)
endif()

if(ATOM_ENABLE_PYTHON)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_ENABLE_PYTHON=1)
endif()

# Enable testing
enable_testing()

# Add test discovery
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
