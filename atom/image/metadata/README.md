# Image Metadata Module

This directory contains functionality for reading, writing, and manipulating image metadata including EXIF data, IPTC information, and custom metadata formats.

## Overview

The metadata module provides comprehensive support for image metadata handling. It can extract, modify, and preserve metadata across different image formats and processing operations.

## Components

### EXIF Data (`exif.hpp/cpp`)

EXIF (Exchangeable Image File Format) data contains camera settings, timestamps, GPS coordinates, and other technical information embedded in image files.

#### Key Features

- **Complete EXIF Support**: All standard EXIF tags and extensions
- **GPS Data**: Location information extraction and manipulation
- **Camera Information**: Make, model, settings, lens information
- **Timestamp Handling**: Creation and modification dates
- **Thumbnail Extraction**: Embedded thumbnail images
- **Custom Tags**: Support for manufacturer-specific tags

#### Usage Examples

```cpp
#include "atom/image/metadata/exif.hpp"

// Reading EXIF data
ExifData exif = ExifData::fromFile("photo.jpg");

// Camera information
std::string camera = exif.getCameraMake() + " " + exif.getCameraModel();
std::string lens = exif.getLensModel();

// Shooting parameters
double aperture = exif.getAperture();
double shutter = exif.getShutterSpeed();
int iso = exif.getISO();
double focal_length = exif.getFocalLength();

// GPS information
if (exif.hasGPS()) {
    auto coords = exif.getGPSCoordinates();
    std::cout << "Latitude: " << coords.latitude << std::endl;
    std::cout << "Longitude: " << coords.longitude << std::endl;
}

// Timestamp
auto timestamp = exif.getDateTime();
std::cout << "Taken: " << timestamp << std::endl;

// Modifying EXIF data
exif.setArtist("Photographer Name");
exif.setCopyright("© 2025 Photographer");
exif.addComment("Processed with Atom Image Library");

// Writing back to file
exif.writeToFile("photo_modified.jpg");
```

#### Supported EXIF Tags

**Basic Information**
- Camera make and model
- Software used
- Date and time
- Image dimensions
- Color space

**Camera Settings**
- Aperture (F-number)
- Shutter speed
- ISO sensitivity
- Focal length
- Flash settings
- Exposure mode
- White balance
- Metering mode

**GPS Data**
- Latitude and longitude
- Altitude
- Direction
- Speed
- Timestamp

**Advanced Tags**
- Lens information
- Color profile
- Thumbnail data
- Manufacturer-specific tags

## Metadata Preservation

### During Processing

Metadata is automatically preserved during image processing operations:

```cpp
#include "atom/image/processing/image_processor.hpp"

ProcessingOptions options;
options.preserveMetadata = true;  // Default: true

auto processor = createOptimalProcessor();
processor->setOptions(options);

// Metadata is preserved during processing
auto resized = processor->resize(image, 800, 600);
// EXIF data is automatically transferred to the result
```

### Format Conversion

Metadata handling during format conversion:

```cpp
// Convert JPEG to PNG while preserving metadata
blob jpeg_image = blob::load("photo.jpg");
ExifData exif = ExifData::fromBlob(jpeg_image);

// Process image
auto processed = processor->enhance(jpeg_image);

// Save with metadata embedded in compatible format
processed.save("photo.png");
exif.embedInFile("photo.png");  // Add metadata to PNG
```

## Custom Metadata

### Application-Specific Tags

Support for custom metadata tags:

```cpp
ExifData exif;

// Add custom processing information
exif.setCustomTag("ProcessingVersion", "Atom Image 1.0");
exif.setCustomTag("ProcessingDate", getCurrentTimestamp());
exif.setCustomTag("ProcessingSettings", "sharpen=1.5,denoise=0.8");

// Quality metrics
exif.setCustomTag("QualityScore", "95.2");
exif.setCustomTag("NoiseLevel", "low");
```

### Batch Metadata Operations

Efficient metadata handling for multiple files:

```cpp
#include "atom/image/metadata/batch_metadata.hpp"

BatchMetadataProcessor batch;

// Add files to batch
batch.addFile("photo1.jpg");
batch.addFile("photo2.jpg");
batch.addFile("photo3.jpg");

// Apply metadata changes to all files
batch.setArtist("Photographer Name");
batch.setCopyright("© 2025");
batch.addKeyword("landscape");

// Process all files
batch.process();
```

## Metadata Validation

### Data Integrity

Validation of metadata consistency:

```cpp
ExifValidator validator;

// Validate EXIF data
auto result = validator.validate(exif);
if (!result.isValid()) {
    for (const auto& error : result.getErrors()) {
        std::cerr << "Validation error: " << error << std::endl;
    }
}

// Fix common issues
auto fixed_exif = validator.autoFix(exif);
```

### Privacy Protection

Remove sensitive metadata:

```cpp
ExifSanitizer sanitizer;

// Remove GPS and personal information
sanitizer.removeGPS(exif);
sanitizer.removePersonalInfo(exif);
sanitizer.removeCameraSerial(exif);

// Keep only essential technical data
auto sanitized = sanitizer.keepTechnicalOnly(exif);
```

## Performance Optimization

### Lazy Loading

Efficient metadata access for large collections:

```cpp
// Only load metadata when needed
ExifData exif = ExifData::fromFile("photo.jpg", ExifLoadMode::LAZY);

// Access specific tags without loading everything
std::string camera = exif.getCameraMake();  // Loads only camera info
```

### Caching

Metadata caching for repeated access:

```cpp
ExifCache cache;

// Cache metadata for frequently accessed files
cache.preload({"photo1.jpg", "photo2.jpg", "photo3.jpg"});

// Fast access from cache
auto exif = cache.get("photo1.jpg");
```

## Format Support

### EXIF-Compatible Formats
- **JPEG**: Full EXIF support
- **TIFF**: Complete metadata support
- **RAW**: Camera-specific metadata
- **HEIF**: Modern EXIF support

### Limited Support
- **PNG**: Text chunks for basic metadata
- **WebP**: XMP metadata support
- **BMP**: No standard metadata support

## Error Handling

Robust error handling for metadata operations:

```cpp
try {
    ExifData exif = ExifData::fromFile("photo.jpg");
    auto camera = exif.getCameraMake();
} catch (const ExifNotFoundException& e) {
    // No EXIF data found
} catch (const ExifCorruptedException& e) {
    // EXIF data is corrupted
} catch (const atom::error::Exception& e) {
    // General error
}
```

## Internationalization

Support for international metadata:

```cpp
// Unicode support for text fields
exif.setArtist("Фотограф");  // Cyrillic
exif.setComment("写真家");    // Japanese
exif.setCopyright("Fotógrafo"); // Accented characters

// Locale-aware date formatting
exif.setDateTime(timestamp, "en_US");
```

## Dependencies

### Required
- atom-error: Error handling framework
- atom-image-core: Core image functionality

### Optional
- libexif: Enhanced EXIF support
- exiv2: Advanced metadata library
- libiptc: IPTC metadata support

## Testing

Comprehensive metadata testing:

```bash
# Run EXIF tests
make test-exif

# Test with sample images
make test-metadata-samples

# Validate metadata integrity
make test-metadata-validation
```

## Contributing

When contributing to metadata handling:

1. Follow EXIF specification standards
2. Add tests for new metadata tags
3. Consider privacy implications
4. Document format-specific limitations
5. Ensure Unicode support for text fields
6. Test with various camera manufacturers
