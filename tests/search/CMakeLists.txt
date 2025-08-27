cmake_minimum_required(VERSION 3.20)

project(atom_iosearch.test)

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

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# Set include directories to find atom headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/../../atom
)

# For now, let's build a simple test without external dependencies
# We'll add the actual library linking later once the main project is built
target_link_libraries(${PROJECT_NAME}
    gtest
    gtest_main
    ${SQLite3_LIBRARIES}
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
