/*
 * math.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Extra Math Library - Umbrella header

This header includes all math sub-components for backward compatibility.
Users who include math.hpp will automatically get all functionality.

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_MATH_HPP
#define ATOM_ALGORITHM_MATH_MATH_HPP

// Mathematical concepts and type constraints
#include "math_concepts.hpp"

// Safe arithmetic operations with overflow/underflow detection
#include "safe_math.hpp"

// Bit manipulation operations
#include "bit_ops.hpp"

// Number theory functions (GCD, LCM, primes, modular arithmetic)
#include "number_theory.hpp"

// Secure random number generation
#include "random.hpp"

// Custom memory pool and allocator
#include "math_memory.hpp"

// Parallel mathematical operations
#include "parallel_math.hpp"

#endif  // ATOM_ALGORITHM_MATH_MATH_HPP
