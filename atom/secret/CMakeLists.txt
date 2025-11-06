# CMakeLists.txt for Atom-Secret This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-Secret Description: Secret Management Library for Atom
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-secret
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources and Headers
set(SOURCES encryption.cpp password_manager.cpp password_utils.cpp
            serialization.cpp storage.cpp)

set(HEADERS
    common.hpp
    encryption.hpp
    password_entry.hpp
    password_manager.hpp
    password_utils.hpp
    result.hpp
    serialization.hpp
    storage.hpp)

set(LIBS loguru ${CMAKE_THREAD_LIBS_INIT})

# Find required packages
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto)
  message(STATUS "OpenSSL found - cryptographic features enabled")
else()
  message(
    STATUS "OpenSSL not found - some cryptographic features will be disabled")
endif()

# Find spdlog
find_package(spdlog QUIET)
if(spdlog_FOUND)
  if(TARGET spdlog::spdlog_header_only)
    list(APPEND LIBS spdlog::spdlog_header_only)
  else()
    list(APPEND LIBS spdlog::spdlog)
  endif()
endif()

# Create library target
add_library(atom-secret STATIC ${SOURCES} ${HEADERS})

# Configure module using standardized function
atom_configure_module(atom-secret)

# Link module-specific dependencies
target_link_libraries(atom-secret PRIVATE ${LIBS})

# Platform-specific dependencies
if(LINUX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLIB REQUIRED glib-2.0)
  pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
  target_link_libraries(atom-secret PRIVATE ${GLIB_LIBRARIES}
                                            ${LIBSECRET_LIBRARIES})
  target_include_directories(atom-secret PUBLIC ${GLIB_INCLUDE_DIRS}
                                                ${LIBSECRET_INCLUDE_DIRS})
endif()

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/secret)
