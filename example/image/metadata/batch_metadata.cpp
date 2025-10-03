/*
 * batch_metadata.cpp
 *
 * Batch metadata processing example
 * Part of the Atom project
 * Author: Max Qian
 * License: GPL3
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @brief Demonstrates batch metadata processing operations
 * 
 * This example shows how to process metadata for multiple images
 * in batch operations, which is essential for large astronomical
 * datasets and scientific image collections.
 */

struct ImageMetadata {
    std::string filename;
    std::string object_name;
    std::string filter;
    double exposure_time;
    int iso;
    double temperature;
    std::string timestamp;
    double ra;  // Right Ascension
    double dec; // Declination
    std::string telescope;
    std::string camera;
    int width;
    int height;
    int bit_depth;
    double airmass;
    double seeing;
    std::string processing_status;
};

int main() {
    std::cout << "Batch Metadata Processing Example\n";
    std::cout << "=================================\n\n";

    // Simulate a collection of astronomical images with metadata
    std::vector<ImageMetadata> image_collection = {
        {"M31_L_001.fits", "M31 Andromeda Galaxy", "Luminance", 300.0, 800, -10.0, "2024-10-03T22:30:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.15, 2.1, "Raw"},
        {"M31_L_002.fits", "M31 Andromeda Galaxy", "Luminance", 300.0, 800, -10.2, "2024-10-03T22:35:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.18, 2.3, "Raw"},
        {"M31_L_003.fits", "M31 Andromeda Galaxy", "Luminance", 300.0, 800, -10.1, "2024-10-03T22:40:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.22, 2.0, "Raw"},
        {"M31_R_001.fits", "M31 Andromeda Galaxy", "Red", 240.0, 800, -9.8, "2024-10-03T23:00:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.35, 2.4, "Raw"},
        {"M31_R_002.fits", "M31 Andromeda Galaxy", "Red", 240.0, 800, -9.9, "2024-10-03T23:05:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.38, 2.2, "Raw"},
        {"M31_G_001.fits", "M31 Andromeda Galaxy", "Green", 240.0, 800, -10.0, "2024-10-03T23:30:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.45, 2.5, "Raw"},
        {"M31_G_002.fits", "M31 Andromeda Galaxy", "Green", 240.0, 800, -10.1, "2024-10-03T23:35:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.48, 2.3, "Raw"},
        {"M31_B_001.fits", "M31 Andromeda Galaxy", "Blue", 300.0, 800, -10.2, "2024-10-04T00:00:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.52, 2.8, "Raw"},
        {"M31_B_002.fits", "M31 Andromeda Galaxy", "Blue", 300.0, 800, -10.0, "2024-10-04T00:05:00", 10.6847, 41.2687, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.55, 2.6, "Raw"},
        {"NGC7000_Ha_001.fits", "NGC 7000 North America Nebula", "H-alpha", 600.0, 800, -12.0, "2024-10-04T01:00:00", 312.25, 44.22, "EdgeHD 11", "ASI2600MM", 6248, 4176, 16, 1.25, 1.8, "Raw"}
    };

    std::cout << "1. Batch metadata loading:\n";
    std::cout << "   Loaded " << image_collection.size() << " images with metadata\n";
    std::cout << "   Sample entries:\n";
    for (size_t i = 0; i < std::min(size_t(3), image_collection.size()); ++i) {
        const auto& img = image_collection[i];
        std::cout << "   - " << img.filename << ": " << img.object_name 
                  << " (" << img.filter << ", " << img.exposure_time << "s)\n";
    }

    std::cout << "\n2. Metadata analysis and statistics:\n";
    
    // Group by object
    std::map<std::string, std::vector<ImageMetadata*>> objects;
    for (auto& img : image_collection) {
        objects[img.object_name].push_back(&img);
    }
    
    std::cout << "   Objects in collection:\n";
    for (const auto& [object, images] : objects) {
        std::cout << "   - " << object << ": " << images.size() << " images\n";
    }

    // Group by filter
    std::map<std::string, std::vector<ImageMetadata*>> filters;
    for (auto& img : image_collection) {
        filters[img.filter].push_back(&img);
    }
    
    std::cout << "\n   Filters used:\n";
    for (const auto& [filter, images] : filters) {
        double total_exposure = 0.0;
        for (const auto* img : images) {
            total_exposure += img->exposure_time;
        }
        std::cout << "   - " << filter << ": " << images.size() 
                  << " images, " << total_exposure << "s total exposure\n";
    }

    std::cout << "\n3. Quality assessment:\n";
    
    // Analyze seeing conditions
    double avg_seeing = 0.0;
    double min_seeing = image_collection[0].seeing;
    double max_seeing = image_collection[0].seeing;
    
    for (const auto& img : image_collection) {
        avg_seeing += img.seeing;
        min_seeing = std::min(min_seeing, img.seeing);
        max_seeing = std::max(max_seeing, img.seeing);
    }
    avg_seeing /= image_collection.size();
    
    std::cout << "   Seeing conditions:\n";
    std::cout << "   - Average: " << std::fixed << std::setprecision(1) << avg_seeing << " arcsec\n";
    std::cout << "   - Best: " << min_seeing << " arcsec\n";
    std::cout << "   - Worst: " << max_seeing << " arcsec\n";
    
    // Identify best frames
    std::vector<ImageMetadata*> sorted_by_seeing;
    for (auto& img : image_collection) {
        sorted_by_seeing.push_back(&img);
    }
    std::sort(sorted_by_seeing.begin(), sorted_by_seeing.end(),
              [](const ImageMetadata* a, const ImageMetadata* b) {
                  return a->seeing < b->seeing;
              });
    
    std::cout << "\n   Best seeing frames:\n";
    for (size_t i = 0; i < std::min(size_t(3), sorted_by_seeing.size()); ++i) {
        const auto* img = sorted_by_seeing[i];
        std::cout << "   - " << img->filename << ": " << img->seeing << " arcsec\n";
    }

    std::cout << "\n4. Batch metadata updates:\n";
    
    // Update processing status
    int processed_count = 0;
    for (auto& img : image_collection) {
        if (img.seeing <= 2.2) { // Good seeing threshold
            img.processing_status = "Selected for processing";
            processed_count++;
        } else {
            img.processing_status = "Rejected - poor seeing";
        }
    }
    
    std::cout << "   Updated processing status for all images\n";
    std::cout << "   Selected for processing: " << processed_count << " images\n";
    std::cout << "   Rejected: " << (image_collection.size() - processed_count) << " images\n";

    std::cout << "\n5. Metadata validation:\n";
    
    // Check for consistency
    bool validation_passed = true;
    std::string reference_telescope = image_collection[0].telescope;
    std::string reference_camera = image_collection[0].camera;
    
    for (const auto& img : image_collection) {
        if (img.telescope != reference_telescope) {
            std::cout << "   ⚠ Telescope mismatch in " << img.filename << std::endl;
            validation_passed = false;
        }
        if (img.camera != reference_camera) {
            std::cout << "   ⚠ Camera mismatch in " << img.filename << std::endl;
            validation_passed = false;
        }
        if (img.width != image_collection[0].width || img.height != image_collection[0].height) {
            std::cout << "   ⚠ Resolution mismatch in " << img.filename << std::endl;
            validation_passed = false;
        }
    }
    
    if (validation_passed) {
        std::cout << "   ✓ All images have consistent equipment and resolution\n";
    }

    std::cout << "\n6. Batch export operations:\n";
    
    // Export metadata to different formats
    std::cout << "   Exporting metadata to various formats:\n";
    
    // CSV export simulation
    std::cout << "   - CSV file: metadata_export.csv ✓\n";
    std::cout << "     Columns: filename, object, filter, exposure, iso, seeing, airmass\n";
    
    // JSON export simulation
    std::cout << "   - JSON file: metadata_export.json ✓\n";
    std::cout << "     Structured format with nested objects\n";
    
    // FITS header template
    std::cout << "   - FITS header template: fits_template.txt ✓\n";
    std::cout << "     Standard astronomical keywords\n";
    
    // Processing log
    std::cout << "   - Processing log: processing_log.txt ✓\n";
    std::cout << "     Detailed processing history\n";

    std::cout << "\n7. Automated processing workflow:\n";
    
    // Simulate automated processing based on metadata
    std::map<std::string, std::vector<ImageMetadata*>> processing_groups;
    
    for (auto& img : image_collection) {
        if (img.processing_status == "Selected for processing") {
            std::string group_key = img.object_name + "_" + img.filter;
            processing_groups[group_key].push_back(&img);
        }
    }
    
    std::cout << "   Processing groups created:\n";
    for (const auto& [group, images] : processing_groups) {
        std::cout << "   - " << group << ": " << images.size() << " images\n";
        
        // Calculate total exposure time for group
        double total_exposure = 0.0;
        for (const auto* img : images) {
            total_exposure += img->exposure_time;
        }
        std::cout << "     Total exposure: " << total_exposure << "s\n";
        
        // Update metadata for processed groups
        for (auto* img : images) {
            img->processing_status = "Queued for stacking";
        }
    }

    std::cout << "\n8. Metadata preservation:\n";
    std::cout << "   Original metadata preserved in processing\n";
    std::cout << "   Processing history added to metadata\n";
    std::cout << "   Calibration information recorded\n";
    std::cout << "   Quality metrics updated\n";
    std::cout << "   Batch processing parameters saved\n";

    std::cout << "\nBatch metadata processing completed successfully!\n";
    std::cout << "Key benefits of batch processing:\n";
    std::cout << "- Efficient handling of large datasets\n";
    std::cout << "- Consistent metadata validation\n";
    std::cout << "- Automated quality assessment\n";
    std::cout << "- Streamlined processing workflows\n";
    std::cout << "- Comprehensive data organization\n";

    return 0;
}
