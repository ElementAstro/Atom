/*
 * metadata_preservation.cpp
 *
 * Metadata preservation during image processing example
 * Part of the Atom project
 * Author: Max Qian
 * License: GPL3
 */

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <iomanip>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

/**
 * @brief Demonstrates metadata preservation during image processing
 * 
 * This example shows how to maintain and update metadata throughout
 * the image processing pipeline, ensuring traceability and reproducibility.
 */

struct ProcessingStep {
    std::string operation;
    std::string timestamp;
    std::map<std::string, std::string> parameters;
    std::string software_version;
};

struct ImageMetadata {
    // Original acquisition metadata
    std::map<std::string, std::string> acquisition;
    
    // Processing history
    std::vector<ProcessingStep> processing_history;
    
    // Current state
    std::map<std::string, std::string> current_state;
    
    // Quality metrics
    std::map<std::string, double> quality_metrics;
};

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void addProcessingStep(ImageMetadata& metadata, const std::string& operation,
                      const std::map<std::string, std::string>& parameters) {
    ProcessingStep step;
    step.operation = operation;
    step.timestamp = getCurrentTimestamp();
    step.parameters = parameters;
    step.software_version = "Atom Image Processing v1.0";
    
    metadata.processing_history.push_back(step);
    
    // Update current state
    metadata.current_state["last_operation"] = operation;
    metadata.current_state["last_processed"] = step.timestamp;
    metadata.current_state["processing_steps"] = std::to_string(metadata.processing_history.size());
}

int main() {
    std::cout << "Metadata Preservation During Image Processing\n";
    std::cout << "============================================\n\n";

    // Initialize image metadata with original acquisition data
    ImageMetadata metadata;
    
    // Original acquisition metadata
    metadata.acquisition = {
        {"filename", "M31_L_001.fits"},
        {"object_name", "M31 Andromeda Galaxy"},
        {"observer", "John Astronomer"},
        {"telescope", "Celestron EdgeHD 11"},
        {"camera", "ZWO ASI2600MM-Pro"},
        {"filter", "Luminance"},
        {"exposure_time", "300.0"},
        {"iso_gain", "139"},
        {"offset", "21"},
        {"binning", "1x1"},
        {"temperature", "-10.0"},
        {"date_obs", "2024-10-03T22:30:00"},
        {"ra", "00h42m44.3s"},
        {"dec", "+41d16m09s"},
        {"airmass", "1.15"},
        {"seeing", "2.1"},
        {"sky_background", "21.5"},
        {"humidity", "45%"},
        {"wind_speed", "5 km/h"},
        {"site_elevation", "1200m"},
        {"site_latitude", "45.123"},
        {"site_longitude", "-75.456"}
    };
    
    // Initial state
    metadata.current_state = {
        {"format", "FITS"},
        {"bit_depth", "16"},
        {"width", "6248"},
        {"height", "4176"},
        {"channels", "1"},
        {"compression", "none"},
        {"calibration_state", "raw"},
        {"processing_level", "0"}
    };
    
    // Initial quality metrics
    metadata.quality_metrics = {
        {"snr_estimate", 45.2},
        {"fwhm_pixels", 3.8},
        {"eccentricity", 0.15},
        {"background_level", 1250.0},
        {"saturation_level", 0.0}
    };

    std::cout << "1. Original image metadata:\n";
    std::cout << "   Object: " << metadata.acquisition["object_name"] << std::endl;
    std::cout << "   Exposure: " << metadata.acquisition["exposure_time"] << "s\n";
    std::cout << "   Filter: " << metadata.acquisition["filter"] << std::endl;
    std::cout << "   Date: " << metadata.acquisition["date_obs"] << std::endl;
    std::cout << "   Initial SNR: " << metadata.quality_metrics["snr_estimate"] << std::endl;

    std::cout << "\n2. Processing pipeline with metadata preservation:\n";

    // Step 1: Dark frame subtraction
    {
        std::map<std::string, std::string> params = {
            {"dark_frame", "master_dark_300s_-10C.fits"},
            {"scaling_method", "exposure_time"},
            {"interpolation", "linear"}
        };
        addProcessingStep(metadata, "dark_subtraction", params);
        
        metadata.current_state["calibration_state"] = "dark_subtracted";
        metadata.current_state["processing_level"] = "1";
        metadata.quality_metrics["background_level"] = 125.0; // Reduced after dark subtraction
        
        std::cout << "   ✓ Dark subtraction applied\n";
        std::cout << "     - Dark frame: " << params["dark_frame"] << std::endl;
        std::cout << "     - Background reduced to: " << metadata.quality_metrics["background_level"] << std::endl;
    }

    // Step 2: Flat field correction
    {
        std::map<std::string, std::string> params = {
            {"flat_frame", "master_flat_L_filter.fits"},
            {"normalization", "median"},
            {"vignetting_correction", "enabled"}
        };
        addProcessingStep(metadata, "flat_correction", params);
        
        metadata.current_state["calibration_state"] = "flat_corrected";
        metadata.current_state["processing_level"] = "2";
        metadata.quality_metrics["snr_estimate"] = 52.1; // Improved after flat correction
        
        std::cout << "   ✓ Flat field correction applied\n";
        std::cout << "     - Flat frame: " << params["flat_frame"] << std::endl;
        std::cout << "     - SNR improved to: " << metadata.quality_metrics["snr_estimate"] << std::endl;
    }

    // Step 3: Bias correction
    {
        std::map<std::string, std::string> params = {
            {"bias_frame", "master_bias.fits"},
            {"overscan_correction", "enabled"},
            {"trim_region", "50,50,6198,4126"}
        };
        addProcessingStep(metadata, "bias_correction", params);
        
        metadata.current_state["calibration_state"] = "fully_calibrated";
        metadata.current_state["processing_level"] = "3";
        metadata.current_state["width"] = "6198";  // After trimming
        metadata.current_state["height"] = "4126";
        
        std::cout << "   ✓ Bias correction and trimming applied\n";
        std::cout << "     - Image trimmed to: " << metadata.current_state["width"] 
                  << "x" << metadata.current_state["height"] << std::endl;
    }

    // Step 4: Cosmic ray removal
    {
        std::map<std::string, std::string> params = {
            {"algorithm", "L.A.Cosmic"},
            {"sigma_threshold", "5.0"},
            {"iterations", "3"},
            {"cosmic_rays_found", "127"}
        };
        addProcessingStep(metadata, "cosmic_ray_removal", params);
        
        metadata.quality_metrics["cosmic_ray_count"] = 127;
        metadata.quality_metrics["snr_estimate"] = 54.3; // Slight improvement
        
        std::cout << "   ✓ Cosmic ray removal completed\n";
        std::cout << "     - Cosmic rays removed: " << params["cosmic_rays_found"] << std::endl;
    }

    // Step 5: Registration and alignment
    {
        std::map<std::string, std::string> params = {
            {"reference_frame", "M31_L_001.fits"},
            {"algorithm", "triangle_matching"},
            {"transformation", "affine"},
            {"rms_error", "0.23"},
            {"matched_stars", "1847"}
        };
        addProcessingStep(metadata, "registration", params);
        
        metadata.quality_metrics["registration_rms"] = 0.23;
        metadata.quality_metrics["matched_stars"] = 1847;
        
        std::cout << "   ✓ Image registration completed\n";
        std::cout << "     - RMS error: " << params["rms_error"] << " pixels\n";
        std::cout << "     - Stars matched: " << params["matched_stars"] << std::endl;
    }

    // Step 6: Gradient removal
    {
        std::map<std::string, std::string> params = {
            {"method", "dynamic_background_extraction"},
            {"tolerance", "1.0"},
            {"sample_size", "15"},
            {"gradient_removed", "true"}
        };
        addProcessingStep(metadata, "gradient_removal", params);
        
        metadata.quality_metrics["background_uniformity"] = 0.95; // Improved uniformity
        
        std::cout << "   ✓ Background gradient removal applied\n";
        std::cout << "     - Background uniformity: " << metadata.quality_metrics["background_uniformity"] << std::endl;
    }

    std::cout << "\n3. Processing history summary:\n";
    std::cout << "   Total processing steps: " << metadata.processing_history.size() << std::endl;
    
    for (size_t i = 0; i < metadata.processing_history.size(); ++i) {
        const auto& step = metadata.processing_history[i];
        std::cout << "   " << (i+1) << ". " << step.operation 
                  << " (" << step.timestamp << ")\n";
    }

    std::cout << "\n4. Current image state:\n";
    std::cout << "   Processing level: " << metadata.current_state["processing_level"] << std::endl;
    std::cout << "   Calibration state: " << metadata.current_state["calibration_state"] << std::endl;
    std::cout << "   Dimensions: " << metadata.current_state["width"] 
              << "x" << metadata.current_state["height"] << std::endl;
    std::cout << "   Last operation: " << metadata.current_state["last_operation"] << std::endl;

    std::cout << "\n5. Quality metrics evolution:\n";
    std::cout << "   Final SNR: " << metadata.quality_metrics["snr_estimate"] << std::endl;
    std::cout << "   Background level: " << metadata.quality_metrics["background_level"] << std::endl;
    std::cout << "   Cosmic rays removed: " << metadata.quality_metrics["cosmic_ray_count"] << std::endl;
    std::cout << "   Registration RMS: " << metadata.quality_metrics["registration_rms"] << " pixels\n";

    std::cout << "\n6. Metadata export and preservation:\n";
    
    // Simulate saving metadata to various formats
    std::cout << "   Saving metadata to multiple formats:\n";
    std::cout << "   ✓ FITS header keywords updated\n";
    std::cout << "   ✓ XMP sidecar file created\n";
    std::cout << "   ✓ Processing log file generated\n";
    std::cout << "   ✓ JSON metadata file exported\n";
    std::cout << "   ✓ Database record updated\n";

    std::cout << "\n7. Traceability and reproducibility:\n";
    std::cout << "   Original acquisition parameters preserved\n";
    std::cout << "   Complete processing history recorded\n";
    std::cout << "   All calibration frames documented\n";
    std::cout << "   Software versions tracked\n";
    std::cout << "   Parameter values saved for each step\n";
    std::cout << "   Quality metrics evolution documented\n";

#ifdef HAVE_OPENCV
    std::cout << "\n8. Integration with image data:\n";
    try {
        // Simulate processed image
        cv::Mat processed_image = cv::Mat::zeros(512, 512, CV_16UC1);
        cv::randu(processed_image, cv::Scalar(100), cv::Scalar(4000));
        
        std::cout << "   Processed image dimensions: " << processed_image.cols 
                  << "x" << processed_image.rows << std::endl;
        std::cout << "   Metadata synchronized with image data\n";
        std::cout << "   Ready for final output with embedded metadata\n";
        
    } catch (const cv::Exception& e) {
        std::cerr << "   OpenCV error: " << e.what() << std::endl;
    }
#else
    std::cout << "\n8. OpenCV not available for image integration\n";
#endif

    std::cout << "\nMetadata preservation completed successfully!\n";
    std::cout << "Key benefits of metadata preservation:\n";
    std::cout << "- Complete processing traceability\n";
    std::cout << "- Reproducible results\n";
    std::cout << "- Quality assessment tracking\n";
    std::cout << "- Scientific documentation\n";
    std::cout << "- Automated workflow validation\n";

    return 0;
}
