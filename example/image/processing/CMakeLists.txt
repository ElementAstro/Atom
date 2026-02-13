# Processing Image Examples
cmake_minimum_required(VERSION 3.21)

# Get parent directory name for executable prefix
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. NAME)

# Processing image examples
set(PROCESSING_EXAMPLES
    image_processor_example
    # filters  # Disabled: API mismatches with FilterParams (needs struct field
    # access instead of operator[]) geometric_transforms  # Disabled: API
    # mismatches (transpose, applyAffineTransform, crop, flip methods don't
    # exist) image_enhancement  # Disabled: API mismatches (adjustSaturation,
    # adjustVibrance methods don't exist) computer_vision           # Disabled:
    # align later ml_processing             # Disabled: requires thread/random
    # includes ocr_operations  # Disabled: requires API update to match current
    # Blob interface realtime_processing  # Disabled: missing camera_capture.hpp
    # header gpu_acceleration          # Disabled: demo only, optional
    # gpu_acceleration_demo     # Disabled: missing fields in GPUDeviceInfo
    # computer_vision_features  # Disabled: missing feature_detection.hpp header
    # batch_processing_demo  # Disabled: missing batch_processor.hpp header
    # performance_optimization_demo  # Disabled: missing
    # performance_profiler.hpp header
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
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_OPENCV)
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
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_TENSORFLOW)
      target_link_libraries(${EXECUTABLE_NAME} ${TensorFlow_LIBRARIES})
    endif()

    if(ATOM_IMAGE_HAS_PYTORCH)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_PYTORCH)
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
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_OPENCL)
      target_link_libraries(${EXECUTABLE_NAME} ${OpenCL_LIBRARIES})
    endif()

    if(ATOM_IMAGE_HAS_VULKAN)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_IMAGE_HAS_VULKAN)
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
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER
                                                    "Examples/Image/Processing")

    message(STATUS "Added processing image example: ${EXECUTABLE_NAME}")
  else()
    message(WARNING "Processing example source file not found: ${SOURCE_FILE}")
  endif()
endfunction()

# Add all processing examples
foreach(EXAMPLE ${PROCESSING_EXAMPLES})
  add_processing_example(${EXAMPLE})
endforeach()

# Add subdirectories
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ocr)
  add_subdirectory(ocr)
endif()

# Intentionally do not glob-add all .cpp; only curated examples are built
