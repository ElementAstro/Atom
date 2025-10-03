# CMakeLists.txt for Atom-Connection
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Connection
# Description: Connection Between Lithium Drivers, TCP and IPC
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)
project(atom-connection VERSION 1.0.0 LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES
    fifoclient.cpp
    fifoserver.cpp
    sockethub.cpp
    tcpclient.cpp
    ttybase.cpp
    udpclient.cpp
    udpserver.cpp
)

# Check for ASIO support and add async sources if available
find_package(asio QUIET)
if(Asio_FOUND)
    list(APPEND SOURCES
        async_fifoclient.cpp
        async_fifoserver.cpp
        async_sockethub.cpp
        async_tcpclient.cpp
        async_udpclient.cpp
        async_udpserver.cpp
    )
    message(STATUS "ASIO found - async connection features enabled")
else()
    message(STATUS "ASIO not found - async connection features disabled")
endif()

# Headers
set(HEADERS
    fifoclient.hpp
    fifoserver.hpp
    sockethub.hpp
    tcpclient.hpp
    ttybase.hpp
    udpclient.hpp
    udpserver.hpp
)

# Add async headers if ASIO is available
if(asio_FOUND)
    list(APPEND HEADERS
        async_fifoclient.hpp
        async_fifoserver.hpp
        async_sockethub.hpp
        async_tcpclient.hpp
        async_udpclient.hpp
        async_udpserver.hpp
    )
endif()

if (ATOM_USE_SSH)
    list(APPEND SOURCES
        sshclient.cpp
        sshserver.cpp
    )
    list(APPEND HEADERS
        sshclient.hpp
        sshserver.hpp
    )
endif()

# Dependencies
set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
)

if (WIN32)
    list(APPEND LIBS ws2_32 mswsock)
endif()

if (ATOM_USE_SSH)
    list(APPEND LIBS ${LIBSSH_LIBRARIES})
endif()

# Create library target
add_library(atom-connection STATIC ${SOURCES} ${HEADERS})

# Configure module using standardized function
atom_configure_module(atom-connection)

# Link module-specific dependencies
target_link_libraries(atom-connection PRIVATE ${LIBS})

# Install headers
install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/connection
)
