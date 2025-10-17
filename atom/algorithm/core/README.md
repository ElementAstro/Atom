# Core Algorithm Components

This directory contains the fundamental building blocks and common utilities used throughout the algorithm module.

## Contents

- **`rust_numeric.hpp`** - Rust-style numeric type aliases and utilities (i8, u8, i32, u32, f32, f64, etc.)
- **`algorithm.hpp/cpp`** - Core algorithm concepts, base classes, and common functionality

## Purpose

The core directory provides:

- Type definitions and concepts used across all algorithm implementations
- Common base classes and interfaces
- Fundamental utilities that other algorithm categories depend on

## Dependencies

- Standard C++ library
- spdlog for logging
- atom/error for exception handling

## Usage

These files are typically included indirectly through the backward compatibility headers in the parent directory. For new code, prefer including specific headers:

```cpp
#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/algorithm/core/algorithm.hpp"
```

## Note

This directory contains the most fundamental components that other algorithm categories depend on. Changes here may affect the entire algorithm module.
