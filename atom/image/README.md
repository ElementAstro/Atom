# Atom Image Processing Module

A comprehensive, high-performance image processing library with support for multiple formats, advanced algorithms, and specialized astronomical image processing capabilities.

## Features

### 🚀 **Core Capabilities**

- **Universal Image Blob**: Template-based image container with support for multiple backends
- **FITS Processing**: Complete astronomical image format support with HDU management
- **OCR Integration**: Advanced optical character recognition with preprocessing
- **SER Format**: Specialized support for astronomical video sequences
- **Format Conversion**: Comprehensive support for common image formats
- **Performance Optimization**: Memory pooling and SIMD-optimized operations

### 📸 **Supported Formats**

- **Standard Formats**: JPEG, PNG, TIFF, BMP, TGA
- **Astronomical**: FITS (Flexible Image Transport System)
- **Video Sequences**: SER (Simple Extensible Recorder)
- **Raw Data**: Binary blobs with metadata

### 🔧 **Image Processing Operations**

- **Transformations**: Resize, rotate, crop, flip, perspective correction
- **Filtering**: Blur, sharpen, edge detection, noise reduction
- **Enhancement**: Histogram equalization, contrast adjustment, gamma correction
- **Analysis**: Statistical analysis, quality assessment, feature detection

## Architecture

### Core Components

```text
atom/image/
├── image_blob.hpp          # Universal image container
├── fits_*.{hpp,cpp}        # FITS format support
├── exif.{hpp,cpp}          # EXIF metadata handling
├── ocr/                    # OCR processing
│   ├── ocr.{hpp,cpp}      # Main OCR implementation
│   └── install_ocr_dependencies.sh
└── ser/                    # SER format support
    ├── ser.hpp            # Main SER interface
    ├── ser_reader.{h,cpp} # SER file reading
    ├── ser_writer.{h,cpp} # SER file writing
    ├── frame_processor.{h,cpp}
    ├── quality.{h,cpp}    # Quality assessment
    ├── registration.h     # Image registration
    ├── stacking.h         # Image stacking
    └── utils.{h,cpp}      # Utilities
```

### Memory Management

- **Blob Modes**: Normal (owned) and Fast (view-only) modes
- **Memory Pools**: Efficient allocation for frequent operations
- **Cache-Friendly**: 64-byte alignment for optimal performance
- **RAII**: Automatic resource management

### Error Handling

- **Exception Safety**: Strong exception safety guarantees
- **Error Categories**: Specialized error types for different operations
- **Validation**: Comprehensive input validation and bounds checking

## Dependencies

### Required

- **atom-error**: Error handling framework
- **loguru**: Logging system

### Optional

- **OpenCV**: Computer vision operations
- **CFITSIO**: FITS file I/O
- **Tesseract + Leptonica**: OCR capabilities
- **CImg**: Alternative image processing backend
- **stb_image**: Lightweight image I/O

## Build Configuration

### CMake

```cmake
find_package(atom-image REQUIRED)
target_link_libraries(your_target atom::image)
```

### XMake

```lua
add_requires("atom-image")
target("your_target")
    add_packages("atom-image")
```

### Build Options

- `ATOM_IMAGE_HAS_OPENCV`: Enable OpenCV integration
- `ATOM_IMAGE_HAS_CFITSIO`: Enable FITS support
- `ATOM_IMAGE_HAS_OCR`: Enable OCR capabilities

## Usage Examples

### Basic Image Operations

```cpp
#include "atom/image/image_blob.hpp"

using namespace atom::image;

// Load image
blob img = blob::load("image.jpg");

// Basic operations
img.resize(800, 600);
img.rotate(90);
img.flip(1);  // Horizontal flip

// Save result
img.save("processed.jpg");
```

### FITS Astronomical Images

```cpp
#include "atom/image/fits_file.hpp"

FITSFile fits("telescope_image.fits");
auto& hdu = fits.getHDU(0);

// Access image data
auto [width, height, channels] = hdu.getImageSize();
float pixel_value = hdu.getPixel<float>(100, 100);

// Add metadata
hdu.setHeaderKeyword("OBSERVER", "Astronomer Name");
hdu.setHeaderKeyword("EXPOSURE", "300.0");

fits.writeFITS("processed_image.fits");
```

### OCR Text Recognition

```cpp
#include "atom/image/ocr/ocr.hpp"

OCRConfig config;
config.language = "eng";
config.enableDeskew = true;
config.enableSpellCheck = true;

EnhancedOCRProcessor ocr(config);
auto result = ocr.processImage(image);

std::cout << "Text: " << result.text << std::endl;
std::cout << "Confidence: " << result.confidence << std::endl;
```

### SER Video Processing

```cpp
#include "atom/image/ser/ser.hpp"

SERReader reader("video.ser");
auto header = reader.getHeader();

for (size_t i = 0; i < header.frameCount; ++i) {
    auto frame = reader.readFrame(i);
    // Process frame...
}
```

## Performance Considerations

### Memory Usage

- Use `fast_blob` for view-only operations
- Enable memory pooling for frequent allocations
- Consider image dimensions for cache efficiency

### Processing Speed

- Leverage SIMD operations when available
- Use parallel processing for batch operations
- Cache frequently accessed data

### Quality vs Speed

- Adjust OCR preprocessing parameters
- Use appropriate image compression
- Balance accuracy and performance needs

## Testing

Comprehensive test suite covering:

- Image blob operations and format conversion
- FITS file I/O and metadata handling
- OCR accuracy and performance
- SER format compatibility
- Memory management and error handling

Run tests:

```bash
# CMake
ctest --test-dir build

# XMake
xmake test
```

## Contributing

1. Follow the existing code style and patterns
2. Add comprehensive tests for new features
3. Update documentation for API changes
4. Ensure backward compatibility when possible

## API Reference

### Core Classes

#### `atom::image::blob`

Universal image container with multiple backend support.

```cpp
// Construction
blob img(data_ptr, size);                    // From raw data
blob img = blob::load("image.jpg");          // From file
blob img(cv_mat);                           // From OpenCV Mat

// Properties
int width = img.getWidth();
int height = img.getHeight();
int channels = img.getChannels();
size_t size = img.size();

// Operations
img.resize(800, 600);
img.rotate(90.0);
img.flip(1);
auto cropped = img.crop(10, 10, 100, 100);

// Serialization
auto serialized = img.serialize();
blob restored = blob::deserialize(serialized);
```

#### `atom::image::ImageProcessor`

High-performance image processing pipeline.

```cpp
auto processor = createOptimalProcessor();

// Basic operations
auto resized = processor->resize(img, 800, 600, "cubic");
auto rotated = processor->rotate(img, 45.0, true);
auto cropped = processor->crop(img, 10, 10, 100, 100);

// Filtering
auto blurred = processor->applyFilter(img, FilterType::GAUSSIAN_BLUR, {{"sigma", 2.0}});
auto sharpened = processor->applyFilter(img, FilterType::SHARPEN, {{"strength", 1.5}});

// Enhancement
auto enhanced = processor->adjustBrightnessContrast(img, 20, 10);
auto gamma_corrected = processor->adjustGamma(img, 1.2);
auto equalized = processor->enhanceHistogram(img, true);

// Batch processing
std::vector<blob> inputs = {...};
auto results = processor->processBatch(inputs, [&](const blob& img) {
    return processor->resize(img, 256, 256);
});
```

#### `atom::image::FITSFile`

Astronomical image format support.

```cpp
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

#### `atom::image::EnhancedOCRProcessor`

Optical character recognition with preprocessing.

```cpp
OCRConfig config;
config.language = "eng";
config.enableDeskew = true;
config.enableSpellCheck = true;

EnhancedOCRProcessor ocr(config);

// Process single image
auto result = ocr.processImage(image);
std::cout << "Text: " << result.text << std::endl;
std::cout << "Confidence: " << result.confidence << "%" << std::endl;

// Batch processing
std::vector<cv::Mat> images = {...};
auto results = ocr.processBatch(images);
```

#### `atom::image::SERReader` / `atom::image::SERWriter`

Astronomical video sequence format.

```cpp
// Reading SER files
SERReader reader("video.ser");
auto header = reader.getHeader();

std::cout << "Frames: " << header.frameCount << std::endl;
std::cout << "Size: " << header.imageWidth << "x" << header.imageHeight << std::endl;

// Read individual frames
for (uint32_t i = 0; i < header.frameCount; ++i) {
    auto frame = reader.readFrame(i);
    // Process frame...
}

// Read frame ranges
auto frames = reader.readFrameRange(10, 50);

// Writing SER files
SERWriter writer("output.ser");
SERHeader header = createHeader(width, height, frameCount);
writer.writeHeader(header);

for (const auto& frame : frames) {
    writer.writeFrame(frame);
}
```

### Error Handling

The module uses a comprehensive error handling system:

```cpp
try {
    auto img = blob::load("image.jpg");
    auto processed = processor->resize(img, 800, 600);
} catch (const atom::error::Exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    std::cerr << "Code: " << e.code() << std::endl;
}
```

### Performance Optimization

#### Memory Management

```cpp
// Use fast blobs for view-only operations
fast_blob view(data_ptr, size);

// Enable memory alignment for SIMD operations
blob img;
img.alignMemory(64);

// Configure processor for optimal performance
ProcessingOptions options;
options.useMultithreading = true;
options.enableSIMD = true;
options.maxMemoryUsage = 2 * 1024 * 1024 * 1024; // 2GB
processor->setOptions(options);
```

#### Batch Processing

```cpp
// Process multiple images efficiently
std::vector<blob> images = loadImages();
auto results = processor->processBatch(images, [&](const blob& img) {
    return processor->resize(img, 256, 256);
});
```

## Testing

### Running Tests

```bash
# Build and run all tests
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
make
ctest

# Run specific test suites
make test-blob          # Image blob tests
make test-processor     # Image processor tests
make test-fits          # FITS format tests
make test-ocr           # OCR tests
make test-ser           # SER format tests

# Run performance benchmarks
make benchmark

# Generate coverage report (Debug build)
make coverage
```

### Test Categories

- **Core Tests**: Basic functionality and memory management
- **Integration Tests**: Multi-component workflows
- **Performance Tests**: Benchmarking and optimization validation
- **Error Handling Tests**: Exception safety and error recovery
- **Format Tests**: File I/O and format conversion

## Troubleshooting

### Common Issues

#### OpenCV Not Found

```bash
# Install OpenCV development packages
sudo apt-get install libopencv-dev  # Ubuntu/Debian
brew install opencv                  # macOS
```

#### CFITSIO Not Found

```bash
# Install CFITSIO development packages
sudo apt-get install libcfitsio-dev  # Ubuntu/Debian
brew install cfitsio                  # macOS
```

#### Tesseract Not Found

```bash
# Install Tesseract and Leptonica
sudo apt-get install tesseract-ocr-dev libleptonica-dev  # Ubuntu/Debian
brew install tesseract leptonica                         # macOS
```

#### Memory Issues

- Reduce `maxMemoryUsage` in ProcessingOptions
- Use `fast_blob` for large datasets
- Enable memory pooling for frequent operations

#### Performance Issues

- Enable SIMD optimizations
- Use multithreading for batch operations
- Consider GPU acceleration for large images

## License

This project is licensed under the GPL-3.0 License - see the LICENSE file for details.
