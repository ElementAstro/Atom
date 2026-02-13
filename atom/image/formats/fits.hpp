/*
 * fits.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-24

Description: Unified FITS format support header

**************************************************/

#ifndef ATOM_IMAGE_FITS_HPP
#define ATOM_IMAGE_FITS_HPP

/**
 * @file fits.hpp
 * @brief Unified header for FITS (Flexible Image Transport System) support
 *
 * This header provides a single include point for all FITS-related
 * functionality including file operations, header handling, HDU processing, and
 * WCS support.
 *
 * FITS is the standard data format in astronomy for storing images, tables,
 * and metadata.
 */

// Core FITS functionality
#include "fits_data.hpp"
#include "fits_file.hpp"
#include "fits_header.hpp"

// HDU (Header Data Unit) support
#include "ascii_table_hdu.hpp"
#include "binary_table_hdu.hpp"
#include "hdu.hpp"

// Utilities and advanced features
#include "fits_compression.hpp"
#include "fits_image_types.hpp"
#include "fits_loader.hpp"
#include "fits_utils.hpp"
#include "fits_wcs.hpp"

// Calibration support
#include "calibration.hpp"

// Advanced format handling
#include "advanced_formats.hpp"

namespace atom::image::fits {

/**
 * @brief FITS module version information
 */
struct FITSVersion {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
    static constexpr const char* STRING = "1.0.0";
};

/**
 * @brief Get FITS module version
 */
[[nodiscard]] constexpr FITSVersion getVersion() { return FITSVersion{}; }

/**
 * @brief Check if CFITSIO library is available
 */
[[nodiscard]] constexpr bool hasCFITSIO() {
#ifdef ATOM_IMAGE_HAS_CFITSIO
    return true;
#else
    return false;
#endif
}

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_FITS_HPP
