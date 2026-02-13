# Atom Image Module Examples

This directory contains comprehensive examples demonstrating all capabilities of the Atom Image module. The examples are organized by functionality and provide complete, functional demonstrations of the available features.

## 📁 Directory Structure

```
example/image/
├── core/                    # Core image blob operations
├── io/                      # Image I/O and format handling
├── processing/              # Image processing and enhancement
├── formats/                 # Specialized format support
├── metadata/                # Metadata handling and EXIF operations
├── CMakeLists.txt          # Build configuration
└── README.md               # This file
```

## 🚀 Quick Start

### Prerequisites

- C++17 compatible compiler
- CMake 3.10 or higher
- Atom Image library

### Optional Dependencies

For full functionality, install these optional dependencies:

- **OpenCV** - Advanced image processing and computer vision
- **CFITSIO** - FITS astronomical image format support
- **Tesseract** - OCR text recognition capabilities
- **LibRaw** - RAW camera format support
- **Exiv2** - Advanced metadata handling

### Building Examples

```bash
# Configure with all features
cmake -DATOM_IMAGE_HAS_OPENCV=ON \
      -DATOM_IMAGE_HAS_CFITSIO=ON \
      -DATOM_IMAGE_HAS_OCR=ON \
      -DATOM_EXAMPLE_IMAGE_BUILD_ALL=ON \
      ..

# Build all examples
make -j$(nproc)

# Or build specific categories
make image_core_basic_blob_operations
make image_processing_image_enhancement
make image_formats_comprehensive_fits
```

## 📚 Example Categories

### 🔧 Core Examples (`core/`)

**Basic Operations and Memory Management**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `basic_blob_operations` | Fundamental blob operations | Creation, manipulation, serialization |
| `backend_integration` | Different backend support | OpenCV, CImg, stb_image integration |
| `memory_management` | Advanced memory techniques | Pooling, cache optimization, large images |

**Learning Path**: Start here → Backend integration → Memory optimization

### 💾 I/O Examples (`io/`)

**File Operations and Format Conversion**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `basic_file_operations` | Loading and saving images | Format detection, validation, batch processing |
| `format_conversion` | Converting between formats | Quality settings, color space conversion |

**Learning Path**: File operations → Format conversion → Advanced I/O

### 🎨 Processing Examples (`processing/`)

**Image Enhancement and Analysis**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `basic_filters` | Filtering operations | Blur, sharpen, edge detection, noise reduction |
| `geometric_transforms` | Spatial transformations | Resize, rotate, crop, perspective correction |
| `image_enhancement` | Quality improvement | Histogram equalization, tone mapping, color correction |
| `ocr_comprehensive` | Text recognition | Preprocessing, multi-language, batch OCR |

**Learning Path**: Basic filters → Transforms → Enhancement → Specialized processing

### 📄 Format Examples (`formats/`)

**Specialized Format Support**

| Example | Description | Key Features |
|---------|-------------|--------------|
| `comprehensive_fits` | Astronomical FITS format | HDU operations, header keywords, calibration |

**Learning Path**: FITS basics → Advanced formats → Video processing

### 🏷️ Metadata Examples (`metadata/`)

**Image Metadata Handling**

| Example | Description | Key Features |
|---------|-------------|--------------|
| Coming Soon | EXIF operations | Reading, writing, batch processing |

## 🎯 Usage Examples

### Running a Basic Example

```bash
# Run basic blob operations example
./image_core_basic_blob_operations

# Run image enhancement example
./image_processing_image_enhancement

# Run FITS example (requires CFITSIO)
./image_formats_comprehensive_fits
```

### Example Output

```
=== Atom Image Core Blob Operations Example ===
Created RGB blob: 100x100 with 3 channels
Original size: 100x100
After resize: 150x120
After 90° rotation: 120x150
Applied vertical flip
Applied horizontal flip
Cropped region: 50x50
```

## 🔧 Build Configuration

### Feature Flags

Control which examples are built using CMake options:

```bash
# Build specific categories
-DATOM_EXAMPLE_IMAGE_CORE=ON          # Core examples
-DATOM_EXAMPLE_IMAGE_IO=ON            # I/O examples
-DATOM_EXAMPLE_IMAGE_PROCESSING=ON    # Processing examples
-DATOM_EXAMPLE_IMAGE_FORMATS=ON       # Format examples
-DATOM_EXAMPLE_IMAGE_METADATA=ON      # Metadata examples

# Enable optional features
-DATOM_IMAGE_HAS_OPENCV=ON            # OpenCV support
-DATOM_IMAGE_HAS_CFITSIO=ON           # FITS format support
-DATOM_IMAGE_HAS_OCR=ON               # OCR capabilities
```

### Conditional Compilation

Examples automatically detect available features:

```cpp
#ifdef ATOM_IMAGE_HAS_OPENCV
    // OpenCV-specific code
    cv::Mat mat = image.to_mat();
#endif

#ifdef ATOM_IMAGE_HAS_CFITSIO
    // FITS-specific code
    fits_file.save("output.fits");
#endif
```

## 📖 Learning Progression

### Beginner Path

1. `core/basic_blob_operations` - Learn fundamental concepts
2. `io/basic_file_operations` - Understand file handling
3. `processing/basic_filters` - Apply simple processing

### Intermediate Path

1. `core/memory_management` - Optimize performance
2. `processing/geometric_transforms` - Spatial operations
3. `processing/image_enhancement` - Quality improvement

### Advanced Path

1. `formats/comprehensive_fits` - Specialized formats
2. `processing/ocr_comprehensive` - Text recognition
3. `core/backend_integration` - Multi-backend usage

## 🔍 Feature Matrix

| Feature | Core | I/O | Processing | Formats | Metadata |
|---------|------|-----|------------|---------|----------|
| Basic Operations | ✅ | ✅ | ✅ | ✅ | ✅ |
| OpenCV Integration | ✅ | ✅ | ✅ | - | - |
| Memory Optimization | ✅ | ✅ | ✅ | ✅ | - |
| Format Conversion | - | ✅ | - | ✅ | - |
| Image Enhancement | - | - | ✅ | - | - |
| OCR Processing | - | - | ✅ | - | - |
| FITS Support | - | - | - | ✅ | - |
| EXIF Handling | - | - | - | - | ✅ |

## 🐛 Troubleshooting

### Common Issues

**Build Errors**

```bash
# Missing dependencies
sudo apt-get install libopencv-dev libcfitsio-dev tesseract-ocr-dev

# CMake configuration
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**Runtime Issues**

```bash
# Check library paths
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Verify Tesseract data
tesseract --list-langs
```

### Getting Help

1. Check the specific example's source code comments
2. Review the main Atom Image documentation
3. Examine the CMakeLists.txt for build requirements
4. Look at conditional compilation flags

## 🤝 Contributing

To add new examples:

1. Create the source file in the appropriate category directory
2. Add the example to the category's CMakeLists.txt
3. Update this README with the new example
4. Ensure proper documentation and error handling
5. Test with and without optional dependencies

## 🔗 Related Documentation

- [Atom Image API Reference](../../docs/image/api.md)
- [Performance Guide](../../docs/image/performance.md)
- [Format Support Matrix](../../docs/image/formats.md)
- [Build Configuration](../../docs/image/building.md)

## 📊 Performance Notes

### Optimization Tips

- Use `fast_blob` for read-only operations
- Enable OpenCV for advanced processing
- Consider memory pooling for batch operations
- Use appropriate data types (uint8_t vs float)
- Enable SIMD optimizations when available

### Benchmarking

Run performance comparisons:

```bash
./image_core_memory_management     # Memory optimization
./image_processing_basic_filters   # Filter performance
./image_core_backend_integration   # Backend comparison
```

## 📄 License

These examples are part of the Atom Image module and follow the same license terms.
