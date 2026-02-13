# OCR Processing Examples

# Check if Tesseract is available
if(NOT ATOM_IMAGE_HAS_TESSERACT)
  message(STATUS "Tesseract not found, skipping OCR examples")
  return()
endif()

# ocr_example
add_executable(ocr_example ocr_example.cpp)
target_link_libraries(ocr_example PRIVATE atom-image)
set_target_properties(
  ocr_example
  PROPERTIES FOLDER "Examples/Image/Processing/OCR"
             RUNTIME_OUTPUT_DIRECTORY
             "${CMAKE_BINARY_DIR}/bin/examples/image/processing/ocr")
