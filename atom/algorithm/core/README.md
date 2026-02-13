# Core Algorithm Components

This directory contains the fundamental building blocks and common utilities used throughout the algorithm module.

## Contents

### Rust-style Numeric Types (split from `rust_numeric.hpp`)

- **`rust_types.hpp`** — Primitive type aliases (i8, u8, i32, u32, f32, f64, usize, isize)
- **`rust_error.hpp`** — `ErrorKind` enum and `Error` class
- **`rust_result.hpp`** — `Result<T>` monadic error handling
- **`rust_option.hpp`** — `Option<T>` nullable value wrapper
- **`rust_range.hpp`** — `Range<T>` iterable range with `range()` / `range_inclusive()`
- **`rust_int_methods.hpp`** — `IntMethods<Int>` checked/saturating/wrapping arithmetic + concrete types (I8–Usize)
- **`rust_float_methods.hpp`** — `FloatMethods<Float>` math/trig/conversion + F32/F64
- **`rust_iter.hpp`** — `Ord<T>`, `MapIterator`, `FilterIterator`, `EnumerateIterator` and adapters
- **`rust_numeric.hpp`** — **Aggregate header** that includes all of the above

### String Searching Algorithms (split from `algorithm.hpp/.cpp`)

- **`kmp.hpp` / `kmp.cpp`** — KMP string searching algorithm
- **`bloom_filter.hpp`** — Bloom filter data structure (header-only template)
- **`boyer_moore.hpp` / `boyer_moore.cpp`** — Boyer-Moore string searching algorithm
- **`algorithm.hpp`** — **Aggregate header** that includes all of the above

### Acceleration Utilities

- **`simd_utils.hpp`** — SIMD-optimized memory and math operations (SSE2/AVX2/NEON)
- **`opencl_utils.hpp` / `opencl_utils.cpp`** — OpenCL compute abstractions
- **`hex_utils.hpp`** — Hexadecimal conversion utilities

## Dependencies

- Standard C++ library
- spdlog for logging
- atom/error for exception handling

## Usage

Aggregate headers provide backward compatibility — existing `#include` paths continue to work:

```cpp
#include "atom/algorithm/core/rust_numeric.hpp"  // all rust-style types
#include "atom/algorithm/core/algorithm.hpp"     // all search algorithms
```

For finer-grained includes, use individual headers:

```cpp
#include "atom/algorithm/core/rust_option.hpp"   // only Option<T>
#include "atom/algorithm/core/kmp.hpp"           // only KMP
```

## Note

This directory contains the most fundamental components that other algorithm categories depend on. Changes here may affect the entire algorithm module.
