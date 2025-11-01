# CMakeLists.txt for Atom-Web Examples This project is licensed under the terms
# of the GPL3 license.
#
# Project Name: Atom-Web Examples Description: Comprehensive examples for Web
# API Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.20)

# Get subdirectory name for executable prefix
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

# Find loguru - try multiple approaches
find_path(LOGURU_INCLUDE_DIR loguru.hpp
          PATHS /usr/include /usr/local/include /mingw64/include
                ${CMAKE_PREFIX_PATH}/include)
find_library(
  LOGURU_LIBRARY
  NAMES loguru
  PATHS /usr/lib /usr/local/lib /mingw64/lib ${CMAKE_PREFIX_PATH}/lib)

# Try pkg-config as fallback
if(NOT LOGURU_INCLUDE_DIR OR NOT LOGURU_LIBRARY)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LOGURU QUIET loguru)
  endif()
endif()

if(NOT LOGURU_INCLUDE_DIR AND NOT LOGURU_FOUND)
  message(STATUS "Loguru not found, web examples will be built without logging")
  set(LOGURU_AVAILABLE FALSE)
else()
  set(LOGURU_AVAILABLE TRUE)
  if(LOGURU_INCLUDE_DIR)
    message(STATUS "Found Loguru: ${LOGURU_INCLUDE_DIR}")
  else()
    message(STATUS "Found Loguru via pkg-config")
  endif()
endif()

# Options for building examples
option(ATOM_EXAMPLE_WEB_BUILD_ALL "Build all web examples" ON)
option(ATOM_EXAMPLE_WEB_BUILD_BASIC "Build basic web examples" ON)
option(ATOM_EXAMPLE_WEB_BUILD_ADDRESS "Build address examples" ON)
option(ATOM_EXAMPLE_WEB_BUILD_HTTP "Build HTTP examples" ON)
option(ATOM_EXAMPLE_WEB_BUILD_MIME "Build MIME examples" ON)
option(ATOM_EXAMPLE_WEB_BUILD_INTEGRATION "Build integration examples" ON)

# Function to create example executable
function(create_web_example EXAMPLE_PATH CATEGORY)
  get_filename_component(EXAMPLE_NAME ${EXAMPLE_PATH} NAME_WE)
  get_filename_component(EXAMPLE_DIR ${EXAMPLE_PATH} DIRECTORY)

  # Create unique target name
  if(EXAMPLE_DIR)
    string(REPLACE "/" "_" TARGET_NAME
                   "${SUBDIR_NAME}_${EXAMPLE_DIR}_${EXAMPLE_NAME}")
  else()
    set(TARGET_NAME "${SUBDIR_NAME}_${EXAMPLE_NAME}")
  endif()

  # Check if this category should be built
  string(TOUPPER ${CATEGORY} CATEGORY_UPPER)
  if(NOT ATOM_EXAMPLE_WEB_BUILD_${CATEGORY_UPPER}
     AND NOT ATOM_EXAMPLE_WEB_BUILD_ALL)
    return()
  endif()

  # Individual example option
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_WEB_${EXAMPLE_NAME_UPPER}
         "Build web example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_WEB_BUILD_ALL})

  if(ATOM_EXAMPLE_WEB_${EXAMPLE_NAME_UPPER})
    add_executable(${TARGET_NAME} ${EXAMPLE_PATH}.cpp)

    target_link_libraries(${TARGET_NAME} atom ${CMAKE_THREAD_LIBS_INIT})

    # Add loguru if available
    if(LOGURU_AVAILABLE)
      if(LOGURU_LIBRARY)
        target_link_libraries(${TARGET_NAME} ${LOGURU_LIBRARY})
        target_include_directories(${TARGET_NAME} PRIVATE ${LOGURU_INCLUDE_DIR})
      elseif(LOGURU_FOUND)
        target_link_libraries(${TARGET_NAME} ${LOGURU_LIBRARIES})
        target_include_directories(${TARGET_NAME}
                                   PRIVATE ${LOGURU_INCLUDE_DIRS})
      endif()
    endif()

    # Platform-specific libraries
    if(WIN32)
      target_link_libraries(${TARGET_NAME} wsock32 ws2_32)
    endif()

    # Set IDE folder grouping
    set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER
                                                "Examples/Web/${CATEGORY}")

    message(STATUS "Added web example: ${TARGET_NAME}")
  endif()
endfunction()

# Define examples by category
set(BASIC_EXAMPLES curl httpparser minetype time utils)

set(ADDRESS_EXAMPLES
    address/ipv4_example address/ipv6_example address/unix_domain_example
    address/address_factory_example)

set(HTTP_EXAMPLES http/downloader_example http/curl_comprehensive_example
                  http/httpparser_comprehensive_example)

set(MIME_EXAMPLES mime/minetype_comprehensive_example)

set(INTEGRATION_EXAMPLES integration_example)

# Create examples
foreach(EXAMPLE ${BASIC_EXAMPLES})
  create_web_example(${EXAMPLE} "BASIC")
endforeach()

foreach(EXAMPLE ${ADDRESS_EXAMPLES})
  create_web_example(${EXAMPLE} "ADDRESS")
endforeach()

foreach(EXAMPLE ${HTTP_EXAMPLES})
  create_web_example(${EXAMPLE} "HTTP")
endforeach()

foreach(EXAMPLE ${MIME_EXAMPLES})
  create_web_example(${EXAMPLE} "MIME")
endforeach()

foreach(EXAMPLE ${INTEGRATION_EXAMPLES})
  create_web_example(${EXAMPLE} "INTEGRATION")
endforeach()

# Print configuration summary
message(STATUS "")
message(STATUS "Atom Web Examples Configuration:")
message(STATUS "  Basic examples: ${BASIC_EXAMPLES}")
message(STATUS "  Address examples: ${ADDRESS_EXAMPLES}")
message(STATUS "  HTTP examples: ${HTTP_EXAMPLES}")
message(STATUS "  MIME examples: ${MIME_EXAMPLES}")
message(STATUS "  Integration examples: ${INTEGRATION_EXAMPLES}")
message(STATUS "")
message(STATUS "Build options:")
message(STATUS "  ATOM_EXAMPLE_WEB_BUILD_ALL: ${ATOM_EXAMPLE_WEB_BUILD_ALL}")
message(
  STATUS "  ATOM_EXAMPLE_WEB_BUILD_BASIC: ${ATOM_EXAMPLE_WEB_BUILD_BASIC}")
message(
  STATUS "  ATOM_EXAMPLE_WEB_BUILD_ADDRESS: ${ATOM_EXAMPLE_WEB_BUILD_ADDRESS}")
message(STATUS "  ATOM_EXAMPLE_WEB_BUILD_HTTP: ${ATOM_EXAMPLE_WEB_BUILD_HTTP}")
message(STATUS "  ATOM_EXAMPLE_WEB_BUILD_MIME: ${ATOM_EXAMPLE_WEB_BUILD_MIME}")
message(
  STATUS
    "  ATOM_EXAMPLE_WEB_BUILD_INTEGRATION: ${ATOM_EXAMPLE_WEB_BUILD_INTEGRATION}"
)
