# Connection Examples Build Configuration
cmake_minimum_required(VERSION 3.21)

# Build options
option(ATOM_EXAMPLE_CONNECTION_BUILD_ALL "Build all connection examples" ON)

# Add subdirectories for organized examples
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fifo")
  add_subdirectory(fifo)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/serial")
  add_subdirectory(serial)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shared")
  add_subdirectory(shared)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ssh")
  add_subdirectory(ssh)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tcp")
  add_subdirectory(tcp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/udp")
  add_subdirectory(udp)
endif()

message(STATUS "Connection Examples: Subdirectories configured")
