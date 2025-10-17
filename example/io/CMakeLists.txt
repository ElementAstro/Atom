# CMakeLists.txt for Atom I/O Examples
cmake_minimum_required(VERSION 3.20)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compiler warnings
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Get subdirectory name for executable prefix
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Options for building examples
option(ATOM_EXAMPLE_IO_BUILD_ALL "Build all io examples" ON)
option(ATOM_EXAMPLE_IO_BUILD_CORE "Build core io examples" ON)
option(ATOM_EXAMPLE_IO_BUILD_ASYNC "Build async io examples" ON)
option(ATOM_EXAMPLE_IO_BUILD_COMPRESSION "Build compression examples" ON)
option(ATOM_EXAMPLE_IO_BUILD_FILESYSTEM "Build filesystem examples" ON)
option(ATOM_EXAMPLE_IO_BUILD_INTEGRATION "Build integration examples" ON)

# Function to create examples for a subdirectory
function(create_subdir_examples SUBDIR_PATH CATEGORY_NAME BUILD_OPTION)
  if(${BUILD_OPTION})
    file(GLOB SUBDIR_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR_PATH}/*.cpp)

    foreach(CPP_FILE ${SUBDIR_CPP_FILES})
      get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
      set(EXECUTABLE_NAME ${SUBDIR_NAME}_${CATEGORY_NAME}_${EXAMPLE_NAME})

      string(TOUPPER ${CATEGORY_NAME}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
      option(ATOM_EXAMPLE_IO_${EXAMPLE_NAME_UPPER}
             "Build ${CATEGORY_NAME} example: ${EXAMPLE_NAME}"
             ${ATOM_EXAMPLE_IO_BUILD_ALL})

      if(ATOM_EXAMPLE_IO_${EXAMPLE_NAME_UPPER})
        add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
        target_link_libraries(${EXECUTABLE_NAME} atom)

        # Set output directory
        set_target_properties(
          ${EXECUTABLE_NAME}
          PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                     ${CMAKE_BINARY_DIR}/examples/io/${CATEGORY_NAME})

        # Set IDE folder grouping
        set_property(TARGET ${EXECUTABLE_NAME}
                     PROPERTY FOLDER "Examples/Io/${CATEGORY_NAME}")

        # Add to category list
        list(APPEND ${CATEGORY_NAME}_TARGETS ${EXECUTABLE_NAME})
        set(${CATEGORY_NAME}_TARGETS
            ${${CATEGORY_NAME}_TARGETS}
            PARENT_SCOPE)
      endif()
    endforeach()
  endif()
endfunction()

# Create examples for each category
create_subdir_examples("core" "core" ATOM_EXAMPLE_IO_BUILD_CORE)
create_subdir_examples("async" "async" ATOM_EXAMPLE_IO_BUILD_ASYNC)
create_subdir_examples("compression" "compression"
                       ATOM_EXAMPLE_IO_BUILD_COMPRESSION)
create_subdir_examples("filesystem" "filesystem"
                       ATOM_EXAMPLE_IO_BUILD_FILESYSTEM)

# Handle root-level integration examples
if(ATOM_EXAMPLE_IO_BUILD_INTEGRATION)
  file(GLOB INTEGRATION_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*_examples.cpp)

  foreach(CPP_FILE ${INTEGRATION_CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_IO_${EXAMPLE_NAME_UPPER}
           "Build integration example: ${EXAMPLE_NAME}"
           ${ATOM_EXAMPLE_IO_BUILD_ALL})

    if(ATOM_EXAMPLE_IO_${EXAMPLE_NAME_UPPER})
      add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
      target_link_libraries(${EXECUTABLE_NAME} atom)

      set_target_properties(
        ${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                      ${CMAKE_BINARY_DIR}/examples/io)

      set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER
                                                      "Examples/Io/Integration")

      list(APPEND integration_TARGETS ${EXECUTABLE_NAME})
    endif()
  endforeach()
endif()

# Create category targets
if(core_TARGETS)
  add_custom_target(io_core_examples DEPENDS ${core_TARGETS})
endif()

if(async_TARGETS)
  add_custom_target(io_async_examples DEPENDS ${async_TARGETS})
endif()

if(compression_TARGETS)
  add_custom_target(io_compression_examples DEPENDS ${compression_TARGETS})
endif()

if(filesystem_TARGETS)
  add_custom_target(io_filesystem_examples DEPENDS ${filesystem_TARGETS})
endif()

if(integration_TARGETS)
  add_custom_target(io_integration_examples_group
                    DEPENDS ${integration_TARGETS})
endif()

# Create master target for all I/O examples
set(ALL_IO_TARGETS)
if(core_TARGETS)
  list(APPEND ALL_IO_TARGETS ${core_TARGETS})
endif()
if(async_TARGETS)
  list(APPEND ALL_IO_TARGETS ${async_TARGETS})
endif()
if(compression_TARGETS)
  list(APPEND ALL_IO_TARGETS ${compression_TARGETS})
endif()
if(filesystem_TARGETS)
  list(APPEND ALL_IO_TARGETS ${filesystem_TARGETS})
endif()
if(integration_TARGETS)
  list(APPEND ALL_IO_TARGETS ${integration_TARGETS})
endif()

if(ALL_IO_TARGETS)
  add_custom_target(io_examples ALL DEPENDS ${ALL_IO_TARGETS})
endif()

# Installation
if(ALL_IO_TARGETS)
  install(TARGETS ${ALL_IO_TARGETS} DESTINATION bin/examples/io)
endif()

# Print configuration summary
message(STATUS "Atom I/O Examples Configuration:")
message(STATUS "  Core examples: ${ATOM_EXAMPLE_IO_BUILD_CORE}")
message(STATUS "  Async examples: ${ATOM_EXAMPLE_IO_BUILD_ASYNC}")
message(STATUS "  Compression examples: ${ATOM_EXAMPLE_IO_BUILD_COMPRESSION}")
message(STATUS "  Filesystem examples: ${ATOM_EXAMPLE_IO_BUILD_FILESYSTEM}")
message(STATUS "  Integration examples: ${ATOM_EXAMPLE_IO_BUILD_INTEGRATION}")
if(ALL_IO_TARGETS)
  list(LENGTH ALL_IO_TARGETS TARGET_COUNT)
  message(STATUS "  Total targets: ${TARGET_COUNT}")
endif()
