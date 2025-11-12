# Example directory: utils
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Option: Build all utils examples
option(ATOM_EXAMPLE_UTILS_BUILD_ALL "Build all utils examples" ON)

# Define subdirectories that contain examples
set(UTILS_SUBDIRS
    container
    conversion
    core
    crypto
    debug
    format
    memory
    process
    random
    text
    time)

# Function to add examples from a directory
function(add_examples_from_directory DIR_PATH CATEGORY_NAME)
  # Get all CPP files in the directory
  file(GLOB CPP_FILES ${DIR_PATH}/*.cpp)

  foreach(CPP_FILE ${CPP_FILES})
    # Get filename without extension
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Construct executable name (utils_category_examplename)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${CATEGORY_NAME}_${EXAMPLE_NAME})

    # Configure option to control individual example builds
    string(TOUPPER ${CATEGORY_NAME}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_UTILS_${EXAMPLE_NAME_UPPER}
           "Build utils ${CATEGORY_NAME} example: ${EXAMPLE_NAME}"
           ${ATOM_EXAMPLE_UTILS_BUILD_ALL})

    # Conditionally add executable
    if(ATOM_EXAMPLE_UTILS_${EXAMPLE_NAME_UPPER})
      add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
      target_link_libraries(${EXECUTABLE_NAME} atom)

      # Add include directories explicitly
      target_include_directories(
        ${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/atom)

      # Set IDE folder grouping
      set_property(TARGET ${EXECUTABLE_NAME}
                   PROPERTY FOLDER "Examples/Utils/${CATEGORY_NAME}")
    endif()
  endforeach()
endfunction()

# Add examples from root directory (integration examples, etc.)
file(GLOB ROOT_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ROOT_CPP_FILES})
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_UTILS_${EXAMPLE_NAME_UPPER}
         "Build utils example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_UTILS_BUILD_ALL})

  if(ATOM_EXAMPLE_UTILS_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Add include directories explicitly
    target_include_directories(
      ${EXECUTABLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/atom)

    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Utils")
  endif()
endforeach()

# Add examples from each subdirectory
foreach(SUBDIR ${UTILS_SUBDIRS})
  set(SUBDIR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
  if(EXISTS ${SUBDIR_PATH})
    add_examples_from_directory(${SUBDIR_PATH} ${SUBDIR})
  endif()
endforeach()
