# Image Core Module

This directory contains the core image data structures and fundamental functionality for the Atom image processing library.

## Overview

The core module provides the foundational classes and utilities that all other image processing components depend on. It includes the universal image container (`blob`) and basic image manipulation operations.

## Components

### Image Blob (`image_blob.hpp`)

The `blob` class is a universal image container that supports multiple backends and provides a unified interface for image data manipulation.

#### Key Features

- **Multiple Backends**: Support for OpenCV, CImg, and stb_image
- **Memory Management**: Both owned (normal) and view-only (fast) modes
- **Format Support**: Automatic format detection and conversion
- **Performance**: Memory alignment and SIMD-friendly operations
- **Serialization**: Built-in serialization and deserialization

#### Usage Examples

```cpp
#include "atom/image/core/image_blob.hpp"

using namespace atom::image;

// Create from file
blob img = blob::load("image.jpg");

// Basic operations
img.resize(800, 600);
img.rotate(90);
img.flip(1);

// Memory-efficient view
fast_blob view(data_ptr, size);

// Serialization
auto serialized = img.serialize();
blob restored = blob::deserialize(serialized);
```

#### Blob Types

- **`blob`**: Normal blob with owned memory, supports all operations
- **`fast_blob`**: View-only blob with minimal overhead, read-only operations
- **`cblob`**: Const blob for immutable data
- **`fast_cblob`**: Const view blob

#### Memory Management

The blob class provides sophisticated memory management:

- **Automatic Alignment**: 64-byte alignment for SIMD operations
- **Memory Pooling**: Integration with system memory pools
- **Copy-on-Write**: Efficient copying for large images
- **RAII**: Automatic resource management

## Design Principles

### Performance First

All core components are designed with performance as the primary concern:

- Zero-copy operations where possible
- SIMD-friendly memory layouts
- Cache-efficient data access patterns
- Minimal virtual function overhead

### Backend Agnostic

The core module abstracts away the underlying image processing library:

- Unified API regardless of backend
- Automatic backend selection based on availability
- Graceful degradation when backends are unavailable

### Type Safety

Strong type safety is enforced throughout:

- Template-based design for compile-time optimization
- Concept-based constraints for valid operations
- Clear separation between mutable and immutable data

## Integration

### With Processing Module

The core blob types integrate seamlessly with the processing module:

```cpp
#include "atom/image/processing/image_processor.hpp"

auto processor = createOptimalProcessor();
blob processed = processor->resize(img, 800, 600);
```

### With Format Modules

Core types work directly with all format handlers:

```cpp
#include "atom/image/formats/fits_file.hpp"

FITSFile fits("data.fits");
blob image_data = fits.getImageData();
```

## Performance Considerations

### Memory Usage

- Use `fast_blob` for view-only operations
- Enable memory alignment for SIMD operations
- Consider image dimensions for cache efficiency

### Processing Speed

- Leverage template specializations for common types
- Use batch operations for multiple images
- Enable compiler optimizations (-O3, -march=native)

## Thread Safety

The core module provides thread-safe operations:

- Immutable blobs are fully thread-safe
- Mutable operations require external synchronization
- Copy operations are atomic and thread-safe

## Error Handling

All core operations use the Atom error handling framework:

```cpp
try {
    blob img = blob::load("image.jpg");
} catch (const atom::error::Exception& e) {
    // Handle error
}
```

## Dependencies

### Required
- atom-error: Error handling framework
- Standard C++20 library

### Optional
- OpenCV: Advanced image operations
- CImg: Alternative image processing backend
- stb_image: Lightweight image I/O

## Testing

Core functionality is thoroughly tested:

```bash
# Run core tests
make test-blob

# Run performance benchmarks
make benchmark-core
```

## Contributing

When contributing to the core module:

1. Maintain backward compatibility
2. Add comprehensive tests for new features
3. Update documentation for API changes
4. Consider performance implications
5. Follow the existing code style
