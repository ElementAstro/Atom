cmake_minimum_required(VERSION 3.20)

project(atom_ioconnection.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Remove build directory files and files with their own main() functions
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/build/.*")
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/udpclient.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# Set C++20 standard
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../../
    ${CMAKE_SOURCE_DIR}/../../atom/connection
)

# Add compiler definitions for Windows
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

target_link_libraries(${PROJECT_NAME} gtest gtest_main atom-connection atom-error loguru)
