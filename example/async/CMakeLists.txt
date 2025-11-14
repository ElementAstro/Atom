# Example directory: async This directory contains examples for the atom/async
# module, organized to mirror the structure of atom/async with subdirectories
# for each component category.
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Option: Build all async examples
option(ATOM_EXAMPLE_ASYNC_BUILD_ALL "Build all async examples" ON)

# Function to add examples from a directory
function(add_examples_from_directory CATEGORY_DIR CATEGORY_NAME)
  # Get all CPP files in the category directory
  file(GLOB CATEGORY_CPP_FILES
       ${CMAKE_CURRENT_SOURCE_DIR}/${CATEGORY_DIR}/*.cpp)

  foreach(CPP_FILE ${CATEGORY_CPP_FILES})
    # Get filename without extension
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Construct executable name (async_category_example_name)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${CATEGORY_NAME}_${EXAMPLE_NAME})

    # Configure option to control individual example builds
    string(TOUPPER ${CATEGORY_NAME}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_ASYNC_${EXAMPLE_NAME_UPPER}
           "Build async ${CATEGORY_NAME} example: ${EXAMPLE_NAME}"
           ${ATOM_EXAMPLE_ASYNC_BUILD_ALL})

    # Conditionally add executable
    if(ATOM_EXAMPLE_ASYNC_${EXAMPLE_NAME_UPPER})
      add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
      target_link_libraries(${EXECUTABLE_NAME} atom)

      # Set IDE folder grouping
      set_property(TARGET ${EXECUTABLE_NAME}
                   PROPERTY FOLDER "Examples/Async/${CATEGORY_NAME}")

      message(STATUS "Added example: ${EXECUTABLE_NAME}")
    endif()
  endforeach()
endfunction()

# Add examples from each subdirectory (mirroring atom/async structure)
add_examples_from_directory("core" "core")
add_examples_from_directory("execution" "execution")
add_examples_from_directory("messaging" "messaging")
add_examples_from_directory("sync" "sync")
add_examples_from_directory("threading" "threading")
add_examples_from_directory("utils" "utils")

# Add examples from root directory (for integration examples, etc.)
file(GLOB ROOT_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

foreach(CPP_FILE ${ROOT_CPP_FILES})
  # Get filename without extension
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Construct executable name (async_example_name)
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # Configure option to control individual example builds
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_ASYNC_${EXAMPLE_NAME_UPPER}
         "Build async example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_ASYNC_BUILD_ALL})

  # Conditionally add executable
  if(ATOM_EXAMPLE_ASYNC_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Async")

    message(STATUS "Added example: ${EXECUTABLE_NAME}")
  endif()
endforeach()
