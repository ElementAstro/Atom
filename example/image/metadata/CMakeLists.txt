# Metadata Image Examples
cmake_minimum_required(VERSION 3.10)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# Metadata image examples
set(METADATA_EXAMPLES exif_operations batch_metadata metadata_preservation)

# Function to add metadata example
function(add_metadata_example EXAMPLE_NAME)
  set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}.cpp)
  set(EXECUTABLE_NAME ${PARENT_DIR}_metadata_${EXAMPLE_NAME})

  if(EXISTS ${SOURCE_FILE})
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Add conditional compilation flags for metadata support
    if(ATOM_IMAGE_HAS_EXIV2)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_EXIV2)
      target_link_libraries(${EXECUTABLE_NAME} ${Exiv2_LIBRARIES})
    endif()

    if(ATOM_IMAGE_HAS_LIBEXIF)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_LIBEXIF)
      target_link_libraries(${EXECUTABLE_NAME} ${LibExif_LIBRARIES})
    endif()

    if(ATOM_IMAGE_HAS_XMP)
      target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_XMP)
      target_link_libraries(${EXECUTABLE_NAME} ${XMP_LIBRARIES})
    endif()

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER
                                                    "Examples/Image/Metadata")

    message(STATUS "Added metadata image example: ${EXECUTABLE_NAME}")
  else()
    message(WARNING "Metadata example source file not found: ${SOURCE_FILE}")
  endif()
endfunction()

# Add all metadata examples
foreach(EXAMPLE ${METADATA_EXAMPLES})
  add_metadata_example(${EXAMPLE})
endforeach()

# Add any additional CPP files in this directory
file(GLOB ADDITIONAL_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ADDITIONAL_CPP_FILES})
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Check if it's not already in the main list
  list(FIND METADATA_EXAMPLES ${EXAMPLE_NAME} FOUND_INDEX)
  if(FOUND_INDEX EQUAL -1)
    add_metadata_example(${EXAMPLE_NAME})
  endif()
endforeach()
