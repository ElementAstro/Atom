# Algorithm Examples Directory
cmake_minimum_required(VERSION 3.21)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Options for building algorithm examples
option(ATOM_EXAMPLE_ALGORITHM_BUILD_ALL "Build all algorithm examples" ON)

# Function to create executable for a source file
function(create_algorithm_example CPP_FILE CATEGORY)
  # Get filename without extension
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Construct executable name (category_filename)
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${CATEGORY}_${EXAMPLE_NAME})

  # Configuration option for individual example control
  string(TOUPPER ${CATEGORY}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_ALGORITHM_${EXAMPLE_NAME_UPPER}
         "Build algorithm example: ${CATEGORY}/${EXAMPLE_NAME}"
         ${ATOM_EXAMPLE_ALGORITHM_BUILD_ALL})

  # Conditionally add executable
  if(ATOM_EXAMPLE_ALGORITHM_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME}
                 PROPERTY FOLDER "Examples/Algorithm/${CATEGORY}")
  endif()
endfunction()

# Process examples in organized subdirectories
set(ALGORITHM_CATEGORIES
    core
    crypto
    hash
    math
    compression
    signal
    optimization
    encoding
    graphics
    utils)

foreach(CATEGORY ${ALGORITHM_CATEGORIES})
  set(CATEGORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${CATEGORY}")
  if(EXISTS ${CATEGORY_DIR})
    file(GLOB CATEGORY_CPP_FILES "${CATEGORY_DIR}/*.cpp")
    foreach(CPP_FILE ${CATEGORY_CPP_FILES})
      create_algorithm_example(${CPP_FILE} ${CATEGORY})
    endforeach()
  endif()
endforeach()

# Process legacy examples in root directory (for backward compatibility)
file(GLOB ROOT_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ROOT_CPP_FILES})
  # Get filename without extension
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Construct executable name (legacy format)
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # Configuration option for individual example control
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_ALGORITHM_${EXAMPLE_NAME_UPPER}
         "Build algorithm example: ${EXAMPLE_NAME}"
         ${ATOM_EXAMPLE_ALGORITHM_BUILD_ALL})

  # Conditionally add executable
  if(ATOM_EXAMPLE_ALGORITHM_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER
                                                    "Examples/Algorithm/Legacy")
  endif()
endforeach()
