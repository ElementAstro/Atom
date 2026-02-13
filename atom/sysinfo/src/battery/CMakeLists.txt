cmake_minimum_required(VERSION 3.20)
project(
  atom_sysinfo_battery
  VERSION 1.0.0
  LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Define the library
set(BATTERY_SOURCES battery.cpp common.cpp)

set(BATTERY_HEADERS battery.hpp common.hpp)

# Add platform-specific sources
if(WIN32)
  list(APPEND BATTERY_SOURCES platform/windows.cpp)
  list(APPEND BATTERY_HEADERS platform/windows.hpp)
elseif(UNIX AND NOT APPLE)
  list(APPEND BATTERY_SOURCES platform/linux.cpp)
  list(APPEND BATTERY_HEADERS platform/linux.hpp)
elseif(APPLE)
  list(APPEND BATTERY_SOURCES platform/macos.cpp)
  list(APPEND BATTERY_HEADERS platform/macos.hpp)
endif()

# Create the library
add_library(atom_sysinfo_battery ${BATTERY_SOURCES} ${BATTERY_HEADERS})

# Set target properties
set_target_properties(
  atom_sysinfo_battery
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION 1
             CXX_STANDARD 20
             CXX_STANDARD_REQUIRED ON)

# Include directories
target_include_directories(
  atom_sysinfo_battery
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries
target_link_libraries(atom_sysinfo_battery PUBLIC spdlog::spdlog)

# Platform-specific libraries
if(WIN32)
  target_link_libraries(atom_sysinfo_battery PRIVATE setupapi powrprof)
elseif(APPLE)
  find_library(IOKIT_FRAMEWORK IOKit)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
  target_link_libraries(atom_sysinfo_battery
                        PRIVATE ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

# Compiler-specific options
if(MSVC)
  target_compile_options(atom_sysinfo_battery PRIVATE /W4)
else()
  target_compile_options(atom_sysinfo_battery PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install targets
install(
  TARGETS atom_sysinfo_battery
  EXPORT atom_sysinfo_batteryTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES
  DESTINATION include)

install(FILES ${BATTERY_HEADERS} DESTINATION include/atom/sysinfo/battery)

# Export targets
install(
  EXPORT atom_sysinfo_batteryTargets
  FILE atom_sysinfo_batteryTargets.cmake
  NAMESPACE atom::
  DESTINATION lib/cmake/atom_sysinfo_battery)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  atom_sysinfo_batteryConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_battery_config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_batteryConfig.cmake
  INSTALL_DESTINATION lib/cmake/atom_sysinfo_battery)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_batteryConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_batteryConfigVersion.cmake
  DESTINATION lib/cmake/atom_sysinfo_battery)

# Add subdirectories
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  add_subdirectory(examples)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_subdirectory(tests)
endif()
