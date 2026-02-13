/*
 * exif_metadata.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file exif_metadata.cpp
 * @brief EXIF metadata handling example
 *
 * This example demonstrates:
 * - EXIF data extraction
 * - Metadata modification
 * - GPS information handling
 * - Camera settings analysis
 * - Metadata preservation
 */

#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

/**
 * @brief EXIF data structure
 */
struct ExifData {
    // Camera information
    string make;
    string model;
    string software;

    // Image settings
    int width;
    int height;
    string orientation;
    int bits_per_sample;
    string color_space;

    // Camera settings
    string iso_speed;
    string aperture;
    string shutter_speed;
    string focal_length;
    string flash;
    string white_balance;
    string metering_mode;
    string exposure_mode;

    // Date/time
    string date_time_original;
    string date_time_digitized;

    // GPS information
    bool has_gps;
    double gps_latitude;
    double gps_longitude;
    double gps_altitude;
    string gps_latitude_ref;
    string gps_longitude_ref;

    // Additional metadata
    map<string, string> custom_fields;

    ExifData()
        : width(0),
          height(0),
          bits_per_sample(8),
          has_gps(false),
          gps_latitude(0.0),
          gps_longitude(0.0),
          gps_altitude(0.0) {}
};

/**
 * @brief EXIF reader class
 */
class ExifReader {
public:
    /**
     * @brief Read EXIF data from image file
     */
    static ExifData readExifData(const string& image_path) {
        cout << "Reading EXIF data from: " << image_path << endl;

        ExifData exif;

        // Simulate reading EXIF data (in real implementation, would use library
        // like libexif)
        exif.make = "Canon";
        exif.model = "EOS R5";
        exif.software = "Adobe Lightroom 6.0";

        exif.width = 8192;
        exif.height = 5464;
        exif.orientation = "Horizontal (normal)";
        exif.bits_per_sample = 14;
        exif.color_space = "sRGB";

        exif.iso_speed = "800";
        exif.aperture = "f/2.8";
        exif.shutter_speed = "1/250";
        exif.focal_length = "85mm";
        exif.flash = "Flash did not fire";
        exif.white_balance = "Auto";
        exif.metering_mode = "Spot";
        exif.exposure_mode = "Manual";

        exif.date_time_original = "2024:01:15 14:30:25";
        exif.date_time_digitized = "2024:01:15 14:30:25";

        // GPS data (if available)
        if (image_path.find("gps") != string::npos) {
            exif.has_gps = true;
            exif.gps_latitude = 37.7749;
            exif.gps_longitude = -122.4194;
            exif.gps_altitude = 16.0;
            exif.gps_latitude_ref = "N";
            exif.gps_longitude_ref = "W";
        }

        // Custom fields
        exif.custom_fields["Artist"] = "John Photographer";
        exif.custom_fields["Copyright"] = "© 2024 John Photographer";
        exif.custom_fields["ImageDescription"] = "Beautiful sunset landscape";
        exif.custom_fields["UserComment"] = "Shot during golden hour";

        cout << "  Successfully read EXIF data" << endl;
        return exif;
    }

    /**
     * @brief Print EXIF data in readable format
     */
    static void printExifData(const ExifData& exif) {
        cout << "\n=== EXIF Data ===" << endl;

        // Camera information
        cout << "Camera Information:" << endl;
        cout << "  Make: " << exif.make << endl;
        cout << "  Model: " << exif.model << endl;
        cout << "  Software: " << exif.software << endl;

        // Image properties
        cout << "\nImage Properties:" << endl;
        cout << "  Dimensions: " << exif.width << " x " << exif.height << endl;
        cout << "  Orientation: " << exif.orientation << endl;
        cout << "  Bits per sample: " << exif.bits_per_sample << endl;
        cout << "  Color space: " << exif.color_space << endl;

        // Camera settings
        cout << "\nCamera Settings:" << endl;
        cout << "  ISO Speed: " << exif.iso_speed << endl;
        cout << "  Aperture: " << exif.aperture << endl;
        cout << "  Shutter Speed: " << exif.shutter_speed << endl;
        cout << "  Focal Length: " << exif.focal_length << endl;
        cout << "  Flash: " << exif.flash << endl;
        cout << "  White Balance: " << exif.white_balance << endl;
        cout << "  Metering Mode: " << exif.metering_mode << endl;
        cout << "  Exposure Mode: " << exif.exposure_mode << endl;

        // Date/time
        cout << "\nDate/Time:" << endl;
        cout << "  Original: " << exif.date_time_original << endl;
        cout << "  Digitized: " << exif.date_time_digitized << endl;

        // GPS information
        if (exif.has_gps) {
            cout << "\nGPS Information:" << endl;
            cout << "  Latitude: " << fixed << setprecision(6)
                 << exif.gps_latitude << "° " << exif.gps_latitude_ref << endl;
            cout << "  Longitude: " << fixed << setprecision(6)
                 << exif.gps_longitude << "° " << exif.gps_longitude_ref
                 << endl;
            cout << "  Altitude: " << fixed << setprecision(1)
                 << exif.gps_altitude << " m" << endl;
        } else {
            cout << "\nGPS Information: Not available" << endl;
        }

        // Custom fields
        if (!exif.custom_fields.empty()) {
            cout << "\nCustom Fields:" << endl;
            for (const auto& field : exif.custom_fields) {
                cout << "  " << field.first << ": " << field.second << endl;
            }
        }
    }
};

/**
 * @brief EXIF writer class
 */
class ExifWriter {
public:
    /**
     * @brief Write EXIF data to image file
     */
    static bool writeExifData(const string& image_path, const ExifData& exif) {
        cout << "Writing EXIF data to: " << image_path << endl;

        // In real implementation, would use library to write EXIF data
        cout << "  Writing camera information..." << endl;
        cout << "  Writing image properties..." << endl;
        cout << "  Writing camera settings..." << endl;
        cout << "  Writing date/time information..." << endl;

        if (exif.has_gps) {
            cout << "  Writing GPS information..." << endl;
        }

        if (!exif.custom_fields.empty()) {
            cout << "  Writing custom fields..." << endl;
        }

        cout << "  EXIF data written successfully" << endl;
        return true;
    }

    /**
     * @brief Update specific EXIF fields
     */
    static bool updateExifFields(const string& image_path,
                                 const map<string, string>& updates) {
        cout << "Updating EXIF fields in: " << image_path << endl;

        for (const auto& update : updates) {
            cout << "  Updating " << update.first << " to: " << update.second
                 << endl;
        }

        cout << "  EXIF fields updated successfully" << endl;
        return true;
    }

    /**
     * @brief Remove EXIF data from image
     */
    static bool removeExifData(const string& image_path) {
        cout << "Removing EXIF data from: " << image_path << endl;
        cout << "  All EXIF data removed successfully" << endl;
        return true;
    }
};

/**
 * @brief EXIF analyzer class
 */
class ExifAnalyzer {
public:
    /**
     * @brief Analyze camera settings for photography insights
     */
    static void analyzeCameraSettings(const ExifData& exif) {
        cout << "\n=== Camera Settings Analysis ===" << endl;

        // Analyze exposure settings
        cout << "Exposure Analysis:" << endl;
        analyzeExposure(exif);

        // Analyze focus settings
        cout << "\nFocus Analysis:" << endl;
        analyzeFocus(exif);

        // Analyze image quality settings
        cout << "\nImage Quality Analysis:" << endl;
        analyzeImageQuality(exif);

        // Analyze shooting conditions
        cout << "\nShooting Conditions:" << endl;
        analyzeShootingConditions(exif);
    }

    /**
     * @brief Extract location information from GPS data
     */
    static void analyzeLocationData(const ExifData& exif) {
        cout << "\n=== Location Analysis ===" << endl;

        if (!exif.has_gps) {
            cout << "No GPS data available" << endl;
            return;
        }

        cout << "GPS Coordinates: " << fixed << setprecision(6)
             << exif.gps_latitude << ", " << exif.gps_longitude << endl;

        // Determine hemisphere
        string hemisphere = (exif.gps_latitude >= 0) ? "Northern" : "Southern";
        cout << "Hemisphere: " << hemisphere << endl;

        // Estimate timezone (very rough approximation)
        int timezone_offset = static_cast<int>(exif.gps_longitude / 15.0);
        cout << "Estimated timezone offset: UTC"
             << (timezone_offset >= 0 ? "+" : "") << timezone_offset << endl;

        // Altitude analysis
        if (exif.gps_altitude > 0) {
            cout << "Altitude: " << fixed << setprecision(1)
                 << exif.gps_altitude << " meters" << endl;
            if (exif.gps_altitude > 2000) {
                cout << "  High altitude location (may affect exposure)"
                     << endl;
            }
        }
    }

private:
    static void analyzeExposure(const ExifData& exif) {
        // Parse ISO
        int iso = 0;
        try {
            iso = stoi(exif.iso_speed);
        } catch (...) {
        }

        if (iso > 0) {
            cout << "  ISO " << iso << ": ";
            if (iso <= 100)
                cout << "Low noise, excellent quality";
            else if (iso <= 800)
                cout << "Good quality, minimal noise";
            else if (iso <= 3200)
                cout << "Moderate noise, acceptable quality";
            else
                cout << "High noise, consider noise reduction";
            cout << endl;
        }

        // Analyze aperture
        cout << "  Aperture " << exif.aperture << ": ";
        if (exif.aperture.find("1.4") != string::npos ||
            exif.aperture.find("2.8") != string::npos) {
            cout << "Wide aperture, shallow depth of field";
        } else if (exif.aperture.find("8") != string::npos ||
                   exif.aperture.find("11") != string::npos) {
            cout << "Narrow aperture, deep depth of field";
        } else {
            cout << "Moderate aperture, balanced depth of field";
        }
        cout << endl;

        // Analyze shutter speed
        cout << "  Shutter " << exif.shutter_speed << ": ";
        if (exif.shutter_speed.find("1/") != string::npos) {
            string speed_str = exif.shutter_speed.substr(2);
            try {
                int speed = stoi(speed_str);
                if (speed >= 500)
                    cout << "Fast shutter, freezes motion";
                else if (speed >= 60)
                    cout << "Moderate shutter, slight motion blur possible";
                else
                    cout << "Slow shutter, motion blur likely";
            } catch (...) {
                cout << "Custom shutter speed";
            }
        }
        cout << endl;
    }

    static void analyzeFocus(const ExifData& exif) {
        cout << "  Focal length: " << exif.focal_length << endl;

        // Parse focal length
        string fl_str = exif.focal_length;
        fl_str.erase(fl_str.find("mm"));
        try {
            int focal_length = stoi(fl_str);
            if (focal_length < 35) {
                cout
                    << "    Wide angle lens - good for landscapes, architecture"
                    << endl;
            } else if (focal_length <= 85) {
                cout << "    Standard lens - versatile for general photography"
                     << endl;
            } else if (focal_length <= 200) {
                cout << "    Telephoto lens - good for portraits, wildlife"
                     << endl;
            } else {
                cout << "    Super telephoto - specialized for distant subjects"
                     << endl;
            }
        } catch (...) {
        }
    }

    static void analyzeImageQuality(const ExifData& exif) {
        cout << "  Resolution: " << exif.width << "x" << exif.height << " ("
             << fixed << setprecision(1)
             << (exif.width * exif.height / 1000000.0) << " MP)" << endl;
        cout << "  Bit depth: " << exif.bits_per_sample << " bits per channel"
             << endl;
        cout << "  Color space: " << exif.color_space << endl;

        if (exif.bits_per_sample >= 14) {
            cout << "    High bit depth - excellent for post-processing"
                 << endl;
        } else if (exif.bits_per_sample >= 10) {
            cout << "    Good bit depth - suitable for moderate editing"
                 << endl;
        } else {
            cout << "    Standard bit depth - limited editing headroom" << endl;
        }
    }

    static void analyzeShootingConditions(const ExifData& exif) {
        cout << "  Flash: " << exif.flash << endl;
        cout << "  White balance: " << exif.white_balance << endl;
        cout << "  Metering mode: " << exif.metering_mode << endl;
        cout << "  Exposure mode: " << exif.exposure_mode << endl;

        if (exif.flash.find("did not fire") != string::npos) {
            cout << "    Natural light photography" << endl;
        }

        if (exif.white_balance == "Auto") {
            cout << "    Camera determined color temperature" << endl;
        }

        if (exif.exposure_mode == "Manual") {
            cout << "    Full manual control - experienced photographer"
                 << endl;
        }
    }
};

/**
 * @brief Demonstrate EXIF metadata handling
 */
void demonstrateExifHandling() {
    cout << "=== EXIF Metadata Handling Demo ===" << endl;

    // 1. Read EXIF data from different types of images
    cout << "\n1. Reading EXIF Data:" << endl;
    ExifData landscape_exif = ExifReader::readExifData("landscape_gps.jpg");
    ExifData portrait_exif = ExifReader::readExifData("portrait.jpg");

    // 2. Display EXIF data
    cout << "\n2. EXIF Data Display:" << endl;
    ExifReader::printExifData(landscape_exif);

    // 3. Analyze camera settings
    cout << "\n3. Camera Settings Analysis:" << endl;
    ExifAnalyzer::analyzeCameraSettings(landscape_exif);

    // 4. Analyze location data
    cout << "\n4. Location Data Analysis:" << endl;
    ExifAnalyzer::analyzeLocationData(landscape_exif);

    // 5. Modify EXIF data
    cout << "\n5. EXIF Data Modification:" << endl;
    map<string, string> updates = {{"Artist", "Updated Photographer"},
                                   {"Copyright", "© 2024 Updated Copyright"},
                                   {"ImageDescription", "Updated description"}};
    ExifWriter::updateExifFields("modified_image.jpg", updates);

    // 6. Write new EXIF data
    cout << "\n6. Writing New EXIF Data:" << endl;
    ExifData new_exif = portrait_exif;
    new_exif.custom_fields["ProcessingSoftware"] = "Atom Image Library";
    ExifWriter::writeExifData("processed_image.jpg", new_exif);

    // 7. Remove EXIF data (for privacy)
    cout << "\n7. EXIF Data Removal:" << endl;
    ExifWriter::removeExifData("privacy_image.jpg");
}

/**
 * @brief Main function
 */
int main() {
    try {
        cout << "EXIF Metadata Handling Example" << endl;
        cout << "==============================" << endl;

        demonstrateExifHandling();

        cout << "\nEXIF metadata handling demonstration completed!" << endl;
        return 0;

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
