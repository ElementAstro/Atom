cmake_minimum_required(VERSION 3.20)
project(atom-containers VERSION 1.0.0 LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Headers
set(HEADERS
    boost_containers.hpp
    graph.hpp
    high_performance.hpp
    intrusive.hpp
    lockfree.hpp
)

# Configure module using standardized function
atom_configure_module(atom-containers HEADER_ONLY)

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/containers)
