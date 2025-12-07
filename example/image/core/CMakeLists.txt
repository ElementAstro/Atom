# Core Image Examples
cmake_minimum_required(VERSION 3.21)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# Core image examples
set(CORE_EXAMPLES
    blob_operations
    backend_integration
    memory_management
    blob_edge_cases
    fast_blob_performance
    memory_management_features
    serialization_edge_cases
    caching_demo
    cache_manager_example
    image_metadata_example
    exceptions_example
    # error_handling  # Disabled: DEBUG macro conflict (fixed but needs testing)
    # boundary_conditions_demo  # Disabled: undefined references to
    # applyMedianFilter and detectEdges
)

# Function to add core example
function(add_core_example EXAMPLE_NAME)
  set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}.cpp)
  set(EXECUTABLE_NAME ${PARENT_DIR}_core_${EXAMPLE_NAME})

  if(EXISTS ${SOURCE_FILE})
    add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Add conditional compilation flags
    if(ATOM_IMAGE_HAS_OPENCV)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_OPENCV)
      target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})
    endif()

    if(ATOM_IMAGE_HAS_CIMG)
      target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_CIMG)
    endif()

    if(ATOM_IMAGE_HAS_STB)
      target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_STB)
    endif()

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER
                                                    "Examples/Image/Core")

    message(STATUS "Added core image example: ${EXECUTABLE_NAME}")
  else()
    message(WARNING "Core example source file not found: ${SOURCE_FILE}")
  endif()
endfunction()

# Add all core examples
foreach(EXAMPLE ${CORE_EXAMPLES})
  add_core_example(${EXAMPLE})
endforeach()

# Add any additional CPP files in this directory
file(GLOB ADDITIONAL_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ADDITIONAL_CPP_FILES})
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Check if it's not already in the main list
  list(FIND CORE_EXAMPLES ${EXAMPLE_NAME} FOUND_INDEX)
  if(FOUND_INDEX EQUAL -1)
    add_core_example(${EXAMPLE_NAME})
  endif()
endforeach()
