# CMakeLists.txt for sysinfo_printer module
cmake_minimum_required(VERSION 3.20)

project(
  atom_sysinfo_printer
  VERSION 1.0.0
  DESCRIPTION "System Information Printer Module"
  LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(spdlog REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Main library sources
set(SYSINFO_PRINTER_SOURCES printer.cpp)

set(SYSINFO_PRINTER_HEADERS printer.hpp)

# Formatter sources
set(FORMATTER_SOURCES
    formatters/base_formatter.cpp
    formatters/battery_formatter.cpp
    formatters/bios_formatter.cpp
    formatters/cpu_formatter.cpp
    formatters/disk_formatter.cpp
    formatters/gpu_formatter.cpp
    formatters/locale_formatter.cpp
    formatters/memory_formatter.cpp
    formatters/network_formatter.cpp
    formatters/os_formatter.cpp
    formatters/system_formatter.cpp)

set(FORMATTER_HEADERS
    formatters/base_formatter.hpp
    formatters/battery_formatter.hpp
    formatters/bios_formatter.hpp
    formatters/cpu_formatter.hpp
    formatters/disk_formatter.hpp
    formatters/gpu_formatter.hpp
    formatters/locale_formatter.hpp
    formatters/memory_formatter.hpp
    formatters/network_formatter.hpp
    formatters/os_formatter.hpp
    formatters/system_formatter.hpp)

# Exporter sources
set(EXPORTER_SOURCES
    exporters/base_exporter.cpp exporters/html_exporter.cpp
    exporters/json_exporter.cpp exporters/markdown_exporter.cpp
    exporters/xml_exporter.cpp exporters/csv_exporter.cpp)

set(EXPORTER_HEADERS
    exporters/base_exporter.hpp exporters/html_exporter.hpp
    exporters/json_exporter.hpp exporters/markdown_exporter.hpp
    exporters/xml_exporter.hpp exporters/csv_exporter.hpp)

# Report sources
set(REPORT_SOURCES
    reports/base_report.cpp
    reports/full_report.cpp
    reports/simple_report.cpp
    reports/performance_report.cpp
    reports/security_report.cpp
    reports/hardware_report.cpp
    reports/software_report.cpp
    reports/custom_report.cpp)

set(REPORT_HEADERS
    reports/base_report.hpp
    reports/full_report.hpp
    reports/simple_report.hpp
    reports/performance_report.hpp
    reports/security_report.hpp
    reports/hardware_report.hpp
    reports/software_report.hpp
    reports/custom_report.hpp)

# Utility sources
set(UTILITY_SOURCES utils/table_utils.cpp utils/format_utils.cpp
                    utils/string_utils.cpp utils/template_engine.cpp)

set(UTILITY_HEADERS utils/table_utils.hpp utils/format_utils.hpp
                    utils/string_utils.hpp utils/template_engine.hpp)

# Combine all sources
set(ALL_SOURCES ${SYSINFO_PRINTER_SOURCES} ${FORMATTER_SOURCES}
                ${EXPORTER_SOURCES} ${REPORT_SOURCES} ${UTILITY_SOURCES})

set(ALL_HEADERS ${SYSINFO_PRINTER_HEADERS} ${FORMATTER_HEADERS}
                ${EXPORTER_HEADERS} ${REPORT_HEADERS} ${UTILITY_HEADERS})

# Create the main library
add_library(atom_sysinfo_printer STATIC ${ALL_SOURCES} ${ALL_HEADERS})

# Link dependencies
target_link_libraries(atom_sysinfo_printer PRIVATE spdlog::spdlog)

# Set target properties
set_target_properties(
  atom_sysinfo_printer
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL
                                           "Clang")
  target_compile_options(
    atom_sysinfo_printer PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter
                                 -Wno-unused-variable)
endif()

# Add subdirectories for components
add_subdirectory(formatters)
add_subdirectory(exporters)
add_subdirectory(reports)
add_subdirectory(utils)

# Add examples if requested
option(BUILD_SYSINFO_PRINTER_EXAMPLES "Build sysinfo_printer examples" OFF)
if(BUILD_SYSINFO_PRINTER_EXAMPLES)
  add_subdirectory(examples)
endif()

# Add tests if requested
option(BUILD_SYSINFO_PRINTER_TESTS "Build sysinfo_printer tests" OFF)
if(BUILD_SYSINFO_PRINTER_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Install targets
install(
  TARGETS atom_sysinfo_printer
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

# Install headers
install(FILES ${SYSINFO_PRINTER_HEADERS}
        DESTINATION include/atom/sysinfo/sysinfo_printer)

install(FILES ${FORMATTER_HEADERS}
        DESTINATION include/atom/sysinfo/sysinfo_printer/formatters)

install(FILES ${EXPORTER_HEADERS}
        DESTINATION include/atom/sysinfo/sysinfo_printer/exporters)

install(FILES ${REPORT_HEADERS}
        DESTINATION include/atom/sysinfo/sysinfo_printer/reports)

install(FILES ${UTILITY_HEADERS}
        DESTINATION include/atom/sysinfo/sysinfo_printer/utils)

# Install templates
install(
  DIRECTORY templates/
  DESTINATION share/atom/sysinfo_printer/templates
  FILES_MATCHING
  PATTERN "*.html"
  PATTERN "*.md"
  PATTERN "*.json"
  PATTERN "*.xml")

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_printer_config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_printer_config.cmake"
  INSTALL_DESTINATION lib/cmake/atom_sysinfo_printer)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_printer_config_version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_printer_config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_printer_config_version.cmake"
  DESTINATION lib/cmake/atom_sysinfo_printer)
