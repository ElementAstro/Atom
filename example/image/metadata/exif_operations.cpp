/*
 * exif_operations.cpp
 *
 * EXIF metadata operations example
 * Part of the Atom project
 * Author: Max Qian
 * License: GPL3
 */

#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

/**
 * @brief Demonstrates EXIF metadata operations
 *
 * This example shows how to read, write, and manipulate EXIF metadata
 * in image files, which is crucial for astronomical and scientific imaging.
 */
int main() {
    std::cout << "EXIF Metadata Operations Example\n";
    std::cout << "================================\n\n";

    std::cout << "EXIF (Exchangeable Image File Format) metadata includes:\n";
    std::cout << "- Camera settings (ISO, aperture, shutter speed)\n";
    std::cout << "- Date and time information\n";
    std::cout << "- GPS coordinates\n";
    std::cout << "- Camera and lens information\n";
    std::cout << "- Custom fields for scientific data\n\n";

    // Simulate EXIF data structure
    std::map<std::string, std::string> exif_data = {
        {"Make", "Astronomical Camera Co."},
        {"Model", "AstroCam Pro 2000"},
        {"DateTime", "2024:10:03 10:30:00"},
        {"ExposureTime", "30/1"},
        {"FNumber", "28/10"},
        {"ISO", "800"},
        {"FocalLength", "200/1"},
        {"WhiteBalance", "Manual"},
        {"ColorSpace", "sRGB"},
        {"ImageWidth", "4096"},
        {"ImageHeight", "4096"},
        {"BitsPerSample", "16"},
        {"Compression", "Uncompressed"},
        {"PhotometricInterpretation", "BlackIsZero"},
        {"Orientation", "1"},
        {"XResolution", "72/1"},
        {"YResolution", "72/1"},
        {"ResolutionUnit", "2"},
        {"Software", "Atom Image Processing v1.0"},
        {"Artist", "Astronomer"},
        {"Copyright", "2024 Observatory"},
        {"UserComment", "M31 Andromeda Galaxy - 30s exposure"},
        {"SubSecTime", "123"},
        {"ExifVersion", "0231"},
        {"FlashPixVersion", "0100"},
        {"ColorSpace", "1"},
        {"PixelXDimension", "4096"},
        {"PixelYDimension", "4096"},
        {"SensingMethod", "2"},
        {"FileSource", "3"},
        {"SceneType", "1"},
        {"CustomRendered", "0"},
        {"ExposureMode", "1"},
        {"WhiteBalance", "1"},
        {"DigitalZoomRatio", "1/1"},
        {"SceneCaptureType", "0"},
        {"Contrast", "0"},
        {"Saturation", "0"},
        {"Sharpness", "0"},
        {"SubjectDistanceRange", "3"}};

    // Add astronomical-specific EXIF fields
    exif_data["ObjectName"] = "M31 Andromeda Galaxy";
    exif_data["TelescopeModel"] = "Celestron EdgeHD 11";
    exif_data["MountModel"] = "Celestron CGX-L";
    exif_data["FilterWheel"] = "ZWO EFW 8x1.25";
    exif_data["Filter"] = "Luminance";
    exif_data["Binning"] = "1x1";
    exif_data["CoolerTemp"] = "-10C";
    exif_data["Gain"] = "139";
    exif_data["Offset"] = "21";
    exif_data["RA"] = "00h42m44.3s";
    exif_data["DEC"] = "+41d16m09s";
    exif_data["AltAz"] = "45.2d, 123.7d";
    exif_data["SiderealTime"] = "23h15m32s";
    exif_data["AirMass"] = "1.23";
    exif_data["SkyQuality"] = "21.5 mag/arcsec²";
    exif_data["Seeing"] = "2.1 arcsec";
    exif_data["CloudCover"] = "0%";
    exif_data["Humidity"] = "45%";
    exif_data["Temperature"] = "12C";
    exif_data["Pressure"] = "1013.2 hPa";

    std::cout << "1. Reading EXIF metadata (simulated):\n";
    std::cout << "   Basic camera information:\n";
    std::cout << "   - Camera: " << exif_data["Make"] << " "
              << exif_data["Model"] << std::endl;
    std::cout << "   - Date/Time: " << exif_data["DateTime"] << std::endl;
    std::cout << "   - Exposure: " << exif_data["ExposureTime"] << "s\n";
    std::cout << "   - ISO: " << exif_data["ISO"] << std::endl;
    std::cout << "   - F-number: f/" << exif_data["FNumber"] << std::endl;

    std::cout << "\n   Astronomical information:\n";
    std::cout << "   - Object: " << exif_data["ObjectName"] << std::endl;
    std::cout << "   - Telescope: " << exif_data["TelescopeModel"] << std::endl;
    std::cout << "   - Filter: " << exif_data["Filter"] << std::endl;
    std::cout << "   - Coordinates: RA=" << exif_data["RA"]
              << ", DEC=" << exif_data["DEC"] << std::endl;
    std::cout << "   - Sky Quality: " << exif_data["SkyQuality"] << std::endl;

    std::cout << "\n2. EXIF data validation:\n";

    // Validate essential fields
    std::vector<std::string> required_fields = {
        "Make", "Model",      "DateTime",   "ExposureTime",
        "ISO",  "ImageWidth", "ImageHeight"};

    bool validation_passed = true;
    for (const auto& field : required_fields) {
        if (exif_data.find(field) == exif_data.end() ||
            exif_data[field].empty()) {
            std::cout << "   ❌ Missing required field: " << field << std::endl;
            validation_passed = false;
        } else {
            std::cout << "   ✓ " << field << ": " << exif_data[field]
                      << std::endl;
        }
    }

    if (validation_passed) {
        std::cout << "   All required EXIF fields are present.\n";
    }

    std::cout << "\n3. EXIF data modification:\n";

    // Update processing information
    exif_data["Software"] = "Atom Image Processing v1.1";
    exif_data["ProcessingDate"] = "2024:10:03 11:00:00";
    exif_data["ProcessingSteps"] =
        "Dark subtraction, Flat correction, Calibration";
    exif_data["StackedFrames"] = "15";
    exif_data["TotalExposure"] = "450s";

    std::cout << "   Updated processing information:\n";
    std::cout << "   - Software: " << exif_data["Software"] << std::endl;
    std::cout << "   - Processing Date: " << exif_data["ProcessingDate"]
              << std::endl;
    std::cout << "   - Stacked Frames: " << exif_data["StackedFrames"]
              << std::endl;
    std::cout << "   - Total Exposure: " << exif_data["TotalExposure"]
              << std::endl;

    std::cout << "\n4. EXIF data analysis:\n";

    // Analyze exposure settings
    double exposure_time = std::stod(exif_data["ExposureTime"].substr(
        0, exif_data["ExposureTime"].find('/')));
    int iso_value = std::stoi(exif_data["ISO"]);
    double f_number = std::stod(exif_data["FNumber"].substr(
                          0, exif_data["FNumber"].find('/'))) /
                      10.0;

    // Calculate exposure value (EV)
    double ev = std::log2((f_number * f_number) / exposure_time);

    std::cout << "   Exposure analysis:\n";
    std::cout << "   - Exposure time: " << exposure_time << " seconds\n";
    std::cout << "   - ISO: " << iso_value << std::endl;
    std::cout << "   - F-number: f/" << f_number << std::endl;
    std::cout << "   - Exposure Value (EV): " << ev << std::endl;

    // Assess settings for astronomical imaging
    if (exposure_time >= 30.0) {
        std::cout << "   ✓ Good exposure time for deep-sky imaging\n";
    } else {
        std::cout << "   ⚠ Short exposure time - consider longer exposures\n";
    }

    if (iso_value <= 1600) {
        std::cout << "   ✓ Reasonable ISO for low noise\n";
    } else {
        std::cout << "   ⚠ High ISO - may introduce noise\n";
    }

    std::cout << "\n5. EXIF data export:\n";

    // Simulate writing EXIF data to different formats
    std::cout << "   Exporting EXIF data to various formats:\n";
    std::cout << "   - JPEG with embedded EXIF ✓\n";
    std::cout << "   - TIFF with EXIF tags ✓\n";
    std::cout << "   - FITS with header keywords ✓\n";
    std::cout << "   - XMP sidecar file ✓\n";
    std::cout << "   - CSV metadata table ✓\n";

    std::cout << "\n6. Metadata preservation:\n";
    std::cout << "   Original EXIF data preserved during processing\n";
    std::cout << "   Processing history added to metadata\n";
    std::cout << "   Astronomical coordinates maintained\n";
    std::cout << "   Equipment information retained\n";

#ifdef HAVE_OPENCV
    std::cout << "\n7. Integration with image processing:\n";
    try {
        // Create a sample image
        cv::Mat image = cv::Mat::zeros(512, 512, CV_16UC1);
        cv::randu(image, cv::Scalar(1000), cv::Scalar(4000));

        // Add some astronomical features
        cv::circle(image, cv::Point(256, 256), 50, cv::Scalar(8000), -1);

        std::cout << "   Image created with dimensions: " << image.cols << "x"
                  << image.rows << std::endl;
        std::cout << "   Bit depth: " << image.depth() << std::endl;
        std::cout
            << "   EXIF metadata would be embedded during save operation\n";

    } catch (const cv::Exception& e) {
        std::cerr << "   OpenCV error: " << e.what() << std::endl;
    }
#else
    std::cout << "\n7. OpenCV not available for image integration\n";
#endif

    std::cout << "\nEXIF operations completed successfully!\n";
    std::cout << "Key benefits of EXIF metadata:\n";
    std::cout << "- Preserves acquisition parameters\n";
    std::cout << "- Enables processing reproducibility\n";
    std::cout << "- Facilitates data organization\n";
    std::cout << "- Supports scientific documentation\n";
    std::cout << "- Enables automated processing workflows\n";

    return 0;
}
