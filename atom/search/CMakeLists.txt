cmake_minimum_required(VERSION 3.21)
project(
  atom-search
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES
    # Core search functionality
    core/search.cpp
    # Database backends
    database/sqlite.cpp)

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    cache.hpp
    lru.hpp
    ttl.hpp
    mysql.hpp
    sqlite.hpp
    search.hpp
    # Actual implementation headers (in subdirectories)
    core/search.hpp
    database/sqlite.hpp
    cache/cache.hpp
    cache/lru.hpp
    cache/ttl.hpp)

# Dependencies
find_package(spdlog CONFIG REQUIRED)

if(MSVC)
  find_package(SQLite3 QUIET)
  if(NOT SQLite3_FOUND)
    message(
      WARNING "SQLite3 not found for MSVC - database features will be disabled")
  endif()
else()
  find_package(SQLite3 REQUIRED)
endif()

set(LIBS loguru spdlog::spdlog ${CMAKE_THREAD_LIBS_INIT})
if(SQLite3_FOUND)
  list(APPEND LIBS ${SQLite3_LIBRARIES})
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(MARIADB libmariadb)
endif()
if(MARIADB_FOUND)
  message(STATUS "Found libmariadb: ${MARIADB_VERSION}")
  list(APPEND SOURCES database/mysql.cpp)
  list(APPEND HEADERS database/mysql.hpp)
  list(APPEND LIBS ${MARIADB_LIBRARIES})
  include_directories(${MARIADB_INCLUDE_DIRS})
else()
  message(STATUS "libmariadb not found, mysql support will be disabled.")
endif()

# Create library target
add_library(atom-search STATIC ${SOURCES} ${HEADERS})

# Configure module using standardized function
atom_configure_module(atom-search)

# Link module-specific dependencies
target_link_libraries(atom-search PRIVATE ${LIBS})

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/search)
