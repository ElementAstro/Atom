/**
 * @file index.hpp
 * @brief Barrel export header for the atom::algorithm module.
 *
 * This header re-exports all public sub-module headers so that consumers
 * can include a single file:
 *
 *   #include "atom/algorithm/index.hpp"
 *
 * Individual sub-module headers remain available for fine-grained inclusion.
 */

#ifndef ATOM_ALGORITHM_INDEX_HPP
#define ATOM_ALGORITHM_INDEX_HPP

// ---------------------------------------------------------------------------
// Algorithm exception hierarchy
// ---------------------------------------------------------------------------
#include "algorithm_exception.hpp"

// ---------------------------------------------------------------------------
// Core algorithms
// ---------------------------------------------------------------------------
#include "core/algorithm.hpp"
#include "core/kmp.hpp"
#include "core/boyer_moore.hpp"
#include "core/bloom_filter.hpp"
#include "core/rust_numeric.hpp"

// ---------------------------------------------------------------------------
// Cryptography
// ---------------------------------------------------------------------------
#include "crypto/md5.hpp"
#include "crypto/sha1.hpp"
#include "crypto/blowfish.hpp"
#include "crypto/tea.hpp"
#include "crypto/xtea.hpp"
#include "crypto/xxtea.hpp"

// ---------------------------------------------------------------------------
// Hash
// ---------------------------------------------------------------------------
#include "hash/hash.hpp"
#include "hash/mhash.hpp"
#include "hash/keccak.hpp"
#include "hash/minhash.hpp"

// ---------------------------------------------------------------------------
// Math
// ---------------------------------------------------------------------------
#include "math/math.hpp"
#include "math/matrix.hpp"
#include "math/fraction.hpp"
#include "math/bignumber.hpp"
#include "math/safe_math.hpp"
#include "math/number_theory.hpp"
#include "math/statistics.hpp"

// ---------------------------------------------------------------------------
// Compression
// ---------------------------------------------------------------------------
#include "compression/huffman.hpp"
#include "compression/huffman_optimized.hpp"
#include "compression/matrix_compress.hpp"

// ---------------------------------------------------------------------------
// Signal processing
// ---------------------------------------------------------------------------
#include "signal/convolve.hpp"
#include "signal/dft.hpp"
#include "signal/gaussian_filter.hpp"

// ---------------------------------------------------------------------------
// Optimization
// ---------------------------------------------------------------------------
#include "optimization/annealing.hpp"
#include "optimization/pathfinding.hpp"

// ---------------------------------------------------------------------------
// Encoding
// ---------------------------------------------------------------------------
#include "encoding/base.hpp"
#include "encoding/base64.hpp"
#include "encoding/base32.hpp"
#include "encoding/hex.hpp"
#include "encoding/url.hpp"
#include "encoding/xor_cipher.hpp"

// ---------------------------------------------------------------------------
// Graphics
// ---------------------------------------------------------------------------
#include "graphics/flood.hpp"
#include "graphics/perlin.hpp"
#include "graphics/simplex.hpp"

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
#include "utils/fnmatch.hpp"
#include "utils/snowflake.hpp"
#include "utils/weight.hpp"
#include "utils/error_calibration.hpp"
#include "utils/uuid.hpp"

#endif  // ATOM_ALGORITHM_INDEX_HPP
