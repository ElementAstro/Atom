cmake_minimum_required(VERSION 3.20)

project(atom_iotype.test)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest QUIET)

# Add include directories for atom headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../)

# Focus on header-only tests that work
set(ALL_TEST_FILES
    ${PROJECT_SOURCE_DIR}/test_header_only.cpp
)

add_executable(${PROJECT_NAME} ${ALL_TEST_FILES})

# Only link essential libraries for header-only tests
target_link_libraries(${PROJECT_NAME} gtest gtest_main)
