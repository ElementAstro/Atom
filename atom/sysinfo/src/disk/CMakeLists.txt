cmake_minimum_required(VERSION 3.20)
project(atom_sysinfo_disk VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Define the library sources
set(DISK_SOURCES
    disk.cpp
)

set(DISK_HEADERS
    disk.hpp
)

# Common functionality sources
set(COMMON_SOURCES
    common/disk_util.cpp
)

set(COMMON_HEADERS
    common/disk_types.hpp
    common/disk_util.hpp
)

# Component sources
set(COMPONENT_SOURCES
    components/disk_device.cpp
    components/disk_info.cpp
    components/disk_monitor.cpp
    components/disk_security.cpp
    components/disk_analytics.cpp
    components/disk_performance.cpp
)

set(COMPONENT_HEADERS
    components/disk_device.hpp
    components/disk_info.hpp
    components/disk_monitor.hpp
    components/disk_security.hpp
    components/disk_analytics.hpp
    components/disk_performance.hpp
)

# Platform-specific sources (if needed in the future)
# if(WIN32)
#     list(APPEND DISK_SOURCES platform/windows.cpp)
# elseif(APPLE)
#     list(APPEND DISK_SOURCES platform/macos.cpp)
# elseif(UNIX)
#     list(APPEND DISK_SOURCES platform/linux.cpp)
# endif()

# Combine all sources
set(ALL_SOURCES
    ${DISK_SOURCES}
    ${COMMON_SOURCES}
    ${COMPONENT_SOURCES}
)

set(ALL_HEADERS
    ${DISK_HEADERS}
    ${COMMON_HEADERS}
    ${COMPONENT_HEADERS}
)

# Create the library
add_library(${PROJECT_NAME} STATIC ${ALL_SOURCES} ${ALL_HEADERS})

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        spdlog::spdlog
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE setupapi)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

# Compiler-specific options
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Installation
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${DISK_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/disk
)

install(FILES ${COMMON_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/disk/common
)

install(FILES ${COMPONENT_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/disk/components
)

# Export targets
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE atom::sysinfo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Skip config file generation if template doesn't exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()

# Install config files if they exist
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake")
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()
