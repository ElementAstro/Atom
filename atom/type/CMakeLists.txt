# CMakeLists.txt for Type Module
# Part of the Atom Project
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.21)

# Define library name
set(LIB_NAME atom-type)

# Find all source files
file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h" "*.hpp")

# Create library target
if(SOURCES)
    # Create library with source files
    add_library(${LIB_NAME} ${SOURCES} ${HEADERS})
else()
    # Create header-only library
    add_library(${LIB_NAME} INTERFACE)
endif()

# Handle header-only vs. compiled library differently
if(SOURCES)
    target_include_directories(${LIB_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/atom/type>
    )

    # Find required dependencies for JSON and YAML support
    find_package(nlohmann_json QUIET)
    if(nlohmann_json_FOUND)
        target_link_libraries(${LIB_NAME} PUBLIC nlohmann_json::nlohmann_json)
    endif()

    find_package(yaml-cpp QUIET)
    if(yaml-cpp_FOUND)
        target_link_libraries(${LIB_NAME} PUBLIC yaml-cpp::yaml-cpp)
    endif()

    # Basic dependencies
    target_link_libraries(${LIB_NAME} PUBLIC atom-error)
else()
    target_include_directories(${LIB_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/atom/type>
    )

    target_link_libraries(${LIB_NAME} INTERFACE atom-error)
endif()

# Add module to global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${LIB_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")

# Installation rules
install(TARGETS ${LIB_NAME}
    EXPORT ${LIB_NAME}-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/type
)

message(STATUS "Type module configured")
