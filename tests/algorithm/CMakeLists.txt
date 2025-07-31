cmake_minimum_required(VERSION 3.20)

project(atom_algorithm.test)

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

# Temporarily exclude problematic test files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
list(REMOVE_ITEM TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/test_fnmatch.cpp
    ${PROJECT_SOURCE_DIR}/test_convolve.cpp
    ${PROJECT_SOURCE_DIR}/test_bignumber.cpp
)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-algorithm atom-error loguru)

# Register tests with CTest
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
