# CMakeLists.txt for formatters
cmake_minimum_required(VERSION 3.20)

# Formatter sources
set(FORMATTER_SOURCES
    base_formatter.cpp
    battery_formatter.cpp
    bios_formatter.cpp
    cpu_formatter.cpp
    disk_formatter.cpp
    gpu_formatter.cpp
    locale_formatter.cpp
    memory_formatter.cpp
    network_formatter.cpp
    os_formatter.cpp
    system_formatter.cpp)

set(FORMATTER_HEADERS
    base_formatter.hpp
    battery_formatter.hpp
    bios_formatter.hpp
    cpu_formatter.hpp
    disk_formatter.hpp
    gpu_formatter.hpp
    locale_formatter.hpp
    memory_formatter.hpp
    network_formatter.hpp
    os_formatter.hpp
    system_formatter.hpp)

# Create formatters library
add_library(atom_sysinfo_formatters STATIC ${FORMATTER_SOURCES}
                                           ${FORMATTER_HEADERS})

# Link dependencies
target_link_libraries(atom_sysinfo_formatters PRIVATE spdlog::spdlog)

# Set target properties
set_target_properties(
  atom_sysinfo_formatters
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)

# Include directories
target_include_directories(
  atom_sysinfo_formatters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../..)
