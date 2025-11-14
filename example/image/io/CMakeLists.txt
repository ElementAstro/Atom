# I/O Image Examples
cmake_minimum_required(VERSION 3.10)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# I/O image examples
set(IO_EXAMPLES
    file_operations
    format_conversion
    io_features
    caching_prefetching
    format_detector_example
    image_loader_example
    image_saver_example
    # streaming_io_demo  # Disabled: missing streaming_reader.hpp header
    # memory_mapped_io  # Disabled: missing memory_mapped_file.hpp header
)

# Function to add I/O example
function(add_io_example EXAMPLE_NAME)
  set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}.cpp)
  set(EXECUTABLE_NAME ${PARENT_DIR}_io_${EXAMPLE_NAME})

  if(EXISTS ${SOURCE_FILE})
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Add conditional compilation flags for I/O backends
    if(ATOM_IMAGE_HAS_OPENCV)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_OPENCV)
      target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})
    endif()

    if(ATOM_IMAGE_HAS_STB)
      target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_STB)
    endif()

    if(ATOM_IMAGE_HAS_FREEIMAGE)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_FREEIMAGE)
      target_link_libraries(${EXECUTABLE_NAME} ${FreeImage_LIBRARIES})
    endif()

    if(ATOM_IMAGE_HAS_IMAGEIO)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_IMAGEIO)
    endif()

    # Cloud storage support
    if(ATOM_IMAGE_HAS_AWS_SDK)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_AWS_SDK)
    endif()

    if(ATOM_IMAGE_HAS_AZURE_SDK)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_AZURE_SDK)
    endif()

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Image/IO")

    message(STATUS "Added I/O image example: ${EXECUTABLE_NAME}")
  else()
    message(WARNING "I/O example source file not found: ${SOURCE_FILE}")
  endif()
endfunction()

# Add all I/O examples
foreach(EXAMPLE ${IO_EXAMPLES})
  add_io_example(${EXAMPLE})
endforeach()

# Intentionally do not auto-add all .cpp files here; only curated IO_EXAMPLES
# are built
