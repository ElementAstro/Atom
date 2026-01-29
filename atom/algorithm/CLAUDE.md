# atom::algorithm Module Documentation

[根目录](../../CLAUDE.md) > **algorithm**

---

## Module Overview

The **algorithm** module provides comprehensive mathematical algorithms, cryptographic operations, and data processing utilities for the Atom framework. It serves as a foundational module for high-performance computational tasks.

### Module Metadata

| Attribute | Value |
|-----------|-------|
| **Version** | 1.0.0 |
| **Namespace** | `atom::algorithm` |
| **Dependencies** | atom-type, atom-utils, atom-error |
| **Optional Deps** | OpenSSL (crypto), TBB (parallel) |

---

## Module Responsibilities

### Core Capabilities

1. **Mathematical Operations**
   - Basic math utilities and numerical algorithms
   - Fraction and BigNumber support for precise calculations
   - Matrix operations and compression
   - Statistics and numerical analysis

2. **Cryptography**
   - MD5 and SHA-1 hashing
   - Blowfish and TEA encryption
   - Multi-hash (mhash) utilities

3. **Compression**
   - Huffman coding implementation
   - Matrix compression algorithms

4. **Signal Processing**
   - Convolution operations
   - Filtering algorithms

5. **Optimization**
   - Pathfinding algorithms (A*, Dijkstra, etc.)
   - Simulated annealing

6. **Graphics**
   - Flood fill algorithms
   - Perlin and simplex noise generation
   - Image operations

7. **Utilities**
   - fnmatch pattern matching
   - Snowflake ID generation
   - Weight calculations
   - UUID generation
   - Base encoding/decoding

8. **GPU Acceleration**
   - OpenCL utilities
   - SIMD operations
   - GPU-accelerated math operations

---

## Entry Points and Public APIs

### Main Header

```cpp
#include "atom/algorithm/algorithm.hpp"
```

**Note:** This is a backwards compatibility header. New code should use specific subdirectory headers:

```cpp
#include "atom/algorithm/core/algorithm.hpp"
#include "atom/algorithm/math/math.hpp"
#include "atom/algorithm/crypto/md5.hpp"
```

### Key Classes and Functions

#### Mathematical Operations

```cpp
namespace atom::algorithm::math {

// Basic math utilities
double calculateAverage(const std::vector<double>& data);
double calculateStandardDeviation(const std::vector<double>& data);

// Fraction support for precise calculations
class Fraction {
public:
    Fraction(int64_t numerator, int64_t denominator);
    // Fraction arithmetic and comparison
};

// BigNumber for arbitrary precision
class BigNumber {
public:
    BigNumber(const std::string& value);
    // Big number operations
};

// Matrix operations
template<typename T>
class Matrix {
public:
    Matrix(size_t rows, size_t cols);
    // Matrix operations: multiply, transpose, etc.
};

} // namespace atom::algorithm::math
```

#### Cryptographic Operations

```cpp
namespace atom::algorithm::crypto {

// MD5 hashing
std::string md5(const std::string& input);
std::string md5File(const std::string& filepath);

// SHA-1 hashing
std::string sha1(const std::string& input);

// Blowfish encryption
class Blowfish {
public:
    Blowfish(const std::string& key);
    std::string encrypt(const std::string& plaintext);
    std::string decrypt(const std::string& ciphertext);
};

// TEA encryption
class TEA {
public:
    TEA(const std::string& key);
    void encrypt(uint32_t* v, size_t n);
    void decrypt(uint32_t* v, size_t n);
};

} // namespace atom::algorithm::crypto
```

#### Compression

```cpp
namespace atom::algorithm::compression {

// Huffman coding
class Huffman {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
};

// Matrix compression
class MatrixCompressor {
public:
    Matrix compressMatrix(const Matrix& input);
    Matrix decompressMatrix(const Matrix& compressed);
};

} // namespace atom::algorithm::compression
```

#### Pathfinding

```cpp
namespace atom::algorithm::optimization {

// A* pathfinding
template<typename Node, typename Cost>
std::vector<Node> findPathAStar(
    const Node& start,
    const Node& goal,
    std::function<std::vector<Node>(const Node&)> neighbors,
    std::function<Cost(const Node&, const Node&)> cost,
    std::function<Cost(const Node&, const Node&)> heuristic
);

// Simulated annealing
template<typename State, typename Energy>
State simulatedAnnealing(
    State initial_state,
    std::function<Energy(const State&)> energy_function,
    std::function<State(const State&)> neighbor_function,
    double temperature,
    double cooling_rate
);

} // namespace atom::algorithm::optimization
```

---

## Key Dependencies and Configuration

### Required Dependencies

- **atom-type**: Type utilities and containers
- **atom-utils**: String utilities and error handling
- **atom-error**: Error handling framework

### Optional Dependencies

| Dependency | Purpose | CMake Flag |
|------------|---------|------------|
| **OpenSSL** | Cryptographic operations | `ATOM_HAS_OPENSSL=1` |
| **TBB** | Parallel algorithms | `TBB_FOUND` |

### Conditional Compilation

The module supports conditional compilation based on available dependencies:

```cpp
#ifdef ATOM_HAS_OPENSSL
    // OpenSSL-accelerated crypto operations
#endif

#if defined(ATOM_HAS_TBB)
    // TBB-parallelized algorithms
#endif
```

---

## Data Models and Structures

### Fraction

Represents rational numbers with precise arithmetic:

```cpp
class Fraction {
    int64_t numerator_;
    int64_t denominator_;

public:
    Fraction(int64_t num, int64_t denom);
    Fraction operator+(const Fraction& other) const;
    Fraction operator-(const Fraction& other) const;
    Fraction operator*(const Fraction& other) const;
    Fraction operator/(const Fraction& other) const;
    bool operator==(const Fraction& other) const;
    double toDouble() const;
    std::string toString() const;
};
```

### BigNumber

Arbitrary-precision arithmetic:

```cpp
class BigNumber {
    std::vector<uint32_t> digits_;
    bool negative_;

public:
    BigNumber(const std::string& value);
    BigNumber(int64_t value);

    BigNumber operator+(const BigNumber& other) const;
    BigNumber operator-(const BigNumber& other) const;
    BigNumber operator*(const BigNumber& other) const;
    BigNumber operator/(const BigNumber& other) const;

    std::string toString() const;
};
```

### Matrix

Generic matrix container with operations:

```cpp
template<typename T>
class Matrix {
    std::vector<std::vector<T>> data_;
    size_t rows_;
    size_t cols_;

public:
    Matrix(size_t rows, size_t cols);

    T& at(size_t row, size_t col);
    const T& at(size_t row, size_t col) const;

    Matrix<T> multiply(const Matrix<T>& other) const;
    Matrix<T> transpose() const;
    T determinant() const;

    static Matrix<T> identity(size_t size);
};
```

---

## Testing and Quality

### Test Coverage

Tests are located in `tests/algorithm/`:

```
tests/algorithm/
├── test_math.cpp          # Basic math operations
├── test_crypto.cpp        # Cryptographic functions
├── test_compression.cpp   # Compression algorithms
├── test_matrix.cpp        # Matrix operations
├── test_pathfinding.cpp   # Pathfinding algorithms
└── CMakeLists.txt
```

### Running Tests

```bash
# Build algorithm tests
cmake -B build -DATOM_BUILD_ALGORITHM=ON -DATOM_BUILD_TESTS=ON
cmake --build build

# Run algorithm tests
ctest -R "algorithm_*" --output-on-failure

# Run specific test
./build/tests/algorithm/test_crypto
```

### Test Categories

1. **Unit Tests**: Individual function testing
2. **Integration Tests**: Algorithm combinations
3. **Performance Tests**: Benchmarking critical paths
4. **Edge Cases**: Boundary conditions and error handling

---

## Common Development Tasks

### Adding a New Algorithm

1. Choose appropriate subdirectory (e.g., `math/`, `crypto/`, `optimization/`)
2. Create header in `atom/algorithm/<subcategory>/<algorithm>.hpp`
3. Create implementation in `atom/algorithm/<subcategory>/<algorithm>.cpp`
4. Update `atom/algorithm/CMakeLists.txt` to include new files
5. Add tests in `tests/algorithm/test_<algorithm>.cpp`
6. Add example in `example/algorithm/`

### Adding GPU Acceleration

1. Implement OpenCL kernel in `core/opencl_utils.cpp`
2. Add GPU-accelerated variant in appropriate module
3. Use conditional compilation for GPU support:

   ```cpp
   #ifdef ATOM_HAS_OPENCL
       // GPU-accelerated implementation
   #else
       // CPU fallback
   #endif
   ```

### Performance Optimization

1. Use TBB for parallel algorithms when available
2. Implement SIMD variants in `core/simd_utils.hpp`
3. Add performance benchmarks in tests
4. Document complexity and performance characteristics

---

## Usage Examples

### Basic Math Operations

```cpp
#include "atom/algorithm/math/math.hpp"

using namespace atom::algorithm;

// Calculate statistics
std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
double avg = math::calculateAverage(data);
double stddev = math::calculateStandardDeviation(data);

// Fraction arithmetic
math::Fraction f1(1, 2);
math::Fraction f2(1, 3);
auto sum = f1 + f2;  // 5/6
```

### Cryptographic Operations

```cpp
#include "atom/algorithm/crypto/md5.hpp"
#include "atom/algorithm/crypto/blowfish.hpp"

using namespace atom::algorithm;

// Calculate hash
std::string hash = crypto::md5("Hello, World!");

// Encrypt/decrypt
crypto::Blowfish cipher("secret_key");
std::string encrypted = cipher.encrypt("sensitive data");
std::string decrypted = cipher.decrypt(encrypted);
```

### Pathfinding

```cpp
#include "atom/algorithm/optimization/pathfinding.hpp"

using namespace atom::algorithm::optimization;

// A* pathfinding
struct Node {
    int x, y;
    bool operator==(const Node& other) const {
        return x == other.x && y == other.y;
    }
};

auto path = findPathAStar<Node, double>(
    start_node, goal_node,
    [](const Node& n) { return getNeighbors(n); },
    [](const Node& a, const Node& b) { return distance(a, b); },
    [](const Node& a, const Node& b) { return heuristic(a, b); }
);
```

---

## Integration with Other Modules

### Used By

- **image**: Image processing algorithms, compression
- **secret**: Cryptographic operations
- **search**: Hash-based caching
- **connection**: Encryption for secure communication

### Using

- **type**: Type utilities and containers
- **utils**: String utilities, error handling
- **error**: Exception handling framework

---

## Platform-Specific Notes

### Windows (MSVC)

- Requires OpenSSL for cryptographic operations
- SIMD optimizations use SSE/AVX intrinsics

### Windows (MSYS2 MinGW64)

- Same as MSVC but with GCC compatibility

### Linux

- Can use system OpenSSL libraries
- SIMD optimizations available

### macOS

- Can use Homebrew OpenSSL
- Metal (Metal Performance Shaders) support planned

---

## Known Limitations

1. **OpenSSL Dependency**: Some crypto features require OpenSSL
2. **GPU Support**: OpenCL support is limited to compatible hardware
3. **BigNumber**: Performance degrades for very large numbers
4. **Matrix**: Not optimized for sparse matrices

---

## Future Enhancements

- [ ] Add support for more crypto algorithms (AES, RSA)
- [ ] Implement GPU-accelerated pathfinding
- [ ] Add sparse matrix support
- [ ] Implement Metal (macOS) and CUDA (NVIDIA) backends
- [ ] Add more optimization algorithms (genetic algorithms, etc.)

---

## FAQ

### Q: How do I check if OpenSSL is available?

```cpp
#ifdef ATOM_HAS_OPENSSL
    // OpenSSL-dependent code
#endif
```

### Q: Can I use this module without OpenSSL?

Yes, basic math and compression algorithms work without OpenSSL. Only some cryptographic operations require it.

### Q: How do I enable TBB parallelization?

TBB is automatically detected by CMake. If found, parallel algorithms are enabled automatically.

### Q: What's the performance difference between CPU and GPU implementations?

GPU implementations can be 10-100x faster for large datasets, but have overhead for small datasets. Always benchmark for your specific use case.

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented core APIs and data structures
- Added usage examples and integration notes

---

**Document Maintainer:** Atom Framework Team
**Last Updated:** 2025-01-15
**Module Version:** 1.0.0
