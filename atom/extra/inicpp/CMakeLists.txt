# CMakeLists.txt for INI file parser library Integrated with main Atom build
# system Header-only library

# Header files
set(INICPP_HEADERS
    common.hpp
    convert.hpp
    event_listener.hpp
    field.hpp
    file.hpp
    format_converter.hpp
    inicpp.hpp
    path_query.hpp
    section.hpp)

# Create interface library (header-only)
add_library(atom-extra-inicpp INTERFACE)

# Set target properties
target_include_directories(atom-extra-inicpp
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
target_compile_features(atom-extra-inicpp INTERFACE cxx_std_20)

# Installation rules (optional) install(TARGETS atom-extra-inicpp DESTINATION
# lib) install(FILES ${INICPP_HEADERS} DESTINATION include/atom/extra/inicpp)
