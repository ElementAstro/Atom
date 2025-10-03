# Processing Image Examples
cmake_minimum_required(VERSION 3.10)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# Processing image examples
set(PROCESSING_EXAMPLES
    basic_filters
    geometric_transforms
    image_enhancement
    computer_vision
    ml_processing
    ocr_comprehensive
    realtime_processing
    gpu_acceleration
    gpu_acceleration_demo
)

# Function to add processing example
function(add_processing_example EXAMPLE_NAME)
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_NAME}.cpp)
    set(EXECUTABLE_NAME ${PARENT_DIR}_processing_${EXAMPLE_NAME})

    if(EXISTS ${SOURCE_FILE})
        add_executable(${EXECUTABLE_NAME} ${SOURCE_FILE})
        target_link_libraries(${EXECUTABLE_NAME} atom)

        # Add conditional compilation flags for processing backends
        if(ATOM_IMAGE_HAS_OPENCV)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OPENCV)
            target_link_libraries(${EXECUTABLE_NAME} ${OpenCV_LIBS})
        endif()

        if(ATOM_IMAGE_HAS_CIMG)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_CIMG)
        endif()

        # OCR support
        if(ATOM_IMAGE_HAS_OCR)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OCR)
            target_link_libraries(${EXECUTABLE_NAME} ${Tesseract_LIBRARIES})
        endif()

        # ML/AI support
        if(ATOM_IMAGE_HAS_TENSORFLOW)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_TENSORFLOW)
            target_link_libraries(${EXECUTABLE_NAME} ${TensorFlow_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_PYTORCH)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_PYTORCH)
            target_link_libraries(${EXECUTABLE_NAME} ${PyTorch_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_ONNX)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_ONNX)
            target_link_libraries(${EXECUTABLE_NAME} ${ONNX_LIBRARIES})
        endif()

        # GPU acceleration support
        if(ATOM_IMAGE_HAS_CUDA)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_CUDA)
            target_link_libraries(${EXECUTABLE_NAME} ${CUDA_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_OPENCL)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_OPENCL)
            target_link_libraries(${EXECUTABLE_NAME} ${OpenCL_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_VULKAN)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_VULKAN)
            target_link_libraries(${EXECUTABLE_NAME} ${Vulkan_LIBRARIES})
        endif()

        # Performance libraries
        if(ATOM_IMAGE_HAS_IPP)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_IPP)
            target_link_libraries(${EXECUTABLE_NAME} ${IPP_LIBRARIES})
        endif()

        if(ATOM_IMAGE_HAS_MKL)
            target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_IMAGE_HAS_MKL)
            target_link_libraries(${EXECUTABLE_NAME} ${MKL_LIBRARIES})
        endif()

        # Set IDE folder grouping
        set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Image/Processing")

        message(STATUS "Added processing image example: ${EXECUTABLE_NAME}")
    else()
        message(WARNING "Processing example source file not found: ${SOURCE_FILE}")
    endif()
endfunction()

# Add all processing examples
foreach(EXAMPLE ${PROCESSING_EXAMPLES})
    add_processing_example(${EXAMPLE})
endforeach()

# Add any additional CPP files in this directory
file(GLOB ADDITIONAL_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(CPP_FILE ${ADDITIONAL_CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Check if it's not already in the main list
    list(FIND PROCESSING_EXAMPLES ${EXAMPLE_NAME} FOUND_INDEX)
    if(FOUND_INDEX EQUAL -1)
        add_processing_example(${EXAMPLE_NAME})
    endif()
endforeach()
