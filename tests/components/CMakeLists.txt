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

# Create a simple test that doesn't require external libraries
set(TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/simple_test.cpp
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main)
