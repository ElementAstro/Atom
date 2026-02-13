# CMakeLists.txt for atom_image Python bindings This module provides Python
# bindings for the atom::image C++ library

cmake_minimum_required(VERSION 3.14)

# Collect all source files from the new directory structure
set(ATOM_IMAGE_PYTHON_SOURCES
    # Main module
    image.cpp
    # Core module
    core/core.cpp
    # I/O module
    io/io.cpp
    # Formats module
    formats/formats.cpp
    # Metadata module
    metadata/metadata.cpp
    # Processing modules
    processing/filters/filters.cpp
    processing/transforms/transforms.cpp
    processing/enhancement/enhancement.cpp
    processing/cv/computer_vision.cpp
    processing/gpu/gpu_ml.cpp
    processing/realtime/realtime.cpp
    processing/ocr/ocr.cpp)

# Create the Python module
pybind11_add_module(atom_image ${ATOM_IMAGE_PYTHON_SOURCES})

# Set include directories
target_include_directories(atom_image PRIVATE ${CMAKE_SOURCE_DIR}
                                              ${CMAKE_SOURCE_DIR}/atom)

# Link to C++ library
if(TARGET atom-image)
  target_link_libraries(atom_image PRIVATE atom-image)
  message(STATUS "Linking atom_image to atom-image")
else()
  message(WARNING "Target atom-image not found for Python module image")
endif()

# Link to OpenCV if available
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
  target_link_libraries(atom_image PRIVATE ${OpenCV_LIBS})
  target_include_directories(atom_image PRIVATE ${OpenCV_INCLUDE_DIRS})
  message(STATUS "Linking atom_image to OpenCV")
endif()

# Link to spdlog and error handling
find_package(spdlog QUIET)
if(spdlog_FOUND)
  target_link_libraries(atom_image PRIVATE spdlog::spdlog)
endif()

if(TARGET atom-error)
  target_link_libraries(atom_image PRIVATE atom-error)
endif()

# Set output properties
set_target_properties(
  atom_image
  PROPERTIES PREFIX ""
             SUFFIX "${PYTHON_MODULE_EXTENSION}"
             LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Install rules
install(
  TARGETS atom_image
  LIBRARY DESTINATION python/image
  RUNTIME DESTINATION python/image)

# Install Python files
install(FILES __init__.py DESTINATION python/image)

# Install submodule __init__.py files
install(FILES core/__init__.py DESTINATION python/image/core)
install(FILES io/__init__.py DESTINATION python/image/io)
install(FILES formats/__init__.py DESTINATION python/image/formats)
install(FILES metadata/__init__.py DESTINATION python/image/metadata)
install(FILES processing/__init__.py DESTINATION python/image/processing)
install(FILES processing/filters/__init__.py
        DESTINATION python/image/processing/filters)
install(FILES processing/transforms/__init__.py
        DESTINATION python/image/processing/transforms)
install(FILES processing/enhancement/__init__.py
        DESTINATION python/image/processing/enhancement)
install(FILES processing/cv/__init__.py DESTINATION python/image/processing/cv)
install(FILES processing/gpu/__init__.py
        DESTINATION python/image/processing/gpu)
install(FILES processing/ml/__init__.py DESTINATION python/image/processing/ml)
install(FILES processing/realtime/__init__.py
        DESTINATION python/image/processing/realtime)
install(FILES processing/ocr/__init__.py
        DESTINATION python/image/processing/ocr)
