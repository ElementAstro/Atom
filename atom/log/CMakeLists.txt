cmake_minimum_required(VERSION 3.10)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Define atom-log sources
set(ATOM_LOG_SOURCES async_logger.cpp atomlog.cpp log_manager.cpp logger.cpp
                     mmap_logger.cpp)

# Define atom-log headers
set(ATOM_LOG_HEADERS async_logger.hpp atomlog.hpp log_manager.hpp logger.hpp
                     mmap_logger.hpp)

# Create atom-log library
add_library(atom-log SHARED ${ATOM_LOG_SOURCES} ${ATOM_LOG_HEADERS})

# Find and link spdlog (required)
find_package(spdlog REQUIRED)
message(STATUS "spdlog found - linking to atom-log")

# Find and link fmt if available (required for spdlog)
find_package(fmt QUIET)
if(fmt_FOUND)
  message(STATUS "fmt found - linking to atom-log")
endif()

# Configure module using standardized function
atom_configure_module(atom-log)

# Link module-specific dependencies
target_link_libraries(
  atom-log
  PUBLIC spdlog::spdlog atom-error atom-utils
         $<$<TARGET_EXISTS:ZLIB::ZLIB>:ZLIB::ZLIB>
         $<$<TARGET_EXISTS:fmt::fmt>:fmt::fmt>)

# Configure spdlog options
target_compile_definitions(atom-log PUBLIC SPDLOG_USE_STD_FORMAT=1
                                           SPDLOG_HEADER_ONLY)

# Install headers
install(FILES ${ATOM_LOG_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/log)

message(STATUS "atom-log library configured with spdlog")
