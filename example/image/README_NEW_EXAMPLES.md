# Atom Image Module - New Examples Documentation

This document describes the comprehensive set of new examples added to the Atom Image module to achieve complete coverage of all functionality, edge cases, and advanced use cases.

## Overview

The new examples extend the existing example suite with:
- **Complete edge case coverage** for all core functionality
- **Advanced processing techniques** including GPU acceleration and real-time processing
- **Comprehensive SER format support** for astronomical imaging
- **Memory-efficient I/O operations** for large images
- **Performance optimization examples** and benchmarking
- **Error handling and fallback mechanisms**

## New Examples by Module

### Core Module (`example/image/core/`)

#### `blob_edge_cases.cpp`
**Purpose**: Tests edge cases and boundary conditions for the blob system
**Key Features**:
- Empty blob operations and error handling
- Large blob memory management (>1GB images)
- Memory constraint testing and graceful degradation
- Boundary value testing (zero dimensions, maximum sizes)
- Error condition simulation and recovery

**Usage**:
```bash
./image_core_blob_edge_cases
```

**Requirements**: None (uses core functionality only)

#### `fast_blob_performance.cpp`
**Purpose**: Performance comparison between `blob` and `fast_blob` types
**Key Features**:
- Creation performance benchmarking
- Zero-copy operation demonstrations
- Memory usage pattern analysis
- Read-only operation validation
- Performance scaling with image size

**Usage**:
```bash
./image_core_fast_blob_performance
```

**Requirements**: None (uses core functionality only)

#### `advanced_memory_management.cpp`
**Purpose**: Advanced memory management techniques and optimization
**Key Features**:
- Custom memory pool implementation (`BlobMemoryPool`)
- Memory alignment optimization for SIMD operations
- Cache-friendly data layout strategies
- Large image streaming and tiled processing
- Memory usage monitoring and statistics

**Usage**:
```bash
./image_core_advanced_memory_management
```

**Requirements**: None (uses core functionality only)

#### `serialization_edge_cases.cpp`
**Purpose**: Comprehensive serialization testing and edge case handling
**Key Features**:
- Round-trip serialization integrity testing
- Corruption detection and recovery mechanisms
- Cross-platform compatibility validation
- Performance benchmarking for different blob sizes
- Version compatibility testing

**Usage**:
```bash
./image_core_serialization_edge_cases
```

**Requirements**: None (uses core functionality only)

### Formats Module (`example/image/formats/`)

#### `basic_ser_operations.cpp`
**Purpose**: Comprehensive SER format file operations
**Key Features**:
- SER file reading and writing with full header support
- Multiple color format handling (Mono, RGB, BGR, Bayer patterns)
- Timestamp management and frame metadata
- Performance testing for sequential and random access
- Error handling and validation

**Usage**:
```bash
./image_formats_basic_ser_operations
```

**Requirements**: OpenCV (`ATOM_IMAGE_HAS_OPENCV=ON`)

#### `ser_quality_assessment.cpp`
**Purpose**: SER frame quality assessment and ranking
**Key Features**:
- Multiple quality metrics (sharpness, SNR, entropy, star count)
- Customizable quality parameters and weights
- Quality-based frame selection and filtering
- Statistical analysis of quality distributions
- Performance characteristics for different frame sizes

**Usage**:
```bash
./image_formats_ser_quality_assessment
```

**Requirements**: OpenCV (`ATOM_IMAGE_HAS_OPENCV=ON`)

#### `ser_registration_stacking.cpp`
**Purpose**: SER frame registration and stacking operations
**Key Features**:
- Multiple registration algorithms (phase correlation, feature matching, ECC)
- Various stacking methods (mean, median, sigma clipping, weighted)
- Complete astronomical processing workflow
- Quality-weighted frame combination
- Performance optimization for batch processing

**Usage**:
```bash
./image_formats_ser_registration_stacking
```

**Requirements**: OpenCV (`ATOM_IMAGE_HAS_OPENCV=ON`)

### Processing Module (`example/image/processing/`)

#### `gpu_acceleration_demo.cpp`
**Purpose**: GPU acceleration with comprehensive fallback mechanisms
**Key Features**:
- Multi-backend GPU support (CUDA, OpenCL, Vulkan, Metal)
- GPU device detection and capability assessment
- Performance comparison between GPU and CPU processing
- Graceful fallback mechanisms for systems without GPU
- Batch processing optimization
- Error handling and recovery

**Usage**:
```bash
./image_processing_gpu_acceleration_demo
```

**Requirements**: At least one GPU backend (CUDA, OpenCL, etc.)

#### `realtime_processing.cpp`
**Purpose**: Real-time image processing and optimization
**Key Features**:
- Live camera capture simulation and processing
- Multi-threaded processing pipeline
- Adaptive quality control based on performance
- Frame rate monitoring and statistics
- Performance optimization techniques (SIMD, caching, pre-allocation)
- Thread-safe frame buffering

**Usage**:
```bash
./image_processing_realtime_processing
```

**Requirements**: None (uses simulated camera input)

### I/O Module (`example/image/io/`)

#### `streaming_io_demo.cpp`
**Purpose**: Streaming I/O operations for large images
**Key Features**:
- Streaming read/write for large images (>1GB)
- Progressive loading with multiple quality levels
- Tiled processing for memory-efficient operations
- Streaming compression with multiple algorithms (LZ4, ZSTD, LZMA)
- Memory usage optimization strategies
- Performance monitoring and statistics

**Usage**:
```bash
./image_io_streaming_io_demo
```

**Requirements**: None (creates test files automatically)

#### `memory_mapped_io.cpp`
**Purpose**: Memory-mapped I/O operations and virtual memory management
**Key Features**:
- Memory-mapped file access for efficient random access
- Performance comparison with traditional I/O
- Virtual memory management with page fault handling
- Copy-on-write semantics for efficient data sharing
- Cross-platform compatibility testing
- Memory advice and optimization techniques

**Usage**:
```bash
./image_io_memory_mapped_io
```

**Requirements**: None (uses platform-specific memory mapping APIs)

## Build Configuration

### Basic Build
```bash
mkdir build && cd build
cmake .. -DATOM_IMAGE_BUILD_EXAMPLES=ON
cmake --build . -j
```

### With All Dependencies
```bash
cmake .. \
  -DATOM_IMAGE_BUILD_EXAMPLES=ON \
  -DATOM_IMAGE_HAS_OPENCV=ON \
  -DATOM_IMAGE_HAS_CUDA=ON \
  -DATOM_IMAGE_HAS_OPENCL=ON
cmake --build . -j
```

### Selective Building
Individual examples can be disabled:
```bash
cmake .. \
  -DATOM_IMAGE_BUILD_EXAMPLES=ON \
  -DATOM_EXAMPLE_IMAGE_CORE=ON \
  -DATOM_EXAMPLE_IMAGE_FORMATS=OFF \
  -DATOM_EXAMPLE_IMAGE_PROCESSING=ON
```

## Testing and Validation

### Running All New Examples
```bash
# Core examples
./image_core_blob_edge_cases
./image_core_fast_blob_performance
./image_core_advanced_memory_management
./image_core_serialization_edge_cases

# Format examples (requires OpenCV)
./image_formats_basic_ser_operations
./image_formats_ser_quality_assessment
./image_formats_ser_registration_stacking

# Processing examples
./image_processing_gpu_acceleration_demo
./image_processing_realtime_processing

# I/O examples
./image_io_streaming_io_demo
./image_io_memory_mapped_io
```

### Performance Benchmarking
Many examples include built-in performance benchmarking:
- Memory allocation and access patterns
- I/O throughput measurements
- Processing speed comparisons
- GPU vs CPU performance analysis

### Error Testing
Examples include comprehensive error testing:
- Invalid input handling
- Memory constraint scenarios
- Hardware capability limitations
- File corruption and recovery

## Key Technical Concepts Demonstrated

### Memory Management
- **Memory Pooling**: Custom allocators for efficient memory reuse
- **Alignment Optimization**: SIMD-friendly data layouts
- **Virtual Memory**: Page-based memory management for large datasets
- **Copy-on-Write**: Efficient data sharing with modification isolation

### Performance Optimization
- **SIMD Operations**: Vectorized processing for improved performance
- **Cache Optimization**: Data layout and access patterns for cache efficiency
- **GPU Acceleration**: Parallel processing with fallback mechanisms
- **Streaming Processing**: Memory-efficient handling of large datasets

### Advanced I/O
- **Memory Mapping**: Direct file access without explicit I/O operations
- **Progressive Loading**: Multi-resolution image loading
- **Streaming Compression**: Real-time compression/decompression
- **Tiled Processing**: Chunk-based processing for memory efficiency

### Astronomical Imaging (SER Format)
- **Frame Registration**: Alignment algorithms for image sequences
- **Quality Assessment**: Automated frame quality evaluation
- **Stacking Algorithms**: Multiple methods for combining frames
- **Metadata Handling**: Comprehensive header and timestamp management

## Integration with Existing Examples

The new examples are designed to complement existing examples:
- **Consistent API Usage**: Follow established patterns and conventions
- **Progressive Complexity**: Build upon concepts from basic examples
- **Cross-Reference**: Reference related functionality in other examples
- **Documentation Standards**: Maintain consistent documentation style

## Troubleshooting

### Common Issues

1. **OpenCV Not Found**: Install OpenCV development packages
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libopencv-dev
   
   # macOS
   brew install opencv
   ```

2. **GPU Examples Fail**: Ensure GPU drivers and SDKs are installed
   - CUDA: Install NVIDIA CUDA Toolkit
   - OpenCL: Install appropriate OpenCL runtime

3. **Large Memory Usage**: Some examples create large test files
   - Ensure sufficient disk space (>2GB recommended)
   - Monitor system memory during execution

4. **Performance Variations**: Results may vary based on:
   - System specifications (CPU, memory, storage)
   - Background processes and system load
   - Compiler optimizations and build configuration

### Getting Help

- Check example output for detailed error messages
- Verify build configuration and dependencies
- Review system requirements and capabilities
- Consult main documentation for API details

## Future Enhancements

Potential areas for additional examples:
- **Network Streaming**: Remote image processing and streaming
- **Cloud Integration**: AWS S3, Azure Blob Storage integration
- **Advanced ML**: Deep learning model integration
- **Mobile Optimization**: ARM NEON and mobile GPU examples
- **Distributed Processing**: Multi-node image processing

## Contributing

When adding new examples:
1. Follow existing naming conventions and structure
2. Include comprehensive documentation and comments
3. Add appropriate error handling and edge case testing
4. Update CMakeLists.txt files for build integration
5. Test across different platforms and configurations
6. Update this documentation with new examples
