# Data Compression Algorithms

This directory contains algorithms for data compression and decompression.

## Contents

- **`huffman.hpp/cpp`** - Huffman coding compression with parallel and SIMD optimizations
- **`matrix_compress.hpp/cpp`** - Specialized matrix compression algorithms

## Features

### Huffman Compression

- **Parallel Tree Building**: Multi-threaded Huffman tree construction
- **SIMD Compression**: Vectorized compression for improved performance
- **Adaptive Algorithms**: Dynamic Huffman coding support
- **Memory Efficient**: Optimized memory usage for large datasets
- **Exception Safe**: Robust error handling and validation

### Matrix Compression

- **Sparse Matrix Support**: Efficient compression of sparse matrices
- **Multiple Formats**: Support for various matrix compression formats
- **Lossless Compression**: Preserves exact matrix values
- **Fast Decompression**: Optimized for quick matrix reconstruction

## Use Cases

- **File Compression**: General-purpose data compression
- **Network Transmission**: Reduce bandwidth usage
- **Data Storage**: Minimize storage requirements
- **Scientific Computing**: Compress large numerical datasets
- **Image Processing**: Lossless image compression (when combined with appropriate preprocessing)

## Performance Features

- **Parallel Processing**: Multi-threaded compression and decompression
- **SIMD Optimizations**: Vectorized operations for bulk data processing
- **Memory Pooling**: Efficient memory management for large datasets
- **Streaming Support**: Process data without loading entire datasets into memory

## Usage Examples

```cpp
#include "atom/algorithm/compression/huffman.hpp"

// Basic Huffman compression
std::string data = "Hello, World! This is a test string for compression.";
auto compressed = atom::algorithm::huffmanCompress(data);
auto decompressed = atom::algorithm::huffmanDecompress(compressed);

// Parallel compression for large datasets
auto parallel_compressed = atom::algorithm::huffman_optimized::compressParallel(
    std::span<const unsigned char>(data.begin(), data.end()),
    huffman_codes,
    4  // thread count
);
```

## Algorithm Details

### Huffman Coding

- Uses frequency analysis to build optimal prefix codes
- Supports both static and adaptive variants
- Implements canonical Huffman codes for better compression ratios
- Parallel tree construction for large alphabets

### Matrix Compression

- Detects sparse patterns automatically
- Uses run-length encoding for dense regions
- Supports both row-major and column-major compression
- Optimized for numerical matrices with repeated values

## Dependencies

- Core algorithm components
- Standard C++ library (C++20 features)
- TBB for parallel processing
- Optional: SIMD intrinsics for vectorization
