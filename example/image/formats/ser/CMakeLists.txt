# SER Format Examples

# Check if OpenCV is available (required for SER format)
if(NOT TARGET opencv_core)
  message(STATUS "OpenCV not found, skipping SER format examples")
  return()
endif()

# ser_reader_example
add_executable(ser_reader_example ser_reader_example.cpp)
target_link_libraries(ser_reader_example PRIVATE atom-image)
set_target_properties(
  ser_reader_example
  PROPERTIES FOLDER "Examples/Image/Formats/SER"
             RUNTIME_OUTPUT_DIRECTORY
             "${CMAKE_BINARY_DIR}/bin/examples/image/formats/ser")

# ser_writer_example
add_executable(ser_writer_example ser_writer_example.cpp)
target_link_libraries(ser_writer_example PRIVATE atom-image)
set_target_properties(
  ser_writer_example
  PROPERTIES FOLDER "Examples/Image/Formats/SER"
             RUNTIME_OUTPUT_DIRECTORY
             "${CMAKE_BINARY_DIR}/bin/examples/image/formats/ser")

# frame_processor_example
add_executable(frame_processor_example frame_processor_example.cpp)
target_link_libraries(frame_processor_example PRIVATE atom-image)
set_target_properties(
  frame_processor_example
  PROPERTIES FOLDER "Examples/Image/Formats/SER"
             RUNTIME_OUTPUT_DIRECTORY
             "${CMAKE_BINARY_DIR}/bin/examples/image/formats/ser")

# quality_example
add_executable(quality_example quality_example.cpp)
target_link_libraries(quality_example PRIVATE atom-image)
set_target_properties(
  quality_example
  PROPERTIES FOLDER "Examples/Image/Formats/SER"
             RUNTIME_OUTPUT_DIRECTORY
             "${CMAKE_BINARY_DIR}/bin/examples/image/formats/ser")

# utils_example
add_executable(utils_example utils_example.cpp)
target_link_libraries(utils_example PRIVATE atom-image)
set_target_properties(
  utils_example
  PROPERTIES FOLDER "Examples/Image/Formats/SER"
             RUNTIME_OUTPUT_DIRECTORY
             "${CMAKE_BINARY_DIR}/bin/examples/image/formats/ser")
