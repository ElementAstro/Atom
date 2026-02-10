# CMakeLists.txt for Atom-Connection This project is licensed under the terms of
# the GPL3 license.
#
# Project Name: Atom-Connection Description: Connection Between Lithium Drivers,
# TCP and IPC Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-connection
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources organized by connection type
set(SOURCES
    # FIFO/Named Pipe connections
    fifo/fifoclient.cpp
    fifo/fifoserver.cpp
    fifo/fifo_codec.cpp
    fifo/fifo_platform.cpp
    # TCP connections
    tcp/tcpclient.cpp
    # UDP connections
    udp/udpclient.cpp
    udp/udpserver.cpp
    # Serial/TTY connections
    serial/ttybase.cpp
    # Shared utilities
    shared/sockethub.cpp)

# Check for ASIO support and add async sources if available For MSYS2/MinGW,
# asio is a header-only library installed at /mingw64/include We check for the
# header directly since there's no CMake config file
find_package(asio QUIET)
if(NOT asio_FOUND)
  # Check common locations for asio.hpp
  find_file(
    ASIO_HEADER asio.hpp
    PATHS /mingw64/include ${CMAKE_PREFIX_PATH}/include
          $ENV{MINGW_PREFIX}/include
    NO_DEFAULT_PATH)
  if(ASIO_HEADER)
    set(asio_FOUND TRUE)
    message(STATUS "Found standalone ASIO at ${ASIO_HEADER}")
  endif()
endif()
# Always enable async features for now since we know asio is installed
set(asio_FOUND TRUE)
if(asio_FOUND)
  list(
    APPEND
    SOURCES
    # Async FIFO
    fifo/async_fifoclient.cpp
    fifo/async_fifoserver.cpp
    # Async TCP
    tcp/async_tcpclient.cpp
    # Async UDP
    udp/async_udpclient.cpp
    udp/async_udpserver.cpp
    # Async shared utilities
    shared/async_sockethub.cpp)
  message(STATUS "ASIO found - async connection features enabled")
else()
  message(STATUS "ASIO not found - async connection features disabled")
endif()

# Headers organized by connection type
set(HEADERS
    # FIFO/Named Pipe connections
    fifo/fifoclient.hpp
    fifo/fifoserver.hpp
    fifo/fifo_common.hpp
    fifo/fifo_codec.hpp
    fifo/fifo_platform.hpp
    fifo/fifo_threadpool.hpp
    fifo/fifo_logger.hpp
    # TCP connections
    tcp/tcp_common.hpp
    tcp/tcpclient.hpp
    # UDP connections
    udp/udp_common.hpp
    udp/udp_platform.hpp
    udp/multicast_helper.hpp
    udp/udpclient.hpp
    udp/udpserver.hpp
    # Serial/TTY connections
    serial/ttybase.hpp
    # Shared utilities - common types and components
    shared/socket_types.hpp
    shared/socket_client_base.hpp
    shared/rate_limiter.hpp
    shared/sockethub.hpp)

# Add async headers if ASIO is available
if(asio_FOUND)
  list(
    APPEND
    HEADERS
    # Async FIFO
    fifo/async_fifoclient.hpp
    fifo/async_fifoserver.hpp
    # Async TCP
    tcp/async_tcpclient.hpp
    # Async UDP
    udp/async_udpclient.hpp
    udp/async_udpserver.hpp
    # Async shared utilities
    shared/async_sockethub.hpp)
endif()

# SSH connections (optional)
if(ATOM_USE_SSH)
  list(APPEND SOURCES ssh/sshclient.cpp ssh/sshserver.cpp)
  list(APPEND HEADERS ssh/sshclient.hpp ssh/sshserver.hpp)
endif()

# Dependencies
find_package(spdlog QUIET)
set(LIBS ${CMAKE_THREAD_LIBS_INIT})
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

# Add OpenSSL if available (optional for MSVC)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

if(WIN32)
  list(APPEND LIBS ws2_32 mswsock)
endif()

if(ATOM_USE_SSH)
  list(APPEND LIBS ${LIBSSH_LIBRARIES})
endif()

# Create library target
add_library(atom-connection STATIC ${SOURCES} ${HEADERS})
add_library(atom::connection ALIAS atom-connection)

# Configure module using standardized function
atom_configure_module(atom-connection)

# Link module-specific dependencies
target_link_libraries(atom-connection PRIVATE ${LIBS} atom-log)

# Install library target
install(
  TARGETS atom-connection
  EXPORT atom-connection-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-connection-targets
  FILE atom-connectionTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers preserving directory structure
install(
  DIRECTORY fifo/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection/fifo
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY tcp/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection/tcp
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY udp/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection/udp
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY serial/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection/serial
  FILES_MATCHING
  PATTERN "*.hpp")
install(
  DIRECTORY shared/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection/shared
  FILES_MATCHING
  PATTERN "*.hpp")
if(ATOM_USE_SSH)
  install(
    DIRECTORY ssh/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection/ssh
    FILES_MATCHING
    PATTERN "*.hpp")
endif()
