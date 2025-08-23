cmake_minimum_required(VERSION 3.20)
project(
  atom-containers
  VERSION 1.0.0
  LANGUAGES C CXX)

# Headers
set(HEADERS
    boost_containers.hpp
    graph.hpp
    high_performance.hpp
    intrusive.hpp
    lockfree.hpp
)

# Create header-only library
add_library(atom-containers INTERFACE)
target_include_directories(atom-containers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/containers)

# Set target properties
set_target_properties(atom-containers PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Register this module to the global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${PROJECT_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
