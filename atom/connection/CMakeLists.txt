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
    udpclient.cpp
    udpserver.cpp
)

# Headers
set(HEADERS
    fifoclient.hpp
    fifoserver.hpp
    sockethub.hpp
    tcpclient.hpp
    udpclient.hpp
    udpserver.hpp
)

if (ENABLE_LIBSSH)
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
)

if (WIN32)
    list(APPEND LIBS ws2_32)
endif()

if (ENABLE_SSH)
    find_package(LibSSH REQUIRED)
    list(APPEND LIBS ${LIBSSH_LIBRARIES})
    link_directories(${LIBSSH_LIBRARY_DIRS})
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
