#ifndef ATOM_IMAGE_HPP
#define ATOM_IMAGE_HPP

/**
 * @file image.hpp
 * @brief Comprehensive image processing library for the Atom framework
 *
 * This header provides a unified interface to all image processing
 * functionality including format support, processing operations, and metadata
 * handling.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

// Exception types
#include "exceptions.hpp"

// Core image functionality
#include "core/image_blob.hpp"

// Image processing operations
#include "processing/computer_vision.hpp"
#include "processing/enhancement.hpp"
#include "processing/filters.hpp"
#include "processing/gpu_acceleration.hpp"
#include "processing/image_processor.hpp"
#include "processing/ml_processing.hpp"
#include "processing/realtime.hpp"
#include "processing/transforms.hpp"

#ifdef ATOM_IMAGE_HAS_OCR
#include "processing/ocr/ocr.hpp"
#endif

// Format support - unified FITS header
#include "formats/fits.hpp"

#ifdef ATOM_IMAGE_HAS_OPENCV
#include "formats/ser/frame_processor.h"
#include "formats/ser/quality.h"
#include "formats/ser/ser.hpp"
#include "formats/ser/ser_reader.h"
#include "formats/ser/ser_writer.h"
#endif

// Metadata handling
#include "metadata/exif.hpp"

/**
 * @namespace atom::image
 * @brief Main namespace for image processing functionality
 */
namespace atom::image {

/**
 * @brief Version information for the image processing module
 */
struct Version {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
    static constexpr const char* STRING = "1.0.0";
};

/**
 * @brief Feature availability flags
 */
struct Features {
#ifdef ATOM_IMAGE_HAS_OPENCV
    static constexpr bool HAS_OPENCV = true;
#else
    static constexpr bool HAS_OPENCV = false;
#endif

#ifdef ATOM_IMAGE_HAS_CFITSIO
    static constexpr bool HAS_CFITSIO = true;
#else
    static constexpr bool HAS_CFITSIO = false;
#endif

#ifdef ATOM_IMAGE_HAS_OCR
    static constexpr bool HAS_OCR = true;
#else
    static constexpr bool HAS_OCR = false;
#endif
};

/**
 * @brief Initialize the image processing module
 * @return true if initialization was successful
 */
bool initialize();

/**
 * @brief Cleanup the image processing module
 */
void cleanup();

/**
 * @brief Get module version information
 * @return Version structure with version details
 */
[[nodiscard]] constexpr Version getVersion() { return Version{}; }

/**
 * @brief Get available features
 * @return Features structure with capability flags
 */
[[nodiscard]] constexpr Features getFeatures() { return Features{}; }

}  // namespace atom::image

#endif  // ATOM_IMAGE_HPP
