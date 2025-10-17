/**
 * @file comprehensive_fits.cpp
 * @brief Comprehensive example demonstrating FITS astronomical image processing
 *
 * This example covers:
 * - FITS file creation and manipulation
 * - HDU (Header Data Unit) operations
 * - Header keyword management
 * - Multi-extension FITS handling
 * - Astronomical image processing workflows
 * - Data type conversions and scaling
 * - World Coordinate System (WCS) handling
 * - FITS table operations
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/formats/fits_data.hpp"
#include "atom/image/formats/fits_file.hpp"
#include "atom/image/formats/fits_header.hpp"
#include "atom/image/formats/fits_utils.hpp"
#include "atom/image/formats/hdu.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Create a simulated astronomical image
 */
blob<float> createAstronomicalImage(int width, int height) {
    blob<float> img(height, width, 1);

    // Create a starfield with various astronomical objects
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float value = 100.0f;  // Sky background

            // Add random stars
            if (rand() % 1000 == 0) {
                value += 5000.0f + (rand() % 10000);  // Bright star
            } else if (rand() % 500 == 0) {
                value += 1000.0f + (rand() % 2000);  // Medium star
            } else if (rand() % 200 == 0) {
                value += 200.0f + (rand() % 500);  // Faint star
            }

            // Add a galaxy (elliptical profile)
            int galaxy_x = width / 3;
            int galaxy_y = height / 3;
            double dx = x - galaxy_x;
            double dy = y - galaxy_y;
            double galaxy_dist =
                std::sqrt(dx * dx + 2 * dy * dy);  // Elliptical
            if (galaxy_dist < 50) {
                value += 2000.0f * std::exp(-galaxy_dist / 20.0);
            }

            // Add a nebula (irregular structure)
            int nebula_x = 2 * width / 3;
            int nebula_y = 2 * height / 3;
            double nebula_dx = x - nebula_x;
            double nebula_dy = y - nebula_y;
            double nebula_dist =
                std::sqrt(nebula_dx * nebula_dx + nebula_dy * nebula_dy);
            if (nebula_dist < 80) {
                double noise =
                    0.5 + 0.5 * std::sin(x * 0.1) * std::cos(y * 0.1);
                value += 800.0f * std::exp(-nebula_dist / 40.0) * noise;
            }

            // Add noise
            value += (rand() % 20) - 10;  // ±10 ADU noise

            img.at(y, x, 0) = std::max(0.0f, value);
        }
    }

    return img;
}

/**
 * @brief Demonstrate basic FITS file operations
 */
void demonstrateBasicFITSOperations() {
    std::cout << "\n=== Basic FITS File Operations ===\n";

    try {
        // Create a new FITS file
        std::cout << "Creating new FITS file...\n";

        auto astronomical_image = createAstronomicalImage(512, 512);
        std::cout << "Created astronomical image: " << astronomical_image.cols()
                  << "x" << astronomical_image.rows() << "\n";

        // Create FITS file
        FITSFile fits;

        // Get primary HDU
        auto& primary_hdu = fits.getHDU(0);

        // Set image data
        primary_hdu.setImageData(astronomical_image);
        std::cout << "Set image data in primary HDU\n";

        // Get image information
        auto [width, height, channels] = primary_hdu.getImageSize();
        std::cout << "Image dimensions: " << width << "x" << height << " with "
                  << channels << " channel(s)\n";

        // Test pixel access
        float center_pixel = primary_hdu.getPixel<float>(width / 2, height / 2);
        std::cout << "Center pixel value: " << center_pixel << "\n";

        // Set a pixel value
        primary_hdu.setPixel<float>(10, 10, 65535.0f);
        float test_pixel = primary_hdu.getPixel<float>(10, 10);
        std::cout << "Set pixel (10,10) to 65535, read back: " << test_pixel
                  << "\n";

        // Calculate basic statistics
        float min_val = std::numeric_limits<float>::max();
        float max_val = std::numeric_limits<float>::min();
        double sum = 0.0;
        int count = 0;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float pixel = primary_hdu.getPixel<float>(x, y);
                min_val = std::min(min_val, pixel);
                max_val = std::max(max_val, pixel);
                sum += pixel;
                count++;
            }
        }

        double mean = sum / count;
        std::cout << "Image statistics: Min=" << min_val << ", Max=" << max_val
                  << ", Mean=" << std::fixed << std::setprecision(2) << mean
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic FITS operations: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate FITS header keyword management
 */
void demonstrateHeaderKeywords() {
    std::cout << "\n=== FITS Header Keyword Management ===\n";

    try {
        FITSFile fits;
        auto& primary_hdu = fits.getHDU(0);

        // Set standard astronomical keywords
        std::cout << "Setting standard astronomical keywords...\n";

        primary_hdu.setHeaderKeyword("OBJECT", "M31 Andromeda Galaxy");
        primary_hdu.setHeaderKeyword("OBSERVER", "Astronomer Name");
        primary_hdu.setHeaderKeyword("TELESCOP", "10-inch Reflector");
        primary_hdu.setHeaderKeyword("INSTRUME", "CCD Camera");
        primary_hdu.setHeaderKeyword("FILTER", "V");

        // Exposure information
        primary_hdu.setHeaderKeyword("EXPTIME", 300.0);
        primary_hdu.setHeaderKeyword("GAIN", 1.5);
        primary_hdu.setHeaderKeyword("RDNOISE", 8.2);
        primary_hdu.setHeaderKeyword("TEMP", -20.0);

        // Date and time
        primary_hdu.setHeaderKeyword("DATE-OBS", "2025-01-15T22:30:45");
        primary_hdu.setHeaderKeyword("UT", "22:30:45");

        // Coordinates
        primary_hdu.setHeaderKeyword("RA",
                                     10.6847);  // Right Ascension in degrees
        primary_hdu.setHeaderKeyword("DEC", 41.2687);  // Declination in degrees
        primary_hdu.setHeaderKeyword("EQUINOX", 2000.0);

        // Image scale
        primary_hdu.setHeaderKeyword("PIXSCALE", 1.2);     // arcsec/pixel
        primary_hdu.setHeaderKeyword("FOCALLEN", 2000.0);  // mm

        std::cout << "Set " << 15 << " header keywords\n";

        // Read back keywords
        std::cout << "\nReading back keywords:\n";

        std::string object =
            primary_hdu.getHeaderKeyword<std::string>("OBJECT");
        std::string observer =
            primary_hdu.getHeaderKeyword<std::string>("OBSERVER");
        double exptime = primary_hdu.getHeaderKeyword<double>("EXPTIME");
        double gain = primary_hdu.getHeaderKeyword<double>("GAIN");
        double ra = primary_hdu.getHeaderKeyword<double>("RA");
        double dec = primary_hdu.getHeaderKeyword<double>("DEC");

        std::cout << "  OBJECT: " << object << "\n";
        std::cout << "  OBSERVER: " << observer << "\n";
        std::cout << "  EXPTIME: " << exptime << " seconds\n";
        std::cout << "  GAIN: " << gain << " e-/ADU\n";
        std::cout << "  RA: " << std::fixed << std::setprecision(4) << ra
                  << " degrees\n";
        std::cout << "  DEC: " << std::fixed << std::setprecision(4) << dec
                  << " degrees\n";

        // Add comments to keywords
        primary_hdu.setHeaderKeyword("EXPTIME", 300.0,
                                     "Exposure time in seconds");
        primary_hdu.setHeaderKeyword("GAIN", 1.5,
                                     "CCD gain in electrons per ADU");
        primary_hdu.setHeaderKeyword("TEMP", -20.0,
                                     "CCD temperature in Celsius");

        std::cout << "Added comments to keywords\n";

        // Add custom keywords
        primary_hdu.setHeaderKeyword("SEEING", 2.1, "Seeing in arcseconds");
        primary_hdu.setHeaderKeyword("AIRMASS", 1.15, "Airmass at observation");
        primary_hdu.setHeaderKeyword("MOONPHASE", 0.23,
                                     "Moon phase (0=new, 1=full)");
        primary_hdu.setHeaderKeyword("WEATHER", "Clear", "Weather conditions");

        std::cout << "Added custom observing condition keywords\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in header keyword management: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate multi-extension FITS handling
 */
void demonstrateMultiExtensionFITS() {
    std::cout << "\n=== Multi-Extension FITS Handling ===\n";

    try {
        FITSFile fits;

        // Primary HDU (already exists)
        auto& primary_hdu = fits.getHDU(0);
        auto main_image = createAstronomicalImage(256, 256);
        primary_hdu.setImageData(main_image);
        primary_hdu.setHeaderKeyword("EXTNAME", "PRIMARY");
        primary_hdu.setHeaderKeyword("OBJECT", "Main Target");

        std::cout << "Set up primary HDU with main image\n";

        // Add image extensions
        std::cout << "Adding image extensions...\n";

        // Extension 1: Dark frame
        auto dark_frame = createAstronomicalImage(256, 256);
        // Simulate dark frame (mostly noise, no stars)
        for (int y = 0; y < dark_frame.rows(); ++y) {
            for (int x = 0; x < dark_frame.cols(); ++x) {
                dark_frame.at(y, x, 0) =
                    50.0f + (rand() % 20) - 10;  // Dark current + noise
            }
        }

        auto& dark_hdu = fits.addImageExtension(dark_frame);
        dark_hdu.setHeaderKeyword("EXTNAME", "DARK");
        dark_hdu.setHeaderKeyword("EXPTIME", 300.0);
        dark_hdu.setHeaderKeyword("TEMP", -20.0);

        // Extension 2: Flat field
        auto flat_field = createAstronomicalImage(256, 256);
        // Simulate flat field (smooth gradient)
        for (int y = 0; y < flat_field.rows(); ++y) {
            for (int x = 0; x < flat_field.cols(); ++x) {
                double dx = x - flat_field.cols() / 2.0;
                double dy = y - flat_field.rows() / 2.0;
                double vignetting =
                    1.0 - 0.3 * (dx * dx + dy * dy) /
                              (flat_field.cols() * flat_field.cols());
                flat_field.at(y, x, 0) = 30000.0f * vignetting + (rand() % 100);
            }
        }

        auto& flat_hdu = fits.addImageExtension(flat_field);
        flat_hdu.setHeaderKeyword("EXTNAME", "FLAT");
        flat_hdu.setHeaderKeyword("FILTER", "V");
        flat_hdu.setHeaderKeyword("EXPTIME", 10.0);

        // Extension 3: Bias frame
        auto bias_frame = createAstronomicalImage(256, 256);
        // Simulate bias frame (constant offset + noise)
        for (int y = 0; y < bias_frame.rows(); ++y) {
            for (int x = 0; x < bias_frame.cols(); ++x) {
                bias_frame.at(y, x, 0) =
                    1000.0f + (rand() % 10) - 5;  // Bias level + readout noise
            }
        }

        auto& bias_hdu = fits.addImageExtension(bias_frame);
        bias_hdu.setHeaderKeyword("EXTNAME", "BIAS");
        bias_hdu.setHeaderKeyword("EXPTIME", 0.0);

        std::cout << "Added 3 image extensions (DARK, FLAT, BIAS)\n";

        // List all HDUs
        int num_hdus = fits.getNumHDUs();
        std::cout << "\nFITS file contains " << num_hdus << " HDUs:\n";

        for (int i = 0; i < num_hdus; ++i) {
            auto& hdu = fits.getHDU(i);
            auto [width, height, channels] = hdu.getImageSize();

            std::string extname = "UNKNOWN";
            try {
                extname = hdu.getHeaderKeyword<std::string>("EXTNAME");
            } catch (...) {
                extname = (i == 0) ? "PRIMARY" : "EXTENSION";
            }

            std::cout << "  HDU " << i << ": " << extname << " (" << width
                      << "x" << height << ")\n";
        }

        // Access specific extensions by name
        std::cout << "\nAccessing extensions by name:\n";
        try {
            auto& dark_by_name = fits.getHDUByName("DARK");
            double dark_exptime =
                dark_by_name.getHeaderKeyword<double>("EXPTIME");
            std::cout << "  DARK extension exposure time: " << dark_exptime
                      << " seconds\n";

            auto& flat_by_name = fits.getHDUByName("FLAT");
            std::string flat_filter =
                flat_by_name.getHeaderKeyword<std::string>("FILTER");
            std::cout << "  FLAT extension filter: " << flat_filter << "\n";
        } catch (const std::exception& e) {
            std::cout << "  Note: Extension access by name not available in "
                         "this simulation\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in multi-extension FITS: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate astronomical image processing workflow
 */
void demonstrateAstronomicalProcessing() {
    std::cout << "\n=== Astronomical Image Processing Workflow ===\n";

    try {
        // Create calibration frames
        auto raw_image = createAstronomicalImage(200, 200);
        auto dark_frame = createAstronomicalImage(200, 200);
        auto flat_field = createAstronomicalImage(200, 200);
        auto bias_frame = createAstronomicalImage(200, 200);

        // Simulate calibration frames
        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 200; ++x) {
                // Dark frame: thermal noise
                dark_frame.at(y, x, 0) = 50.0f + (rand() % 10) - 5;

                // Bias frame: readout offset
                bias_frame.at(y, x, 0) = 1000.0f + (rand() % 5) - 2;

                // Flat field: illumination pattern
                double dx = x - 100.0;
                double dy = y - 100.0;
                double vignetting =
                    1.0 - 0.2 * (dx * dx + dy * dy) / (100 * 100);
                flat_field.at(y, x, 0) = 30000.0f * vignetting;
            }
        }

        std::cout << "Created calibration frames (raw, dark, flat, bias)\n";

        // Step 1: Bias subtraction
        std::cout << "\nStep 1: Bias subtraction\n";
        blob<float> bias_corrected(200, 200, 1);

        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 200; ++x) {
                bias_corrected.at(y, x, 0) =
                    raw_image.at(y, x, 0) - bias_frame.at(y, x, 0);
            }
        }

        // Step 2: Dark subtraction
        std::cout << "Step 2: Dark subtraction\n";
        blob<float> dark_corrected(200, 200, 1);

        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 200; ++x) {
                dark_corrected.at(y, x, 0) =
                    bias_corrected.at(y, x, 0) - dark_frame.at(y, x, 0);
            }
        }

        // Step 3: Flat field correction
        std::cout << "Step 3: Flat field correction\n";
        blob<float> calibrated(200, 200, 1);

        // Calculate flat field mean for normalization
        double flat_mean = 0.0;
        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 200; ++x) {
                flat_mean += flat_field.at(y, x, 0);
            }
        }
        flat_mean /= (200 * 200);

        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < 200; ++x) {
                float flat_normalized = flat_field.at(y, x, 0) / flat_mean;
                calibrated.at(y, x, 0) =
                    dark_corrected.at(y, x, 0) / flat_normalized;
            }
        }

        // Calculate improvement statistics
        double raw_noise = 0.0, calibrated_noise = 0.0;
        double raw_mean = 0.0, calibrated_mean = 0.0;

        for (int y = 50; y < 150; ++y) {  // Sample central region
            for (int x = 50; x < 150; ++x) {
                raw_mean += raw_image.at(y, x, 0);
                calibrated_mean += calibrated.at(y, x, 0);
            }
        }
        raw_mean /= (100 * 100);
        calibrated_mean /= (100 * 100);

        for (int y = 50; y < 150; ++y) {
            for (int x = 50; x < 150; ++x) {
                double raw_diff = raw_image.at(y, x, 0) - raw_mean;
                double cal_diff = calibrated.at(y, x, 0) - calibrated_mean;
                raw_noise += raw_diff * raw_diff;
                calibrated_noise += cal_diff * cal_diff;
            }
        }
        raw_noise = std::sqrt(raw_noise / (100 * 100));
        calibrated_noise = std::sqrt(calibrated_noise / (100 * 100));

        std::cout << "\nCalibration results:\n";
        std::cout << "  Raw image noise (RMS): " << std::fixed
                  << std::setprecision(2) << raw_noise << "\n";
        std::cout << "  Calibrated image noise (RMS): " << calibrated_noise
                  << "\n";
        std::cout << "  Noise reduction: " << (raw_noise / calibrated_noise)
                  << "x\n";

        // Step 4: Create FITS file with all frames
        std::cout << "\nStep 4: Saving processed data to FITS\n";

        FITSFile processed_fits;

        // Primary: calibrated image
        auto& primary = processed_fits.getHDU(0);
        primary.setImageData(calibrated);
        primary.setHeaderKeyword("EXTNAME", "CALIBRATED");
        primary.setHeaderKeyword("OBJECT", "Processed Target");
        primary.setHeaderKeyword("HISTORY", "Bias, dark, and flat corrected");
        primary.setHeaderKeyword("BUNIT", "ADU");
        primary.setHeaderKeyword("BZERO", 0.0);
        primary.setHeaderKeyword("BSCALE", 1.0);

        std::cout << "Saved calibrated image as primary HDU\n";
        std::cout << "Added processing history to header\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in astronomical processing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate FITS utilities and validation
 */
void demonstrateFITSUtilities() {
    std::cout << "\n=== FITS Utilities and Validation ===\n";

    try {
        // Test FITS validation
        std::cout << "Testing FITS file validation:\n";

        std::vector<std::string> test_files = {
            "valid_image.fits", "corrupted_header.fits",
            "missing_keywords.fits", "invalid_data.fits",
            "multi_extension.fits"};

        for (const auto& filename : test_files) {
            // Simulate validation results
            bool is_valid = (filename.find("valid") != std::string::npos ||
                             filename.find("multi") != std::string::npos);

            std::cout << "  " << filename << ": "
                      << (is_valid ? "VALID" : "INVALID");

            if (!is_valid) {
                if (filename.find("corrupted") != std::string::npos) {
                    std::cout << " (corrupted header)";
                } else if (filename.find("missing") != std::string::npos) {
                    std::cout << " (missing required keywords)";
                } else if (filename.find("invalid") != std::string::npos) {
                    std::cout << " (invalid data format)";
                }
            }
            std::cout << "\n";
        }

        // Test FITS information extraction
        std::cout << "\nExtracting FITS file information:\n";

        // Simulate file info
        struct FITSInfo {
            std::string filename;
            int width, height;
            std::string datatype;
            int num_extensions;
            std::string object;
            double exptime;
        };

        std::vector<FITSInfo> fits_files = {
            {"galaxy_m31.fits", 2048, 2048, "float32", 1, "M31", 600.0},
            {"calibration.fits", 1024, 1024, "uint16", 4, "Calibration", 0.0},
            {"deep_field.fits", 4096, 4096, "float32", 1, "Deep Field",
             3600.0}};

        std::cout << "Filename          | Dimensions | Type    | Ext | Object  "
                     "   | ExpTime\n";
        std::cout << "------------------|------------|---------|-----|---------"
                     "---|--------\n";

        for (const auto& info : fits_files) {
            std::cout << std::left << std::setw(17) << info.filename << " | "
                      << std::setw(10)
                      << (std::to_string(info.width) + "x" +
                          std::to_string(info.height))
                      << " | " << std::setw(7) << info.datatype << " | "
                      << std::setw(3) << info.num_extensions << " | "
                      << std::setw(10) << info.object << " | " << std::setw(6)
                      << info.exptime << "s\n";
        }

        // Test coordinate conversion utilities
        std::cout << "\nCoordinate conversion utilities:\n";

        // RA/Dec to pixel coordinates (simplified)
        double ra_deg = 10.6847;   // M31 RA
        double dec_deg = 41.2687;  // M31 Dec
        double pixel_scale = 1.2;  // arcsec/pixel
        int image_width = 2048;
        int image_height = 2048;

        // Simulate WCS conversion
        int pixel_x = image_width / 2 +
                      static_cast<int>((ra_deg - 10.0) * 3600.0 / pixel_scale);
        int pixel_y = image_height / 2 +
                      static_cast<int>((dec_deg - 41.0) * 3600.0 / pixel_scale);

        std::cout << "  RA/Dec (" << ra_deg << "°, " << dec_deg
                  << "°) -> Pixel (" << pixel_x << ", " << pixel_y << ")\n";

        // Pixel to RA/Dec conversion
        double converted_ra =
            10.0 + (pixel_x - image_width / 2) * pixel_scale / 3600.0;
        double converted_dec =
            41.0 + (pixel_y - image_height / 2) * pixel_scale / 3600.0;

        std::cout << "  Pixel (" << pixel_x << ", " << pixel_y
                  << ") -> RA/Dec (" << std::fixed << std::setprecision(4)
                  << converted_ra << "°, " << converted_dec << "°)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in FITS utilities: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Comprehensive FITS Example ===\n";
    std::cout << "This example demonstrates comprehensive FITS astronomical "
                 "image processing\n";

    // Seed random number generator for consistent results
    srand(42);

    // Run all demonstrations
    demonstrateBasicFITSOperations();
    demonstrateHeaderKeywords();
    demonstrateMultiExtensionFITS();
    demonstrateAstronomicalProcessing();
    demonstrateFITSUtilities();

    std::cout << "\n=== Comprehensive FITS example completed ===\n";
    std::cout << "\nNote: This example demonstrates the FITS API structure.\n";
    std::cout << "Actual FITS I/O requires CFITSIO library integration.\n";
    std::cout << "Enable with: cmake -DATOM_IMAGE_HAS_CFITSIO=ON\n";

    return 0;
}
