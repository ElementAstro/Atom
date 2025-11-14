/**
 * @file fits_header_example.cpp
 * @brief Example demonstrating FITS header operations
 *
 * This example covers:
 * - Reading and writing header keywords
 * - Managing comments and history
 * - Header validation
 * - Keyword searching and iteration
 * - Header serialization
 * - Standard FITS keywords
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <string>

#ifdef ATOM_IMAGE_HAS_CFITSIO
#include "atom/image/formats/fits_header.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic header operations
 */
void demonstrateBasicHeaderOperations() {
    cout << "\n=== Basic Header Operations ===\n";

    FITSHeader header;

    // Set various keyword types
    header.setKeyword("SIMPLE", "T");
    header.setKeyword("BITPIX", "16");
    header.setKeyword("NAXIS", "2");
    header.setKeyword("NAXIS1", "1920");
    header.setKeyword("NAXIS2", "1080");
    header.setKeyword("OBJECT", "M31");
    header.setKeyword("EXPTIME", "300.0");

    cout << "Header size: " << header.size() << " keywords\n";
    cout << "Is empty: " << (header.empty() ? "Yes" : "No") << "\n";

    // Get keyword values
    auto object = header.getKeywordValue("OBJECT");
    auto exptime = header.getKeywordValue("EXPTIME");

    cout << "OBJECT: " << object << "\n";
    cout << "EXPTIME: " << exptime << "\n";
}

/**
 * @brief Demonstrate keyword management
 */
void demonstrateKeywordManagement() {
    cout << "\n=== Keyword Management ===\n";

    FITSHeader header;

    // Add keywords
    header.setKeyword("TELESCOP", "Hubble");
    header.setKeyword("INSTRUME", "WFC3");
    header.setKeyword("FILTER", "F814W");

    // Check if keyword exists
    if (header.hasKeyword("TELESCOP")) {
        cout << "TELESCOP keyword exists\n";
    }

    // Try to get keyword (safe)
    auto filter = header.tryGetKeywordValue("FILTER");
    if (filter) {
        cout << "FILTER: " << *filter << "\n";
    }

    // Get all keywords
    auto keywords = header.getAllKeywords();
    cout << "\nAll keywords:\n";
    for (const auto& keyword : keywords) {
        cout << "  " << keyword << " = " << header.getKeywordValue(keyword)
             << "\n";
    }

    // Remove keyword
    if (header.removeKeyword("FILTER")) {
        cout << "\nRemoved FILTER keyword\n";
    }

    cout << "Remaining keywords: " << header.size() << "\n";
}

/**
 * @brief Demonstrate comments and history
 */
void demonstrateCommentsAndHistory() {
    cout << "\n=== Comments and History ===\n";

    FITSHeader header;

    // Add comments
    header.addComment("This is a test FITS file");
    header.addComment("Created with Atom Image Library");
    header.addComment("Processing date: 2025-01-01");

    // Get all comments
    auto comments = header.getComments();
    cout << "Comments (" << comments.size() << "):\n";
    for (const auto& comment : comments) {
        cout << "  " << comment << "\n";
    }

    // Clear comments
    size_t removed = header.clearComments();
    cout << "\nRemoved " << removed << " comments\n";
}

/**
 * @brief Demonstrate header validation
 */
void demonstrateHeaderValidation() {
    cout << "\n=== Header Validation ===\n";

    FITSHeader header;

    // Set required keywords
    header.setKeyword("SIMPLE", "T");
    header.setKeyword("BITPIX", "16");
    header.setKeyword("NAXIS", "2");

    try {
        header.validate();
        cout << "Header is valid\n";
    } catch (const exception& e) {
        cout << "Header validation failed: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate header serialization
 */
void demonstrateHeaderSerialization() {
    cout << "\n=== Header Serialization ===\n";

    FITSHeader header;

    // Add some keywords
    header.setKeyword("SIMPLE", "T");
    header.setKeyword("BITPIX", "16");
    header.setKeyword("NAXIS", "2");
    header.setKeyword("OBJECT", "Test");

    // Serialize to bytes
    auto bytes = header.serialize();
    cout << "Serialized header size: " << bytes.size() << " bytes\n";
    cout << "Header units: " << bytes.size() / 2880 << "\n";

    // Deserialize
    FITSHeader newHeader;
    newHeader.deserialize(bytes);
    cout << "Deserialized header keywords: " << newHeader.size() << "\n";
}

/**
 * @brief Demonstrate astronomical metadata
 */
void demonstrateAstronomicalMetadata() {
    cout << "\n=== Astronomical Metadata ===\n";

    FITSHeader header;

    // Observation metadata
    header.setKeyword("TELESCOP", "VLT");
    header.setKeyword("INSTRUME", "SPHERE");
    header.setKeyword("OBJECT", "Beta Pictoris b");
    header.setKeyword("RA", "86.821");
    header.setKeyword("DEC", "-51.066");
    header.setKeyword("EQUINOX", "2000.0");

    // Exposure metadata
    header.setKeyword("EXPTIME", "60.0");
    header.setKeyword("DATE-OBS", "2025-01-01T12:00:00");
    header.setKeyword("FILTER", "H-band");

    // Observer metadata
    header.setKeyword("OBSERVER", "John Doe");
    header.setKeyword("ORIGIN", "ESO");

    cout << "Astronomical metadata added\n";
    cout << "Total keywords: " << header.size() << "\n";
}

int main() {
    demonstrateBasicHeaderOperations();
    demonstrateKeywordManagement();
    demonstrateCommentsAndHistory();
    demonstrateHeaderValidation();
    demonstrateHeaderSerialization();
    demonstrateAstronomicalMetadata();

    return 0;
}

#else
int main() {
    std::cout << "CFITSIO support not enabled. Rebuild with "
                 "ATOM_IMAGE_HAS_CFITSIO=ON\n";
    return 0;
}
#endif
