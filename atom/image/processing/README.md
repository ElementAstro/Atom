# Image Processing Module

This directory contains image processing algorithms, filters, and transformation operations for the Atom image processing library.

## Overview

The processing module provides high-performance image processing operations including filtering, transformations, enhancement, and specialized processing like OCR (Optical Character Recognition).

## Components

### Image Processor (`image_processor.hpp/cpp`)

The `ImageProcessor` class provides a unified interface for all image processing operations with automatic optimization and backend selection.

#### Key Features

- **Unified API**: Single interface for all processing operations
- **Performance Optimization**: SIMD, multithreading, and memory pooling
- **Batch Processing**: Efficient processing of multiple images
- **Format Conversion**: Seamless conversion between image formats
- **Quality Control**: Configurable quality vs speed trade-offs

#### Usage Examples

```cpp
#include "atom/image/processing/image_processor.hpp"

auto processor = createOptimalProcessor();

// Basic transformations
auto resized = processor->resize(img, 800, 600, "cubic");
auto rotated = processor->rotate(img, 45.0, true);
auto cropped = processor->crop(img, 10, 10, 100, 100);

// Filtering operations
auto blurred = processor->applyFilter(img, FilterType::GAUSSIAN_BLUR, {{"sigma", 2.0}});
auto sharpened = processor->applyFilter(img, FilterType::SHARPEN, {{"strength", 1.5}});

// Enhancement
auto enhanced = processor->adjustBrightnessContrast(img, 20, 10);
auto gamma_corrected = processor->adjustGamma(img, 1.2);

// Batch processing
std::vector<blob> results = processor->processBatch(images, [&](const blob& img) {
    return processor->resize(img, 256, 256);
});
```

### OCR Processing (`ocr/`)

Advanced optical character recognition with preprocessing and post-processing capabilities.

#### Components

- **EnhancedOCRProcessor**: Main OCR processing class
- **OCRConfig**: Configuration and settings management
- **ProgressReporter**: Progress tracking for long operations
- **OCRCache**: Result caching for improved performance
- **SpellChecker**: Text correction and validation

#### Features

- **Preprocessing**: Deskewing, noise removal, perspective correction
- **Text Detection**: Advanced text region detection
- **Recognition**: High-accuracy text recognition using Tesseract
- **Post-processing**: Spell checking and text correction
- **Batch Processing**: Efficient processing of multiple images

#### Usage Examples

```cpp
#include "atom/image/processing/ocr/ocr.hpp"

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
auto results = ocr.processBatch(images);
```

## Processing Operations

### Geometric Transformations

- **Resize**: Multiple algorithms (nearest, linear, cubic, lanczos)
- **Rotation**: Arbitrary angle rotation with optional canvas expansion
- **Cropping**: Rectangular region extraction
- **Flipping**: Horizontal and vertical flipping
- **Perspective Correction**: Keystone and perspective distortion correction

### Filtering and Enhancement

- **Blur Filters**: Gaussian, motion, radial blur
- **Sharpening**: Unsharp mask, high-pass filtering
- **Edge Detection**: Sobel, Canny, Laplacian operators
- **Noise Reduction**: Median, bilateral, non-local means
- **Morphological**: Erosion, dilation, opening, closing

### Color and Tone Adjustment

- **Brightness/Contrast**: Linear and non-linear adjustments
- **Gamma Correction**: Power-law transformations
- **Histogram Equalization**: Global and adaptive (CLAHE)
- **Color Space Conversion**: RGB, HSV, LAB, YUV
- **White Balance**: Automatic and manual color correction

### Advanced Processing

- **Super Resolution**: AI-based image upscaling
- **Denoising**: Advanced noise reduction algorithms
- **HDR Processing**: High dynamic range image processing
- **Focus Stacking**: Combining multiple focus planes

## Performance Optimization

### SIMD Operations

Automatic SIMD optimization for supported operations:

```cpp
ProcessingOptions options;
options.enableSIMD = true;  // Enable SIMD optimizations

// Operations that benefit from SIMD:
// - Convolution filters
// - Color space conversions
// - Arithmetic operations
// - Statistical calculations
```

### Multithreading

Efficient parallel processing for batch operations:

```cpp
ProcessingOptions options;
options.useMultithreading = true;
options.maxThreads = std::thread::hardware_concurrency();

// Batch operations automatically use multiple threads
auto results = processor->processBatch(images, operation);
```

### Memory Management

Optimized memory usage patterns:

```cpp
ProcessingOptions options;
options.maxMemoryUsage = 2 * 1024 * 1024 * 1024;  // 2GB limit
options.enableMemoryPooling = true;
```

## Algorithm Selection

Choose algorithms based on requirements:

```cpp
// Resize algorithms (fastest to highest quality)
auto fast = processor->resize(img, 800, 600, "nearest");    // Fastest
auto balanced = processor->resize(img, 800, 600, "linear"); // Balanced
auto quality = processor->resize(img, 800, 600, "cubic");   // Best quality
auto best = processor->resize(img, 800, 600, "lanczos");    // Highest quality
```

## Quality Metrics

Built-in quality assessment:

```cpp
// Image statistics
auto stats = processor->getStatistics(image);

// Quality metrics (PSNR, SSIM, etc.)
auto quality = processor->calculateQualityMetrics(processed, &original);
```

## Error Handling

Comprehensive error handling with detailed diagnostics:

```cpp
try {
    auto result = processor->resize(image, width, height);
} catch (const atom::error::Exception& e) {
    std::cerr << "Processing error: " << e.what() << std::endl;
    std::cerr << "Error code: " << e.code() << std::endl;
}
```

## Dependencies

### Required
- atom-error: Error handling framework
- atom-image-core: Core image functionality

### Optional
- OpenCV: Advanced computer vision operations
- Tesseract: OCR text recognition
- Leptonica: Image preprocessing for OCR

## Configuration

Processing behavior can be configured globally:

```cpp
ProcessingOptions options;
options.useMultithreading = true;
options.enableSIMD = true;
options.quality = 95;  // For lossy operations
options.preserveAspectRatio = true;

processor->setOptions(options);
```

## Testing

Comprehensive test coverage:

```bash
# Run processing tests
make test-processor

# Run OCR tests
make test-ocr

# Run performance benchmarks
make benchmark-processing
```

## Contributing

When contributing to the processing module:

1. Add comprehensive tests for new algorithms
2. Document performance characteristics
3. Consider memory usage implications
4. Provide quality vs speed trade-offs
5. Follow the existing API patterns
