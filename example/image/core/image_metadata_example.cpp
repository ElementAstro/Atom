/**
 * @file image_metadata_example.cpp
 * @brief Example demonstrating image metadata handling
 *
 * This example covers:
 * - Creating and managing image metadata
 * - Setting and retrieving metadata values
 * - Working with different data types
 * - Merging metadata from multiple sources
 * - Serializing and deserializing metadata
 * - Standard metadata keys
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <string>

#include "atom/image/core/image_metadata.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic metadata operations
 */
void demonstrateBasicMetadata() {
    cout << "\n=== Basic Metadata Operations ===\n";

    ImageMetadata metadata;

    // Set various types of metadata
    metadata.set(MetadataKeys::WIDTH, 1920);
    metadata.set(MetadataKeys::HEIGHT, 1080);
    metadata.set(MetadataKeys::FORMAT, string("JPEG"));
    metadata.set(MetadataKeys::COLOR_SPACE, string("sRGB"));
    metadata.set(MetadataKeys::BIT_DEPTH, 8);

    // Retrieve metadata
    auto width = metadata.get<int>(MetadataKeys::WIDTH);
    auto height = metadata.get<int>(MetadataKeys::HEIGHT);
    auto format = metadata.get<string>(MetadataKeys::FORMAT);

    if (width && height && format) {
        cout << "Image: " << *width << "x" << *height << " " << *format << "\n";
    }

    // Check if key exists
    if (metadata.has(MetadataKeys::COLOR_SPACE)) {
        cout << "Color space: "
             << *metadata.get<string>(MetadataKeys::COLOR_SPACE) << "\n";
    }

    // Get all keys
    auto keys = metadata.keys();
    cout << "Total metadata keys: " << keys.size() << "\n";
}

/**
 * @brief Demonstrate camera metadata
 */
void demonstrateCameraMetadata() {
    cout << "\n=== Camera Metadata ===\n";

    ImageMetadata metadata;

    // Camera information
    metadata.set(MetadataKeys::CAMERA_MAKE, string("Canon"));
    metadata.set(MetadataKeys::CAMERA_MODEL, string("EOS R5"));
    metadata.set(MetadataKeys::LENS_MODEL, string("RF 24-70mm F2.8"));

    // Exposure settings
    metadata.set(MetadataKeys::EXPOSURE_TIME, 1.0 / 250.0);
    metadata.set(MetadataKeys::F_NUMBER, 5.6);
    metadata.set(MetadataKeys::ISO, 400);
    metadata.set(MetadataKeys::FOCAL_LENGTH, 50.0);

    // Display camera info
    cout << "Camera: " << *metadata.get<string>(MetadataKeys::CAMERA_MAKE)
         << " " << *metadata.get<string>(MetadataKeys::CAMERA_MODEL) << "\n";
    cout << "Lens: " << *metadata.get<string>(MetadataKeys::LENS_MODEL) << "\n";
    cout << "Settings: ISO " << *metadata.get<int>(MetadataKeys::ISO) << ", f/"
         << *metadata.get<double>(MetadataKeys::F_NUMBER) << ", 1/"
         << (1.0 / *metadata.get<double>(MetadataKeys::EXPOSURE_TIME)) << "s\n";
}

/**
 * @brief Demonstrate metadata merging
 */
void demonstrateMetadataMerging() {
    cout << "\n=== Metadata Merging ===\n";

    // Original metadata
    ImageMetadata original;
    original.set(MetadataKeys::WIDTH, 1920);
    original.set(MetadataKeys::HEIGHT, 1080);
    original.set(MetadataKeys::FORMAT, string("JPEG"));

    // Additional metadata
    ImageMetadata additional;
    additional.set(MetadataKeys::ARTIST, string("John Doe"));
    additional.set(MetadataKeys::COPYRIGHT, string("© 2025"));
    additional.set(MetadataKeys::FORMAT, string("PNG"));  // Conflict

    cout << "Original format: " << *original.get<string>(MetadataKeys::FORMAT)
         << "\n";

    // Merge without overwriting
    original.merge(additional, false);
    cout << "After merge (no overwrite): "
         << *original.get<string>(MetadataKeys::FORMAT) << "\n";

    // Merge with overwriting
    original.merge(additional, true);
    cout << "After merge (with overwrite): "
         << *original.get<string>(MetadataKeys::FORMAT) << "\n";

    cout << "Total keys after merge: " << original.keys().size() << "\n";
}

/**
 * @brief Demonstrate metadata serialization
 */
void demonstrateMetadataSerialization() {
    cout << "\n=== Metadata Serialization ===\n";

    ImageMetadata metadata;
    metadata.set(MetadataKeys::WIDTH, 1920);
    metadata.set(MetadataKeys::HEIGHT, 1080);
    metadata.set(MetadataKeys::ARTIST, string("Photographer"));
    metadata.set(MetadataKeys::DESCRIPTION, string("Beautiful landscape"));

    // Convert to string
    string serialized = metadata.toString();
    cout << "Serialized metadata:\n" << serialized << "\n";

    // Parse from string
    ImageMetadata restored;
    if (restored.fromString(serialized)) {
        cout << "\nRestored metadata:\n";
        cout << "  Width: " << *restored.get<int>(MetadataKeys::WIDTH) << "\n";
        cout << "  Height: " << *restored.get<int>(MetadataKeys::HEIGHT)
             << "\n";
        cout << "  Artist: " << *restored.get<string>(MetadataKeys::ARTIST)
             << "\n";
    }
}

/**
 * @brief Demonstrate metadata removal and clearing
 */
void demonstrateMetadataManagement() {
    cout << "\n=== Metadata Management ===\n";

    ImageMetadata metadata;

    // Add several metadata items
    metadata.set("key1", string("value1"));
    metadata.set("key2", string("value2"));
    metadata.set("key3", string("value3"));

    cout << "Initial keys: " << metadata.keys().size() << "\n";

    // Remove specific key
    metadata.remove("key2");
    cout << "After removing key2: " << metadata.keys().size() << "\n";

    // Check if empty
    cout << "Is empty: " << (metadata.empty() ? "yes" : "no") << "\n";

    // Clear all
    metadata.clear();
    cout << "After clearing: " << metadata.keys().size() << "\n";
    cout << "Is empty: " << (metadata.empty() ? "yes" : "no") << "\n";
}

int main() {
    demonstrateBasicMetadata();
    demonstrateCameraMetadata();
    demonstrateMetadataMerging();
    demonstrateMetadataSerialization();
    demonstrateMetadataManagement();

    return 0;
}
