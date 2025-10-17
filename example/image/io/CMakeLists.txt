# I/O Image Examples
cmake_minimum_required(VERSION 3.10)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# I/O image examples
set(IO_EXAMPLES
    basic_file_operations
    format_conversion
    advanced_io
    caching_prefetching
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
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OPENCV)
            target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})
        endif()

        if(ATOM_IMAGE_HAS_STB)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_STB)
        endif()

        if(ATOM_IMAGE_HAS_FREEIMAGE)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_FREEIMAGE)
            target_link_libraries(${EXECUTABLE_NAME} ${FreeImage_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_IMAGEIO)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_IMAGEIO)
        endif()

        # Cloud storage support
        if(ATOM_IMAGE_HAS_AWS_SDK)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_AWS_SDK)
        endif()

        if(ATOM_IMAGE_HAS_AZURE_SDK)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_AZURE_SDK)
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

# Add any additional CPP files in this directory
file(GLOB ADDITIONAL_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ADDITIONAL_CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Check if it's not already in the main list
    list(FIND IO_EXAMPLES ${EXAMPLE_NAME} FOUND_INDEX)
    if(FOUND_INDEX EQUAL -1)
        add_io_example(${EXAMPLE_NAME})
    endif()
endforeach()
