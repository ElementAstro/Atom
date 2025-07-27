cmake_minimum_required(VERSION 3.20)
project(atom_sysinfo_common VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the library
set(COMMON_HEADERS
    types.hpp
    utils.hpp
    platform_detection.hpp
    error_handling.hpp
)

# Header-only library
add_library(${PROJECT_NAME} INTERFACE)

# Set target properties
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

# Include directories
target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Installation
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${COMMON_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/common
)

# Export targets
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE atom::sysinfo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
