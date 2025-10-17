# Atom Algorithm Examples

This directory contains comprehensive examples demonstrating the capabilities of the Atom algorithm library. The examples are organized by category and showcase real-world usage patterns, performance characteristics, and best practices.

## Directory Structure

```
example/algorithm/
├── core/                    # Core algorithm concepts and utilities
├── crypto/                  # Cryptographic algorithms
├── hash/                    # Hash functions and utilities
├── math/                    # Mathematical algorithms
├── compression/             # Data compression algorithms
├── signal/                  # Signal processing algorithms
├── optimization/            # Optimization and pathfinding algorithms
├── encoding/                # Data encoding and conversion
├── graphics/                # Graphics and image processing algorithms
├── utils/                   # Utility algorithms and helpers
└── CMakeLists.txt          # Build configuration
```

## Categories Overview

### 🔧 Core (`core/`)

Fundamental algorithm concepts and modern C++ features:

- **algorithm.cpp**: Modern C++20 concepts, RAII patterns, performance measurement
- **rust_numeric.cpp**: Rust-inspired numeric operations and safety

### 🔐 Crypto (`crypto/`)

Cryptographic algorithms and security functions:

- **blowfish.cpp**: Blowfish encryption with variable key lengths
- **md5.cpp**: MD5 hash computation (comprehensive existing example)
- **sha1.cpp**: SHA-1 hash with security warnings and performance analysis
- **tea.cpp**: TEA family algorithms (TEA, XTEA, XXTEA) with performance comparison

### 🔗 Hash (`hash/`)

High-performance hash functions:

- **hash.cpp**: Universal hash functions with SIMD optimizations
- **mhash.cpp**: MinHash for similarity estimation and Keccak-256

### 🧮 Math (`math/`)

Mathematical algorithms and utilities:

- **math.cpp**: Safe arithmetic, bit operations, number theory
- **matrix.cpp**: Matrix operations (comprehensive existing example)
- **fraction.cpp**: Fraction arithmetic (comprehensive existing example)
- **bignumber.cpp**: Big number operations (comprehensive existing example)

### 🗜️ Compression (`compression/`)

Data compression algorithms:

- **huffman.cpp**: Huffman coding with tree serialization
- **matrix_compress.cpp**: Matrix compression with run-length encoding

### 📡 Signal (`signal/`)

Signal processing algorithms:

- **convolve.cpp**: 1D/2D convolution with FFT optimization (enhanced existing example)

### 🎯 Optimization (`optimization/`)

Optimization and pathfinding algorithms:

- **annealing.cpp**: Simulated annealing (comprehensive existing example)
- **pathfinding.cpp**: A*, Dijkstra, and JPS pathfinding algorithms

### 📝 Encoding (`encoding/`)

Data encoding and conversion:

- **base.cpp**: Base64/Base32 encoding with SIMD and XOR encryption

### 🎨 Graphics (`graphics/`)

Graphics and image processing:

- **flood.cpp**: Flood fill algorithms with parallel processing
- **perlin.cpp**: Perlin noise generation for procedural content

### 🛠️ Utils (`utils/`)

Utility algorithms and helpers:

- **fnmatch.cpp**: Filename pattern matching with glob patterns
- **snowflake.cpp**: Distributed unique ID generation
- **weight.cpp**: Weighted algorithms (existing example)
- **error_calibration.cpp**: Error calibration utilities (existing example)

## Building Examples

### Prerequisites

- CMake 3.10 or higher
- C++20 compatible compiler
- Atom library installed

### Build All Examples

```bash
cd example/algorithm
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Build Specific Categories

```bash
# Build only crypto examples
cmake -DATOM_EXAMPLE_ALGORITHM_BUILD_ALL=OFF -DATOM_EXAMPLE_ALGORITHM_CRYPTO=ON ..

# Build specific example
cmake -DATOM_EXAMPLE_ALGORITHM_BUILD_ALL=OFF -DATOM_EXAMPLE_ALGORITHM_CRYPTO_BLOWFISH=ON ..
```

### CMake Options

- `ATOM_EXAMPLE_ALGORITHM_BUILD_ALL`: Build all algorithm examples (default: ON)
- `ATOM_EXAMPLE_ALGORITHM_<CATEGORY>`: Build specific category
- `ATOM_EXAMPLE_ALGORITHM_<CATEGORY>_<EXAMPLE>`: Build specific example

## Running Examples

Each example is built as a standalone executable:

```bash
# Run crypto examples
./algorithm_crypto_blowfish
./algorithm_crypto_sha1
./algorithm_crypto_tea

# Run hash examples
./algorithm_hash_hash
./algorithm_hash_mhash

# Run math examples
./algorithm_math_math

# And so on...
```

## Example Features

### 🚀 Performance Optimizations

- **SIMD Instructions**: Vectorized operations where supported
- **Parallel Processing**: Multi-threaded algorithms for large datasets
- **Memory Efficiency**: Optimized memory usage patterns
- **Cache-Friendly**: Algorithms designed for modern CPU architectures

### 🔍 Comprehensive Testing

- **Edge Cases**: Thorough testing of boundary conditions
- **Error Handling**: Robust error detection and recovery
- **Performance Analysis**: Timing and throughput measurements
- **Quality Metrics**: Algorithm quality and accuracy validation

### 📚 Educational Value

- **Detailed Documentation**: Comprehensive inline documentation
- **Usage Patterns**: Real-world application examples
- **Best Practices**: Modern C++ idioms and patterns
- **Algorithm Comparison**: Side-by-side performance comparisons

### 🛡️ Security Considerations

- **Cryptographic Warnings**: Clear security guidance for crypto algorithms
- **Safe Defaults**: Secure-by-default configurations
- **Vulnerability Notes**: Known limitations and security considerations

## Key Enhancements Made

### 1. **Comprehensive Coverage**

- Added missing Blowfish encryption example
- Enhanced basic examples with detailed demonstrations
- Added performance analysis and benchmarking

### 2. **Modern C++ Features**

- C++20 concepts and constraints
- RAII and exception safety patterns
- Smart pointers and modern memory management
- Ranges and views where applicable

### 3. **Real-World Applications**

- Practical use cases for each algorithm
- Integration examples and workflows
- Performance optimization techniques

### 4. **Educational Structure**

- Progressive complexity in examples
- Clear section headers and organization
- Comprehensive error handling demonstrations

### 5. **Performance Focus**

- Timing measurements and throughput analysis
- SIMD optimization examples
- Parallel processing demonstrations
- Memory usage optimization

## Contributing

When adding new examples:

1. **Follow the established structure**: Use the category-based organization
2. **Include comprehensive documentation**: Add detailed header comments
3. **Demonstrate real-world usage**: Show practical applications
4. **Add performance analysis**: Include timing and benchmarking
5. **Handle edge cases**: Test boundary conditions and error scenarios
6. **Use modern C++**: Leverage C++20 features where appropriate

## License

These examples are part of the Atom framework and follow the same licensing terms.
