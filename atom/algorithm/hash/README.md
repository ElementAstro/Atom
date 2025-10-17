# Hash Algorithms and Utilities

This directory contains general-purpose hashing algorithms and utilities for data processing and analysis.

## Contents

- **`hash.hpp`** - High-performance hash functions with SIMD optimizations and caching
- **`mhash.hpp/cpp`** - Multi-hash utilities including MinHash, Keccak, and similarity estimation

## Features

- **Multiple Hash Algorithms**: FNV-1a, xxHash, CityHash, MurmurHash3
- **SIMD Optimizations**: AVX2 instructions for improved performance
- **Thread-Safe Caching**: LRU cache for frequently computed hashes
- **Parallel Processing**: Multi-threaded hash computation
- **Similarity Estimation**: MinHash for Jaccard similarity estimation
- **Modern C++ Concepts**: Type-safe interfaces with concepts

## Use Cases

- **Data Deduplication**: Fast hash computation for identifying duplicate data
- **Hash Tables**: High-quality hash functions for hash table implementations
- **Similarity Analysis**: MinHash for approximate similarity between sets
- **Checksums**: Fast checksums for data integrity verification
- **Distributed Systems**: Consistent hashing for load balancing

## Usage Examples

```cpp
#include "atom/algorithm/hash/hash.hpp"
#include "atom/algorithm/hash/mhash.hpp"

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
- TBB for parallel processing
- Optional: Boost for additional containers
