/*
 * parallel_math.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Parallel mathematical operations - implementations

**************************************************/

#include "parallel_math.hpp"

#include "atom/error/exception.hpp"

namespace atom::algorithm {

std::vector<uint64_t> parallelVectorAdd(const std::vector<uint64_t>& a,
                                        const std::vector<uint64_t>& b) {
    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Input vectors must have the same length");
    }
    std::vector<uint64_t> result(a.size());
#if defined(_OPENMP)
#pragma omp parallel for
#endif
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
}

}  // namespace atom::algorithm
