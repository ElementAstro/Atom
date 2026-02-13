/*
 * parallel_math.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Parallel mathematical operations

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_PARALLEL_MATH_HPP
#define ATOM_ALGORITHM_MATH_PARALLEL_MATH_HPP

#include <cstdint>
#include <vector>

namespace atom::algorithm {

/**
 * @brief 并行向量加法
 * @param a 输入向量a
 * @param b 输入向量b
 * @return 每个元素为a[i]+b[i]的新向量
 * @throws atom::error::InvalidArgumentException 如果长度不一致
 */
[[nodiscard]] std::vector<uint64_t> parallelVectorAdd(
    const std::vector<uint64_t>& a, const std::vector<uint64_t>& b);

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_PARALLEL_MATH_HPP
