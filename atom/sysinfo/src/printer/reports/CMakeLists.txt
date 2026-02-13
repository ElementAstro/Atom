# CMakeLists.txt for reports
cmake_minimum_required(VERSION 3.20)

# Report sources
set(REPORT_SOURCES
    base_report.cpp
    full_report.cpp
    simple_report.cpp
    performance_report.cpp
    security_report.cpp
    hardware_report.cpp
    software_report.cpp
    custom_report.cpp)

set(REPORT_HEADERS
    base_report.hpp
    full_report.hpp
    simple_report.hpp
    performance_report.hpp
    security_report.hpp
    hardware_report.hpp
    software_report.hpp
    custom_report.hpp)

# Create reports library
add_library(atom_sysinfo_reports STATIC ${REPORT_SOURCES} ${REPORT_HEADERS})

# Link dependencies
target_link_libraries(atom_sysinfo_reports PRIVATE spdlog::spdlog
                                                   atom_sysinfo_formatters)

# Set target properties
set_target_properties(
  atom_sysinfo_reports
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)

# Include directories
target_include_directories(
  atom_sysinfo_reports PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
                              ${CMAKE_CURRENT_SOURCE_DIR}/../..)
