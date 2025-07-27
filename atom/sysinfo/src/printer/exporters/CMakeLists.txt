# CMakeLists.txt for exporters
cmake_minimum_required(VERSION 3.20)

# Exporter sources
set(EXPORTER_SOURCES
    base_exporter.cpp
    html_exporter.cpp
    json_exporter.cpp
    markdown_exporter.cpp
    xml_exporter.cpp
    csv_exporter.cpp
)

set(EXPORTER_HEADERS
    base_exporter.hpp
    html_exporter.hpp
    json_exporter.hpp
    markdown_exporter.hpp
    xml_exporter.hpp
    csv_exporter.hpp
)

# Create exporters library
add_library(atom_sysinfo_exporters STATIC ${EXPORTER_SOURCES} ${EXPORTER_HEADERS})

# Link dependencies
target_link_libraries(atom_sysinfo_exporters
    PRIVATE
        spdlog::spdlog
)

# Set target properties
set_target_properties(atom_sysinfo_exporters PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(atom_sysinfo_exporters
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
