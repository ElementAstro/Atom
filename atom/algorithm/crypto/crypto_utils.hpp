#ifndef ATOM_ALGORITHM_CRYPTO_CRYPTO_UTILS_HPP
#define ATOM_ALGORITHM_CRYPTO_CRYPTO_UTILS_HPP

#include <algorithm>
#include <span>

#include <spdlog/spdlog.h>
#include "atom/algorithm/common/concepts.hpp"  // ByteLike

namespace atom::algorithm {

// ByteType is an alias for the canonical ByteLike concept from common/concepts.hpp
template <typename T>
concept ByteType = ByteLike<T>;

/**
 * @brief Applies PKCS7 padding to the data.
 * @param data The data to pad.
 * @param length The length of the data, will be updated to include padding.
 */
template <ByteType T>
void pkcs7_padding(std::span<T> data, usize& length);

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CRYPTO_CRYPTO_UTILS_HPP
