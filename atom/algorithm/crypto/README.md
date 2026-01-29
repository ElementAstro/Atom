# Cryptographic Algorithms

This directory contains cryptographic hash functions and encryption algorithms.

## Contents

- **`md5.hpp/cpp`** - MD5 hash algorithm implementation with modern C++ features
- **`sha1.hpp/cpp`** - SHA-1 hash algorithm with SIMD optimizations
- **`blowfish.hpp/cpp`** - Blowfish symmetric encryption algorithm
- **`tea.hpp/cpp`** - TEA (Tiny Encryption Algorithm) and XTEA implementations

## Features

- **Modern C++ Design**: Uses concepts, constexpr, and RAII patterns
- **Performance Optimized**: SIMD instructions where available (AVX2)
- **Thread Safe**: All implementations are thread-safe
- **Exception Safe**: Proper error handling with custom exception types
- **Binary Data Support**: Works with std::span and byte containers

## Security Note

⚠️ **Important**: MD5 and SHA-1 are cryptographically broken and should not be used for security-critical applications. They are provided for compatibility and non-security use cases only.

For secure applications, consider using:

- SHA-256 or SHA-3 for hashing
- AES for symmetric encryption
- Modern authenticated encryption schemes

## Usage Examples

```cpp
#include "atom/algorithm/crypto/md5.hpp"
#include "atom/algorithm/crypto/sha1.hpp"

// MD5 hashing
auto md5_hash = atom::algorithm::MD5::encrypt("Hello, World!");

// SHA-1 hashing
atom::algorithm::SHA1 sha1;
sha1.update("Hello, World!");
auto sha1_hash = sha1.digestAsString();
```

## Dependencies

- Core algorithm components (rust_numeric.hpp)
- OpenSSL (for some implementations)
- spdlog for logging
