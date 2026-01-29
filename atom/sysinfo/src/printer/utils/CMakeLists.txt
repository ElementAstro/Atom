# CMakeLists.txt for utils
cmake_minimum_required(VERSION 3.20)

# Utility sources
set(UTILITY_SOURCES table_utils.cpp format_utils.cpp string_utils.cpp
                    template_engine.cpp)

set(UTILITY_HEADERS table_utils.hpp format_utils.hpp string_utils.hpp
                    template_engine.hpp)

# Create utils library
add_library(atom_sysinfo_utils STATIC ${UTILITY_SOURCES} ${UTILITY_HEADERS})

# Link dependencies
target_link_libraries(atom_sysinfo_utils PRIVATE spdlog::spdlog)

# Set target properties
set_target_properties(
  atom_sysinfo_utils
  PROPERTIES CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)

# Include directories
target_include_directories(
  atom_sysinfo_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
                            ${CMAKE_CURRENT_SOURCE_DIR}/../..)
