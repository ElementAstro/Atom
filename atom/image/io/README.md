# Image I/O Module

This directory contains input/output operations for image files, including format detection, streaming I/O, and batch file operations.

## Overview

The I/O module provides efficient and flexible image file input/output operations. It handles format detection, streaming for large files, batch operations, and integration with various storage backends.

## Components

### Format Detection

Automatic detection of image file formats based on file headers, magic numbers, and file extensions.

#### Features
- **Magic Number Detection**: Reliable format identification from file headers
- **Extension Fallback**: Format detection from file extensions when headers are unavailable
- **Confidence Scoring**: Probability-based format detection for ambiguous cases
- **Custom Format Support**: Extensible system for adding new format detectors

#### Usage Examples

```cpp
#include "atom/image/io/format_detector.hpp"

FormatDetector detector;

// Detect format from file
auto format = detector.detectFormat("image.jpg");
// Returns ImageFormat::JPEG

// Detect from data buffer
std::vector<uint8_t> data = readFileData("image.png");
auto format = detector.detectFormat(data);
// Returns ImageFormat::PNG

// Get detection confidence
auto result = detector.detectWithConfidence("image.tiff");
if (result.confidence > 0.9) {
    // High confidence detection
    processImage(result.format);
}
```

### Streaming I/O

Efficient streaming operations for large image files and video sequences.

#### Features
- **Progressive Loading**: Load images progressively for immediate display
- **Memory-Mapped I/O**: Efficient access to large files without loading into memory
- **Chunked Processing**: Process large images in manageable chunks
- **Async Operations**: Non-blocking I/O operations

#### Usage Examples

```cpp
#include "atom/image/io/stream_reader.hpp"

// Progressive image loading
StreamReader reader("large_image.tiff");
reader.setChunkSize(1024 * 1024);  // 1MB chunks

while (reader.hasMoreData()) {
    auto chunk = reader.readChunk();
    processChunk(chunk);
    
    // Update progress
    double progress = reader.getProgress();
    updateProgressBar(progress);
}

// Memory-mapped access
MemoryMappedImage mmap("huge_image.fits");
auto pixel = mmap.getPixel(x, y);  // Direct access without loading
```

### Batch Operations

Efficient processing of multiple image files with parallel I/O.

#### Features
- **Parallel Loading**: Load multiple images simultaneously
- **Queue Management**: Intelligent queuing for optimal resource usage
- **Progress Tracking**: Comprehensive progress reporting for batch operations
- **Error Recovery**: Graceful handling of individual file failures

#### Usage Examples

```cpp
#include "atom/image/io/batch_loader.hpp"

BatchLoader loader;
loader.setMaxConcurrency(4);  // 4 parallel operations

// Add files to batch
std::vector<std::string> files = {
    "image1.jpg", "image2.png", "image3.tiff"
};

loader.addFiles(files);

// Process with callback
loader.process([](const std::string& filename, const blob& image) {
    // Process each loaded image
    auto processed = applyFilters(image);
    processed.save("processed_" + filename);
});

// Or load all at once
auto images = loader.loadAll();
```

### File System Integration

Integration with various file systems and storage backends.

#### Features
- **Local File System**: Standard file operations
- **Network Storage**: HTTP/HTTPS, FTP, cloud storage
- **Archive Support**: ZIP, TAR, and other archive formats
- **Virtual File Systems**: In-memory and custom file systems

#### Usage Examples

```cpp
#include "atom/image/io/file_system.hpp"

// Local file system
LocalFileSystem local;
auto image = local.loadImage("photos/vacation.jpg");

// HTTP/HTTPS support
HttpFileSystem http;
auto remote_image = http.loadImage("https://example.com/image.png");

// Cloud storage
CloudFileSystem cloud("aws-s3", credentials);
auto cloud_image = cloud.loadImage("bucket/path/image.jpg");

// Archive support
ArchiveFileSystem archive("photos.zip");
auto archived_image = archive.loadImage("vacation/beach.jpg");
```

## Performance Optimization

### Caching

Intelligent caching for frequently accessed files:

```cpp
#include "atom/image/io/cache_manager.hpp"

CacheManager cache;
cache.setMaxSize(500 * 1024 * 1024);  // 500MB cache
cache.setEvictionPolicy(EvictionPolicy::LRU);

// Cache will automatically manage frequently accessed images
auto image = cache.loadImage("frequently_used.jpg");
```

### Prefetching

Predictive loading for improved user experience:

```cpp
#include "atom/image/io/prefetcher.hpp"

Prefetcher prefetcher;

// Prefetch based on access patterns
prefetcher.addPattern({"image1.jpg", "image2.jpg", "image3.jpg"});

// Prefetch in background
prefetcher.startPrefetching();

// Images will be ready when requested
auto image = prefetcher.getImage("image2.jpg");  // Instant access
```

### Compression

On-the-fly compression and decompression:

```cpp
#include "atom/image/io/compression.hpp"

CompressionManager compression;

// Compress during save
SaveOptions options;
options.compression = CompressionType::LZ4;
options.compressionLevel = 6;

image.save("compressed.atom", options);

// Automatic decompression during load
auto decompressed = blob::load("compressed.atom");
```

## Error Handling

Comprehensive error handling for I/O operations:

```cpp
try {
    auto image = blob::load("image.jpg");
} catch (const FileNotFoundException& e) {
    // File doesn't exist
} catch (const PermissionDeniedException& e) {
    // Access denied
} catch (const CorruptedFileException& e) {
    // File is corrupted
} catch (const UnsupportedFormatException& e) {
    // Format not supported
} catch (const atom::error::Exception& e) {
    // General I/O error
}
```

## Format Support

### Standard Formats
- **JPEG**: Full support with quality control
- **PNG**: Complete support including transparency
- **TIFF**: Multi-page and compression support
- **BMP**: Standard bitmap format
- **WebP**: Modern web format with animation
- **HEIF**: High-efficiency image format

### Specialized Formats
- **FITS**: Astronomical image format
- **SER**: Astronomical video sequences
- **RAW**: Camera raw formats (CR2, NEF, ARW, etc.)
- **Medical**: DICOM and other medical imaging formats

### Archive Formats
- **ZIP**: Standard archive format
- **TAR**: Unix archive format
- **7Z**: High-compression archive format

## Configuration

Global I/O configuration:

```cpp
#include "atom/image/io/config.hpp"

IOConfig config;
config.maxConcurrentOperations = 8;
config.defaultBufferSize = 64 * 1024;  // 64KB
config.enableMemoryMapping = true;
config.cacheSize = 256 * 1024 * 1024;  // 256MB

IOManager::setGlobalConfig(config);
```

## Monitoring

I/O operation monitoring and statistics:

```cpp
#include "atom/image/io/monitor.hpp"

IOMonitor monitor;

// Get I/O statistics
auto stats = monitor.getStatistics();
std::cout << "Files loaded: " << stats.filesLoaded << std::endl;
std::cout << "Total bytes: " << stats.totalBytes << std::endl;
std::cout << "Average speed: " << stats.averageSpeed << " MB/s" << std::endl;

// Monitor specific operations
monitor.startOperation("large_file.tiff");
auto image = blob::load("large_file.tiff");
monitor.endOperation("large_file.tiff");
```

## Dependencies

### Required
- atom-error: Error handling framework
- atom-image-core: Core image functionality

### Optional
- libcurl: HTTP/HTTPS support
- zlib: Compression support
- liblzma: LZMA compression
- libarchive: Archive format support

## Testing

I/O operation testing:

```bash
# Run I/O tests
make test-io

# Test with various formats
make test-io-formats

# Performance benchmarks
make benchmark-io

# Test with large files
make test-io-large-files
```

## Contributing

When contributing to I/O operations:

1. Add comprehensive error handling
2. Consider performance implications
3. Test with various file sizes
4. Support both sync and async operations
5. Add appropriate caching strategies
6. Document format-specific limitations
