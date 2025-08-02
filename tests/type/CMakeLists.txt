cmake_minimum_required(VERSION 3.20)

project(atom_type.test)

find_package(GTest QUIET)
find_package(TBB QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# Link libraries including TBB for parallel execution support and atom-type
if(TBB_FOUND)
    target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-type atom-error loguru TBB::tbb)
else()
    target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-type atom-error loguru tbb)
endif()

# Register tests with CTest
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
