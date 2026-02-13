/**
 * @file base.hpp
 * @brief Aggregator header for all encoding algorithms.
 *
 * This header includes all encoding sub-components for backwards compatibility.
 * For new code, prefer including the specific header you need:
 *   - "atom/algorithm/encoding/base64.hpp"
 *   - "atom/algorithm/encoding/base32.hpp"
 *   - "atom/algorithm/encoding/hex.hpp"
 *   - "atom/algorithm/encoding/url.hpp"
 *   - "atom/algorithm/encoding/xor_cipher.hpp"
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_ALGORITHM_BASE16_HPP
#define ATOM_ALGORITHM_BASE16_HPP

#include "base64.hpp"
#include "base32.hpp"
#include "hex.hpp"
#include "url.hpp"
#include "xor_cipher.hpp"

#endif  // ATOM_ALGORITHM_BASE16_HPP
