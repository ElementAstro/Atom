# Atom Algorithm Module

A comprehensive collection of high-performance algorithms and data structures implemented in modern C++20.

## 🏗️ Architecture

The algorithm module has been restructured into logical categories for better organization and maintainability:

```
atom/algorithm/
├── core/           # Fundamental building blocks and common utilities
├── crypto/         # Cryptographic algorithms and hash functions
├── hash/           # General-purpose hashing and similarity algorithms
├── math/           # Mathematical computations and data structures
├── compression/    # Data compression algorithms
├── signal/         # Signal processing and convolution
├── optimization/   # Optimization and pathfinding algorithms
├── encoding/       # Data encoding/decoding (Base64, Base32, etc.)
├── graphics/       # Graphics and image processing algorithms
└── utils/          # Miscellaneous utility algorithms
```

## 📦 Categories

### [Core](core/) - Foundation Components

- **rust_numeric.hpp** - Rust-style type aliases (i8, u8, i32, u32, f32, f64, etc.)
- **algorithm.hpp** - Common concepts, base classes, and utilities

### [Crypto](crypto/) - Cryptographic Algorithms

- **MD5** - MD5 hash algorithm (⚠️ cryptographically broken)
- **SHA-1** - SHA-1 hash with SIMD optimizations (⚠️ cryptographically broken)
- **Blowfish** - Symmetric encryption algorithm
- **TEA/XTEA** - Tiny Encryption Algorithm variants

### [Hash](hash/) - Hashing Utilities

- **High-performance hashing** - FNV-1a, xxHash, CityHash, MurmurHash3
- **MinHash** - Similarity estimation and Jaccard index calculation
- **SIMD optimizations** - AVX2 accelerated hash functions

### [Math](math/) - Mathematical Algorithms

- **Extended math functions** - GCD, LCM, primality testing
- **Matrix operations** - Template-based linear algebra
- **Fraction arithmetic** - Rational number computations
- **Big numbers** - Arbitrary precision arithmetic

### [Compression](compression/) - Data Compression

- **Huffman coding** - Parallel and SIMD optimized compression
- **Matrix compression** - Specialized sparse matrix compression

### [Signal](signal/) - Signal Processing

- **Convolution** - 1D/2D convolution with multiple algorithms
- **FFT-based processing** - Efficient large-kernel convolution
- **OpenCL acceleration** - GPU-accelerated signal processing

### [Optimization](optimization/) - Search and Optimization

- **Simulated annealing** - Global optimization with multiple cooling strategies
- **Pathfinding** - A\*, Dijkstra, JPS algorithms for graph traversal

### [Encoding](encoding/) - Data Encoding

- **Base64/Base32** - RFC-compliant encoding with SIMD optimizations
- **XOR encryption** - Simple encryption for data obfuscation

### [Graphics](graphics/) - Image Processing

- **Flood fill** - BFS/DFS flood fill with connectivity options
- **Perlin noise** - Procedural noise generation for textures

### [Utils](utils/) - Utility Algorithms

- **Filename matching** - Glob-style pattern matching
- **Snowflake IDs** - Distributed unique identifier generation
- **Weighted sampling** - Probability-based selection algorithms
- **Error calibration** - Numerical algorithm validation utilities

## 🔄 Backward Compatibility

**All existing code continues to work without changes!** The module maintains full backward compatibility through forwarding headers:

```cpp
// These includes still work exactly as before:
#include "atom/algorithm/md5.hpp"
#include "atom/algorithm/hash.hpp"
#include "atom/algorithm/math.hpp"
// ... all existing includes are preserved
```

For new code, you can use the new organized structure:

```cpp
// New organized includes (optional):
#include "atom/algorithm/crypto/md5.hpp"
#include "atom/algorithm/hash/hash.hpp"
#include "atom/algorithm/math/math.hpp"
```

## 🚀 Features

- **Modern C++20** - Uses concepts, constexpr, ranges, and other modern features
- **High Performance** - SIMD optimizations, parallel processing, cache-friendly algorithms
- **Thread Safe** - All algorithms are designed for concurrent use
- **Exception Safe** - Robust error handling with custom exception types
- **Memory Efficient** - Optimized memory usage and allocation patterns
- **Cross Platform** - Works on Windows, Linux, and macOS

## 🛠️ Build Requirements

- **C++20 compatible compiler** (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake 3.20+** or **XMake 2.8.0+**
- **Dependencies**: OpenSSL, TBB, spdlog
- **Optional**: OpenCL (for GPU acceleration), Boost (for additional features)

## 📖 Usage Examples

```cpp
#include "atom/algorithm/crypto/md5.hpp"
#include "atom/algorithm/hash/hash.hpp"
#include "atom/algorithm/math/math.hpp"

// Cryptographic hashing
auto md5_hash = atom::algorithm::MD5::encrypt("Hello, World!");

// High-performance hashing
auto hash_value = atom::algorithm::computeHash("data",
                                              atom::algorithm::HashAlgorithm::FNV1A);

// Mathematical operations
auto gcd_result = atom::algorithm::gcd64(48, 18);
auto is_prime = atom::algorithm::isPrime(97);
```

## 🔧 Build Instructions

### Using CMake

```bash
cd atom/algorithm
cmake -B build -S .
cmake --build build
```

### Using XMake

```bash
cd atom/algorithm
xmake
```

## 📝 Migration Guide

No migration is required! All existing code continues to work. However, for new projects, consider:

1. **Use new organized includes** for better code organization
2. **Leverage modern C++20 features** like concepts and ranges
3. **Take advantage of performance optimizations** in the new implementations
4. **Follow the new directory structure** when adding new algorithms

## 🤝 Contributing

When adding new algorithms:

1. **Choose the appropriate category** or propose a new one
2. **Follow the established patterns** in each directory
3. **Include comprehensive tests** and documentation
4. **Maintain backward compatibility** for any changes to existing APIs
5. **Update the relevant README.md** files

## 📄 License

This module is part of the Atom project and follows the same licensing terms.
