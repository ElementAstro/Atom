cmake_minimum_required(VERSION 3.20)

project(atom_iosearch.test)

# Set C++ standard to C++20 for concepts support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
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

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Check for MariaDB/MySQL (optional)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB libmariadb)

file(GLOB TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Exclude CMake generated files
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*CMakeFiles.*")

# Add atom search implementation sources
set(ATOM_SEARCH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atom/search/core/search.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../atom/search/database/sqlite.cpp
)

# Add MySQL source only if MariaDB is available
if(MARIADB_FOUND)
    list(APPEND ATOM_SEARCH_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../../atom/search/database/mysql.cpp)
endif()

add_executable(${PROJECT_NAME} ${TEST_SOURCES} ${ATOM_SEARCH_SOURCES})

# Set include directories to find atom headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/../../atom
)

# Link required libraries including atom search dependencies
target_link_libraries(${PROJECT_NAME}
    gtest
    gtest_main
    ${SQLite3_LIBRARIES}
    fmt
)

# Add MariaDB if available
if(MARIADB_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${MARIADB_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${MARIADB_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_HAS_MARIADB)
endif()

# Enable testing
enable_testing()
gtest_discover_tests(${PROJECT_NAME})
