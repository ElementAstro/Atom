# Image Formats Module

This directory contains support for various image file formats including standard formats (JPEG, PNG, TIFF) and specialized formats for scientific and astronomical applications.

## Overview

The formats module provides comprehensive support for reading, writing, and manipulating different image file formats. It includes both standard image formats and specialized formats like FITS (astronomical images) and SER (astronomical video sequences).

## Supported Formats

### Standard Formats

- **JPEG**: Lossy compression, widely supported
- **PNG**: Lossless compression, transparency support
- **TIFF**: Flexible format, multiple compression options
- **BMP**: Uncompressed bitmap format
- **TGA**: Targa format, gaming industry standard

### Specialized Formats

- **FITS**: Flexible Image Transport System (astronomy)
- **SER**: Simple Extensible Recorder (astronomical video)

## Components

### FITS Format Support

FITS (Flexible Image Transport System) is the standard format for astronomical images and data.

#### Files

- `fits_file.hpp/cpp`: Main FITS file handling
- `fits_header.hpp/cpp`: FITS header management
- `fits_data.hpp/cpp`: FITS data access and manipulation
- `fits_utils.hpp/cpp`: FITS utility functions
- `hdu.hpp/cpp`: Header Data Unit (HDU) management

#### Features

- **Multi-HDU Support**: Primary and extension HDUs
- **Data Types**: Support for all FITS data types
- **Compression**: Rice, GZIP, and other compression algorithms
- **Metadata**: Complete header keyword management
- **Memory Mapping**: Efficient access to large files
- **Async Operations**: Non-blocking I/O operations

#### Usage Examples

```cpp
#include "atom/image/formats/fits_file.hpp"

// Reading FITS files
FITSFile fits("telescope_data.fits");
auto& primary_hdu = fits.getHDU(0);

// Access image data
auto [width, height, channels] = primary_hdu.getImageSize();
float pixel = primary_hdu.getPixel<float>(x, y);

// Metadata access
std::string observer = primary_hdu.getHeaderKeyword<std::string>("OBSERVER");
double exposure = primary_hdu.getHeaderKeyword<double>("EXPTIME");

// Writing FITS files
primary_hdu.setHeaderKeyword("OBJECT", "M31");
primary_hdu.setPixel<float>(x, y, value);
fits.writeFITS("processed_data.fits");
```

### SER Format Support (`ser/`)

SER (Simple Extensible Recorder) is a format for astronomical video sequences.

#### Components

- **SERReader**: Reading SER video files
- **SERWriter**: Writing SER video files
- **FrameProcessor**: Frame processing and analysis
- **QualityAssessment**: Frame quality evaluation
- **Registration**: Frame alignment algorithms
- **Stacking**: Frame combination techniques

#### Features

- **High Performance**: Optimized for large video sequences
- **Frame Caching**: Intelligent caching for random access
- **Quality Assessment**: Automatic frame quality evaluation
- **Registration**: Sub-pixel frame alignment
- **Stacking**: Multiple stacking algorithms
- **Metadata**: Comprehensive frame metadata

#### Usage Examples

```cpp
#include "atom/image/formats/ser/ser.hpp"

// Reading SER files
SERReader reader("video.ser");
auto header = reader.getHeader();

// Read individual frames
for (uint32_t i = 0; i < header.frameCount; ++i) {
    auto frame = reader.readFrame(i);
    // Process frame...
}

// Frame processing
FrameProcessor processor;
auto aligned = processor.alignFrames(frame1, frame2);
auto stacked = processor.stackFrames(frames);

// Quality assessment
QualityAssessment qa;
auto metrics = qa.assessFrame(frame);
auto ranked = qa.rankFramesByQuality(frames);
```

## Format Detection

Automatic format detection based on file headers and extensions:

```cpp
#include "atom/image/formats/format_detector.hpp"

FormatDetector detector;
auto format = detector.detectFormat("image.fits");
// Returns ImageFormat::FITS

auto reader = detector.createReader(format);
auto image = reader->load("image.fits");
```

## Compression Support

### FITS Compression

- **Rice**: Lossless compression for integer data
- **GZIP**: General-purpose compression
- **HCOMPRESS**: Specialized astronomical image compression
- **PLIO**: Pixel list compression

### Standard Format Compression

- **JPEG**: Quality-based lossy compression
- **PNG**: Lossless compression with multiple levels
- **TIFF**: Multiple compression algorithms (LZW, ZIP, etc.)

## Memory Management

Efficient memory usage for large files:

```cpp
// Memory mapping for large FITS files
FITSFile fits("large_image.fits");
fits.enableMemoryMapping(true);

// Tiled access for huge images
auto& hdu = fits.getHDU(0);
hdu.enableTiledAccess(512, 512);

// Streaming access for SER files
SERReader reader("video.ser");
reader.enableStreaming(true);
```

## Metadata Handling

Comprehensive metadata support:

```cpp
// FITS metadata
auto& hdu = fits.getHDU(0);
hdu.setHeaderKeyword("OBSERVER", "John Doe");
hdu.setHeaderKeyword("EXPTIME", 300.0);
hdu.addComment("Processed with Atom Image Library");

// EXIF metadata (for standard formats)
ExifData exif = image.getExifData();
auto camera = exif.getCameraModel();
auto timestamp = exif.getTimestamp();
```

## Error Handling

Robust error handling for format-specific issues:

```cpp
try {
    FITSFile fits("data.fits");
    auto image = fits.getImageData();
} catch (const FITSFormatException& e) {
    // Handle FITS-specific errors
} catch (const atom::error::Exception& e) {
    // Handle general errors
}
```

## Performance Considerations

### Large File Handling

- Use memory mapping for files > 100MB
- Enable tiled access for images > 4K resolution
- Consider streaming for video sequences

### Batch Processing

- Process multiple files in parallel
- Use format-specific optimizations
- Cache frequently accessed metadata

## Format Conversion

Seamless conversion between formats:

```cpp
// FITS to standard format
FITSFile fits("data.fits");
auto image = fits.getImageData();
image.save("output.png");

// Standard format to FITS
blob image = blob::load("input.jpg");
FITSFile output("output.fits");
output.setImageData(image);
output.writeFITS();
```

## Dependencies

### Required

- atom-error: Error handling framework
- atom-image-core: Core image functionality

### Optional

- CFITSIO: FITS file I/O library
- OpenCV: Additional format support
- LibTIFF: Enhanced TIFF support
- LibJPEG: JPEG optimization

## Testing

Format-specific test coverage:

```bash
# Run FITS tests
make test-fits

# Run SER tests
make test-ser

# Run format conversion tests
make test-formats

# Test with sample data
make test-formats-data
```

## Contributing

When contributing format support:

1. Follow the existing format interface patterns
2. Add comprehensive error handling
3. Include format-specific tests
4. Document metadata handling
5. Consider performance implications for large files
6. Add format detection support
