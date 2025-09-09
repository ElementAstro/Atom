cmake_minimum_required(VERSION 3.20)

project(atom_memory.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_memory_main.cpp)

# Create a simple memory test executable with minimal dependencies
add_executable(atom_memory_simple.test test_memory_simple.cpp)
target_link_libraries(atom_memory_simple.test gtest gtest_main atom-memory atom-error)
if(WIN32)
    target_link_libraries(atom_memory_simple.test kernel32)
endif()
set_property(TARGET atom_memory_simple.test PROPERTY CXX_STANDARD 20)
target_compile_definitions(atom_memory_simple.test PRIVATE ATOM_MEMORY_TRACKING_DISABLED=1)

# Create an alternative memory test with no dependencies on atom-memory
add_executable(atom_memory_alternative.test test_memory_simple_alternative.cpp)
target_link_libraries(atom_memory_alternative.test gtest gtest_main)
set_property(TARGET atom_memory_alternative.test PROPERTY CXX_STANDARD 20)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main atom-memory atom-error loguru)

# Add Windows system libraries if on Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME} psapi pdh iphlpapi kernel32 user32)
endif()
