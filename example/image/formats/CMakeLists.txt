# Format Image Examples
cmake_minimum_required(VERSION 3.10)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# Format image examples
set(FORMAT_EXAMPLES
    comprehensive_fits
    advanced_formats
    raw_processing
    medical_formats
    scientific_formats
    ser_video
    basic_ser_operations
    # ser_quality_assessment  # Disabled: uses non-existent QualityMetrics struct and API methods
    # ser_registration_stacking  # Disabled: API mismatches (subPixelAccuracy, translation, QualityMetrics)
    # format_conversion_edge_cases  # Disabled: missing format_converter.hpp header
)

# Function to add format example
function(add_format_example EXAMPLE_NAME)
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}.cpp)
    set(EXECUTABLE_NAME ${PARENT_DIR}_formats_${EXAMPLE_NAME})

    if(EXISTS ${SOURCE_FILE})
        add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
        target_link_libraries(${EXECUTABLE_NAME} atom)

        # Add conditional compilation flags for format support
        if(ATOM_IMAGE_HAS_OPENCV)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OPENCV)
            target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})
        endif()

        if(ATOM_IMAGE_HAS_CFITSIO)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_CFITSIO)
            target_link_libraries(${EXECUTABLE_NAME} ${CFITSIO_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_DCMTK)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_DCMTK)
            target_link_libraries(${EXECUTABLE_NAME} ${DCMTK_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_HDF5)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_HDF5)
            target_link_libraries(${EXECUTABLE_NAME} ${HDF5_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_LIBRAW)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_LIBRAW)
            target_link_libraries(${EXECUTABLE_NAME} ${LibRaw_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_OPENEXR)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OPENEXR)
            target_link_libraries(${EXECUTABLE_NAME} ${OpenEXR_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_WEBP)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_WEBP)
            target_link_libraries(${EXECUTABLE_NAME} ${WebP_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_AVIF)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_AVIF)
            target_link_libraries(${EXECUTABLE_NAME} ${AVIF_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_HEIF)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_HEIF)
            target_link_libraries(${EXECUTABLE_NAME} ${HEIF_LIBRARIES})
        endif()

        # Video format support
        if(ATOM_IMAGE_HAS_FFMPEG)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_FFMPEG)
            target_link_libraries(${EXECUTABLE_NAME} ${FFmpeg_LIBRARIES})
        endif()

        # Set IDE folder grouping
        set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Image/Formats")

        message(STATUS "Added format image example: ${EXECUTABLE_NAME}")
    else()
        message(WARNING "Format example source file not found: ${SOURCE_FILE}")
    endif()
endfunction()

# Add all format examples
foreach(EXAMPLE ${FORMAT_EXAMPLES})
    add_format_example(${EXAMPLE})
endforeach()

# Add any additional CPP files in this directory
file(GLOB ADDITIONAL_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ADDITIONAL_CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Check if it's not already in the main list
    list(FIND FORMAT_EXAMPLES ${EXAMPLE_NAME} FOUND_INDEX)
    if(FOUND_INDEX EQUAL -1)
        add_format_example(${EXAMPLE_NAME})
    endif()
endforeach()
