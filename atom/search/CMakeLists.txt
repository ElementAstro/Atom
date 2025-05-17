cmake_minimum_required(VERSION 3.20)
project(
  atom-search
  VERSION 1.0.0
  LANGUAGES C CXX)

# Sources
set(SOURCES search.cpp sqlite.cpp)

# Headers
set(HEADERS cache.hpp search.hpp sqlite.hpp)

# Dependencies
set(LIBS loguru ${CMAKE_THREAD_LIBS_INIT} ${SQLite3_LIBRARIES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB libmariadb)
if(MARIADB_FOUND)
  message(STATUS "Found libmariadb: ${MARIADB_VERSION}")
  list(APPEND SOURCES mysql.cpp)
  list(APPEND HEADERS mysql.hpp)
  list(APPEND LIBS ${MARIADB_LIBRARIES})
  include_directories(${MARIADB_INCLUDE_DIRS})
else()
  message(STATUS "libmariadb not found, mysql support will be disabled.")
endif()

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Install rules
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
