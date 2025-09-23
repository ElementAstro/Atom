# CMakeLists.txt for Atom-Connection
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Connection
# Description: Connection Between Lithium Drivers, TCP and IPC
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)
project(atom-connection VERSION 1.0.0 LANGUAGES C CXX)

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
if(asio_FOUND)
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

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_object ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
)

# Install rules
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
