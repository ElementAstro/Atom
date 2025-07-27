# CMakeLists.txt for Atom-Secret This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Secret Description: Secret Management Library for Atom
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)
project(
  atom-secret
  VERSION 2.0.0
  LANGUAGES C CXX)

# Sources and Headers
set(SOURCES common.cpp encryption.cpp storage.cpp password_manager.cpp)

set(HEADERS common.hpp encryption.hpp password_entry.hpp storage.hpp password_manager.hpp result.hpp)

set(LIBS spdlog::spdlog ${CMAKE_THREAD_LIBS_INIT})

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_object>)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

if(LINUX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLIB REQUIRED glib-2.0)
  pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${GLIB_LIBRARIES}
                                                ${LIBSECRET_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PUBLIC ${GLIB_INCLUDE_DIRS}
                                                    ${LIBSECRET_INCLUDE_DIRS})
endif()

target_include_directories(${PROJECT_NAME} PUBLIC .)

# Set library properties
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

# Add tests subdirectory
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  enable_testing()
  add_subdirectory(tests)
endif()

# Install targets
install(FILES ${HEADERS} DESTINATION include/atom/secret)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)
