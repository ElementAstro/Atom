# Examples directory: extra
cmake_minimum_required(VERSION 3.21)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Optional: Add extra example-specific options
option(ATOM_EXAMPLE_EXTRA_BUILD_ALL "Build all extra examples" ON)

# Function to create examples from a directory
function(create_examples_from_directory DIR_NAME)
  # Get all CPP files in the directory
  file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}/*.cpp)

  foreach(CPP_FILE ${CPP_FILES})
    # Get filename without extension
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Construct executable name (subdir_dirname_filename)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${DIR_NAME}_${EXAMPLE_NAME})

    # Configuration option to control each example build individually
    string(TOUPPER ${DIR_NAME}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_EXTRA_${EXAMPLE_NAME_UPPER}
           "Build extra example: ${DIR_NAME}/${EXAMPLE_NAME}"
           ${ATOM_EXAMPLE_EXTRA_BUILD_ALL})

    # Conditionally add executable
    if(ATOM_EXAMPLE_EXTRA_${EXAMPLE_NAME_UPPER})
      add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
      target_link_libraries(${EXECUTABLE_NAME} atom)

      # Set IDE folder grouping
      set_property(TARGET ${EXECUTABLE_NAME}
                   PROPERTY FOLDER "Examples/Extra/${DIR_NAME}")
    endif()
  endforeach()
endfunction()

# Add examples from subdirectories
set(EXTRA_SUBDIRS
    asio
    beast
    boost
    curl
    dotenv
    iconv
    inicpp
    injection
    pugixml
    spdlog
    uv)

foreach(SUBDIR ${EXTRA_SUBDIRS})
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
    create_examples_from_directory(${SUBDIR})
  endif()
endforeach()
