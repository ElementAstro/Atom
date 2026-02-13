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

# Core module sources
set(CORE_SOURCES)
set(CORE_HEADERS core/error_codes.hpp core/types.hpp core/result.hpp
                 core/utils.hpp)

# Crypto module sources
set(CRYPTO_SOURCES crypto/secure_memory.cpp crypto/key_derivation.cpp
                   crypto/hash.cpp crypto/hmac.cpp crypto/encryption.cpp)
set(CRYPTO_HEADERS crypto/secure_memory.hpp crypto/key_derivation.hpp
                   crypto/hash.hpp crypto/hmac.hpp crypto/encryption.hpp)

# Password module sources
set(PASSWORD_SOURCES password/generator.cpp password/validator.cpp
                     password/breach_checker.cpp)
set(PASSWORD_HEADERS password/entry.hpp password/generator.hpp
                     password/validator.hpp password/breach_checker.hpp)

# OTP module sources
set(OTP_SOURCES otp/totp.cpp otp/hotp.cpp)
set(OTP_HEADERS otp/totp.hpp otp/hotp.hpp)

# Storage module sources
set(STORAGE_SOURCES storage/storage.cpp storage/file_storage.cpp
                    storage/backup.cpp)
set(STORAGE_HEADERS storage/storage.hpp storage/file_storage.hpp
                    storage/backup.hpp)

# Manager module sources
set(MANAGER_SOURCES manager/session.cpp manager/audit_log.cpp
                    manager/password_manager.cpp)
set(MANAGER_HEADERS manager/session.hpp manager/audit_log.hpp
                    manager/password_manager.hpp)

# Serialization module sources
set(SERIALIZATION_SOURCES serialization/json.cpp)
set(SERIALIZATION_HEADERS serialization/json.hpp)

# Legacy sources (for backward compatibility) Note: encryption.cpp removed to
# avoid conflict with crypto/encryption.cpp
set(LEGACY_SOURCES password_manager.cpp password_utils.cpp serialization.cpp
                   storage.cpp)
set(LEGACY_HEADERS
    common.hpp
    encryption.hpp
    password_entry.hpp
    password_manager.hpp
    password_utils.hpp
    result.hpp
    serialization.hpp
    storage.hpp)

# Combine all sources
set(SOURCES
    ${CORE_SOURCES}
    ${CRYPTO_SOURCES}
    ${PASSWORD_SOURCES}
    ${OTP_SOURCES}
    ${STORAGE_SOURCES}
    ${MANAGER_SOURCES}
    ${SERIALIZATION_SOURCES}
    ${LEGACY_SOURCES})

set(HEADERS
    ${CORE_HEADERS}
    ${CRYPTO_HEADERS}
    ${PASSWORD_HEADERS}
    ${OTP_HEADERS}
    ${STORAGE_HEADERS}
    ${MANAGER_HEADERS}
    ${SERIALIZATION_HEADERS}
    ${LEGACY_HEADERS}
    secret.hpp)

set(LIBS ${CMAKE_THREAD_LIBS_INIT})

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
  if(TARGET spdlog::spdlog)
    list(APPEND LIBS spdlog::spdlog)
  else()
    list(APPEND LIBS spdlog::spdlog)
  endif()
endif()

# Create library target
add_library(atom-secret STATIC ${SOURCES} ${HEADERS})
add_library(atom::secret ALIAS atom-secret)

# Configure module using standardized function
atom_configure_module(atom-secret)

# Link module-specific dependencies
target_link_libraries(
  atom-secret
  PUBLIC atom::algorithm atom::type atom::utils
  PRIVATE ${LIBS})

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
