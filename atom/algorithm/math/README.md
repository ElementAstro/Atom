# Mathematical Algorithms and Data Structures

This directory contains mathematical computations, numerical algorithms, and mathematical data structures.

## Contents

- **`math.hpp/cpp`** - Extended mathematical functions and number theory utilities
- **`matrix.hpp`** - Template-based matrix operations with compile-time optimizations
- **`fraction.hpp/cpp`** - Rational number arithmetic with automatic simplification
- **`bignumber.hpp/cpp`** - Arbitrary precision arithmetic for large numbers

## Features

### Math Utilities

- **Number Theory**: GCD, LCM, primality testing, prime generation
- **Bit Operations**: Fast bit manipulation functions
- **Safe Arithmetic**: Overflow/underflow detection
- **Parallel Operations**: Multi-threaded mathematical computations
- **Caching**: Thread-safe caching for expensive computations (prime numbers)

### Matrix Operations

- **Compile-Time Matrices**: Template-based matrices with constexpr operations
- **Linear Algebra**: Matrix multiplication, inversion, decomposition
- **SIMD Optimizations**: Vectorized operations where possible
- **Thread Safety**: Concurrent matrix operations

### Fraction Arithmetic

- **Automatic Simplification**: Fractions are automatically reduced to lowest terms
- **Mixed Operations**: Seamless operations between fractions and other numeric types
- **Overflow Protection**: Safe arithmetic with large numerators/denominators

### Big Number Support

- **Arbitrary Precision**: Handle numbers larger than built-in types
- **Performance Optimized**: Efficient algorithms for large number arithmetic
- **String Conversion**: Easy conversion to/from string representations

## Usage Examples

```cpp
#include "atom/algorithm/math/math.hpp"
#include "atom/algorithm/math/matrix.hpp"
#include "atom/algorithm/math/fraction.hpp"

// Number theory
auto gcd_result = atom::algorithm::gcd64(48, 18);  // Returns 6
auto is_prime = atom::algorithm::isPrime(97);      // Returns true

// Matrix operations
atom::algorithm::Matrix<double, 3, 3> mat = atom::algorithm::identity<double, 3>();
auto det = mat.determinant();

// Fraction arithmetic
atom::algorithm::Fraction f1(3, 4);
atom::algorithm::Fraction f2(1, 2);
auto result = f1 + f2;  // 5/4
```

## Performance Considerations

- Prime number generation uses sieve algorithms with caching
- Matrix operations are optimized for small, compile-time known sizes
- SIMD instructions are used where beneficial
- Thread-safe caching reduces repeated computations

## Dependencies

- Core algorithm components
- Standard C++ library
- Optional: TBB for parallel operations
