cmake_minimum_required(VERSION 3.21)
project(
  atom-image
  VERSION 1.0.0
  LANGUAGES CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Find required packages
find_package(spdlog QUIET)

# Find optional packages
find_package(OpenCV REQUIRED) # Change to REQUIRED for computer vision
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(CFITSIO cfitsio)
  pkg_check_modules(TESSERACT tesseract)
  pkg_check_modules(LEPTONICA lept)
endif()

# Define source files
set(ATOM_IMAGE_SOURCES
    # Core sources
    core/image_metadata.cpp
    # Metadata sources
    metadata/exif.cpp
    # Format sources
    formats/fits_data.cpp
    formats/fits_file.cpp
    formats/fits_header.cpp
    formats/fits_utils.cpp
    formats/hdu.cpp
    formats/advanced_formats.cpp
    # I/O sources
    io/format_detector.cpp
    io/image_loader.cpp
    io/image_saver.cpp
    # Processing sources
    processing/image_processor.cpp
    processing/filters.cpp
    processing/transforms.cpp
    processing/enhancement.cpp
    processing/computer_vision.cpp # Uncommented
    processing/ml_processing.cpp
    processing/gpu_acceleration.cpp
    processing/realtime_stub.cpp)

# Define header files
set(ATOM_IMAGE_HEADERS
    # Main header
    image.hpp
    # Core headers
    core/image_blob.hpp
    core/image_metadata.hpp
    core/exceptions.hpp
    core/cache_manager.hpp
    # Metadata headers
    metadata/exif.hpp
    # Format headers
    formats/fits_data.hpp
    formats/fits_file.hpp
    formats/fits_header.hpp
    formats/fits_utils.hpp
    formats/hdu.hpp
    formats/advanced_formats.hpp
    # I/O headers
    io/format_detector.hpp
    io/image_loader.hpp
    io/image_saver.hpp
    # Processing headers
    processing/image_processor.hpp
    processing/filters.hpp
    processing/transforms.hpp
    processing/enhancement.hpp
    processing/computer_vision.hpp # Added
    processing/ml_processing.hpp
    processing/gpu_acceleration.hpp
    processing/realtime.hpp)

# Add OCR sources if Tesseract is available
if(TESSERACT_FOUND AND LEPTONICA_FOUND)
  list(APPEND ATOM_IMAGE_SOURCES processing/ocr/ocr.cpp)
  list(APPEND ATOM_IMAGE_HEADERS processing/ocr/ocr.hpp)
  set(ATOM_IMAGE_HAS_OCR ON)
endif()

# Add SER sources (requires OpenCV)
if(OpenCV_FOUND)
  list(
    APPEND
    ATOM_IMAGE_SOURCES
    formats/ser/ser_reader.cpp
    formats/ser/ser_writer.cpp
    formats/ser/frame_processor.cpp
    formats/ser/quality.cpp
    formats/ser/registration.cpp
    formats/ser/stacking.cpp
    formats/ser/utils.cpp)

  list(
    APPEND
    ATOM_IMAGE_HEADERS
    formats/ser/ser.hpp
    formats/ser/ser_reader.h
    formats/ser/ser_writer.h
    formats/ser/frame_processor.h
    formats/ser/quality.h
    formats/ser/registration.h
    formats/ser/ser_format.h
    formats/ser/stacking.h
    formats/ser/utils.h
    formats/ser/exception.h)
endif()

# Create library target
add_library(atom-image STATIC ${ATOM_IMAGE_SOURCES})

# Configure module using standardized function
atom_configure_module(atom-image)

# Logging and compression dependencies
atom_setup_logging_deps(atom-image)
atom_setup_compression_deps(atom-image)

# Link optional dependencies
if(OpenCV_FOUND)
  target_link_libraries(atom-image PUBLIC ${OpenCV_LIBS})
  target_include_directories(atom-image PUBLIC ${OpenCV_INCLUDE_DIRS})
  target_compile_definitions(atom-image PUBLIC ATOM_IMAGE_HAS_OPENCV)
endif()

if(CFITSIO_FOUND)
  target_link_libraries(atom-image PUBLIC ${CFITSIO_LIBRARIES})
  target_include_directories(atom-image PUBLIC ${CFITSIO_INCLUDE_DIRS})
  target_compile_definitions(atom-image PUBLIC ATOM_IMAGE_HAS_CFITSIO)
endif()

if(ATOM_IMAGE_HAS_OCR)
  target_link_libraries(atom-image PUBLIC ${TESSERACT_LIBRARIES}
                                          ${LEPTONICA_LIBRARIES})
  target_include_directories(atom-image PUBLIC ${TESSERACT_INCLUDE_DIRS}
                                               ${LEPTONICA_INCLUDE_DIRS})
  target_compile_definitions(atom-image PUBLIC ATOM_IMAGE_HAS_OCR)
endif()

# Install library target
install(
  TARGETS atom-image
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install headers
install(FILES ${ATOM_IMAGE_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/image)

# Export targets Per-module export and config packaging is disabled here to
# avoid cross-target export dependencies. Combined packaging is handled at
# higher level.
