# Hash Algorithms and Utilities

This directory contains general-purpose hashing algorithms and utilities for data processing and analysis.

## Contents

### Core Hash (aggregated by `hash.hpp`)

- **`hash_base.hpp`** - Core hash functions (FNV-1a with SIMD), `Hashable` concept, `HashAlgorithm` enum, `hashCombine`, `verifyHash`, `operator""_hash`
- **`hash_compute.hpp`** - `HashCache<T>` thread-safe cache and `computeHash()` overloads for vector, tuple, array, pair, optional, variant, any

### Multi-Hash Utilities (aggregated by `mhash.hpp`)

- **`hex_utils.hpp/cpp`** - Hexadecimal string conversion utilities (`hexstringFromData`, `dataFromHexstring`, `supportsHexStringConversion`)
- **`minhash.hpp/cpp`** - MinHash algorithm for Jaccard similarity estimation with optional OpenCL acceleration
- **`keccak.hpp/cpp`** - Keccak-256 cryptographic hash function
- **`hash_context.hpp/cpp`** - RAII-style hash context using OpenSSL EVP interface

### Aggregation Headers

- **`hash.hpp`** - Includes `hash_base.hpp` + `hash_compute.hpp`
- **`mhash.hpp`** - Includes `hex_utils.hpp` + `minhash.hpp` + `keccak.hpp` + `hash_context.hpp`

## Features

- **Multiple Hash Algorithms**: FNV-1a, xxHash, CityHash, MurmurHash3
- **SIMD Optimizations**: AVX2 instructions for improved performance
- **Thread-Safe Caching**: LRU cache for frequently computed hashes
- **Parallel Processing**: Multi-threaded hash computation
- **Similarity Estimation**: MinHash for Jaccard similarity estimation
- **Cryptographic Hashing**: Keccak-256 and OpenSSL-based HashContext
- **Modern C++ Concepts**: Type-safe interfaces with concepts

## Use Cases

- **Data Deduplication**: Fast hash computation for identifying duplicate data
- **Hash Tables**: High-quality hash functions for hash table implementations
- **Similarity Analysis**: MinHash for approximate similarity between sets
- **Checksums**: Fast checksums for data integrity verification
- **Distributed Systems**: Consistent hashing for load balancing

## Usage Examples

```cpp
// Use aggregation headers for convenience
#include "atom/algorithm/hash/hash.hpp"
#include "atom/algorithm/hash/mhash.hpp"

// Or include specific sub-components
#include "atom/algorithm/hash/hash_base.hpp"    // Core hash only
#include "atom/algorithm/hash/minhash.hpp"      // MinHash only
#include "atom/algorithm/hash/keccak.hpp"       // Keccak-256 only

// Basic hashing
auto hash_value = atom::algorithm::computeHash("Hello, World!");

// MinHash for similarity
atom::algorithm::MinHash minhash(100);
auto signature1 = minhash.computeSignature({"a", "b", "c"});
auto signature2 = minhash.computeSignature({"b", "c", "d"});
auto similarity = atom::algorithm::MinHash::jaccardIndex(signature1, signature2);
```

## Performance Notes

- Hash functions are optimized with SIMD instructions when available
- Thread-local caching reduces computation overhead for repeated hashes
- Parallel hash computation available for large datasets

## Dependencies

- Core algorithm components
- OpenSSL (for HashContext)
- Optional: Boost for additional containers
- Optional: OpenCL for GPU-accelerated MinHash
