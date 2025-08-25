# Image Examples Directory
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name for executable prefix
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Image-specific build options
option(ATOM_EXAMPLE_IMAGE_BUILD_ALL "Build all image examples" ON)
option(ATOM_EXAMPLE_IMAGE_CORE "Build core image examples" ${ATOM_EXAMPLE_IMAGE_BUILD_ALL})
option(ATOM_EXAMPLE_IMAGE_IO "Build I/O image examples" ${ATOM_EXAMPLE_IMAGE_BUILD_ALL})
option(ATOM_EXAMPLE_IMAGE_PROCESSING "Build processing image examples" ${ATOM_EXAMPLE_IMAGE_BUILD_ALL})
option(ATOM_EXAMPLE_IMAGE_FORMATS "Build format image examples" ${ATOM_EXAMPLE_IMAGE_BUILD_ALL})
option(ATOM_EXAMPLE_IMAGE_METADATA "Build metadata image examples" ${ATOM_EXAMPLE_IMAGE_BUILD_ALL})

# Feature detection for conditional compilation
if(ATOM_IMAGE_HAS_OPENCV)
    message(STATUS "OpenCV support detected for image examples")
endif()

if(ATOM_IMAGE_HAS_CFITSIO)
    message(STATUS "CFITSIO support detected for FITS examples")
endif()

if(ATOM_IMAGE_HAS_OCR)
    message(STATUS "OCR support detected for text recognition examples")
endif()

# Function to add image example with dependencies
function(add_image_example EXAMPLE_NAME SOURCE_FILE CATEGORY)
    string(TOUPPER ${CATEGORY} CATEGORY_UPPER)
    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)

    # Check if category is enabled
    if(ATOM_EXAMPLE_IMAGE_${CATEGORY_UPPER})
        set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

        # Individual example option
        option(ATOM_EXAMPLE_IMAGE_${EXAMPLE_NAME_UPPER}
               "Build image example: ${EXAMPLE_NAME}"
               ${ATOM_EXAMPLE_IMAGE_${CATEGORY_UPPER}})

        if(ATOM_EXAMPLE_IMAGE_${EXAMPLE_NAME_UPPER})
            add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
            target_link_libraries(${EXECUTABLE_NAME} atom)

            # Add conditional compilation flags
            if(ATOM_IMAGE_HAS_OPENCV)
                target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OPENCV)
            endif()

            if(ATOM_IMAGE_HAS_CFITSIO)
                target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_CFITSIO)
            endif()

            if(ATOM_IMAGE_HAS_OCR)
                target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OCR)
            endif()

            # Set IDE folder grouping
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Image/${CATEGORY}")

            message(STATUS "Added image example: ${EXECUTABLE_NAME}")
        endif()
    endif()
endfunction()

# Add subdirectory examples
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/core)
    add_subdirectory(core)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/io)
    add_subdirectory(io)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/processing)
    add_subdirectory(processing)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/formats)
    add_subdirectory(formats)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/metadata)
    add_subdirectory(metadata)
endif()

# Add root-level examples
file(GLOB ROOT_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ROOT_CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
    add_image_example(${EXAMPLE_NAME} ${CPP_FILE} "ROOT")
endforeach()
