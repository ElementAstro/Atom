# Data Encoding and Decoding Algorithms

This directory contains algorithms for encoding and decoding data in various formats.

## Contents

- **`base.hpp/cpp`** - Base32 and Base64 encoding/decoding with SIMD optimizations

## Features

### Base64 Encoding

- **Standard Base64**: RFC 4648 compliant implementation
- **URL-Safe Variant**: URL and filename safe Base64 encoding
- **SIMD Optimizations**: AVX2/SSE2 vectorized operations for bulk encoding
- **Streaming Support**: Process data without loading entire datasets
- **Exception Safety**: Robust error handling and validation

### Base32 Encoding

- **Standard Base32**: RFC 4648 compliant implementation
- **Case Insensitive**: Supports both uppercase and lowercase decoding
- **Padding Options**: Configurable padding behavior
- **Error Detection**: Comprehensive input validation

### XOR Encryption

- **Simple XOR Cipher**: Basic XOR encryption for obfuscation
- **Key Scheduling**: Support for variable-length keys
- **In-Place Operations**: Memory-efficient encryption/decryption

## Performance Features

- **SIMD Acceleration**: Up to 4x speedup with AVX2 instructions
- **Zero-Copy Operations**: Minimize memory allocations
- **Batch Processing**: Optimized for large datasets
- **Cache-Friendly**: Memory access patterns optimized for modern CPUs

## Use Cases

### Base64

- **Email Attachments**: MIME encoding for binary data
- **Web APIs**: JSON-safe binary data transmission
- **Data URLs**: Embedding binary data in text formats
- **Configuration Files**: Storing binary data in text-based configs

### Base32

- **Human-Readable IDs**: Case-insensitive identifiers
- **QR Codes**: Efficient encoding for QR code generation
- **File Names**: Safe encoding for filesystem compatibility
- **Backup Codes**: User-friendly authentication codes

### XOR Encryption

- **Data Obfuscation**: Simple protection against casual inspection
- **Stream Ciphers**: Building block for more complex encryption
- **Checksums**: Simple error detection mechanisms
- **Testing**: Deterministic encryption for unit tests

## Usage Examples

```cpp
#include "atom/algorithm/encoding/base.hpp"

// Base64 encoding
std::string data = "Hello, World!";
auto encoded = atom::algorithm::encodeBase64(data);
auto decoded = atom::algorithm::decodeBase64(encoded.value());

// Base32 encoding
auto base32_encoded = atom::algorithm::encodeBase32(
    std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()),
        data.size()
    )
);

// XOR encryption
std::string key = "secret";
auto encrypted = atom::algorithm::xorEncrypt(data, key);
auto decrypted = atom::algorithm::xorDecrypt(encrypted, key);
```

## Error Handling

All encoding functions return `atom::type::expected<T>` for safe error handling:

```cpp
auto result = atom::algorithm::decodeBase64("invalid_base64");
if (result) {
    // Success - use result.value()
    std::string decoded = result.value();
} else {
    // Error - handle result.error()
    std::string error_msg = result.error();
}
```

## Performance Notes

- SIMD optimizations provide significant speedup for large datasets
- Streaming interfaces minimize memory usage for large files
- Input validation is optimized to fail fast on invalid data
- Memory allocations are minimized through careful buffer management

## Dependencies

- Core algorithm components
- atom/type for `expected<T>` error handling
- Standard C++ library (C++20)
- Optional: SIMD intrinsics for vectorization
