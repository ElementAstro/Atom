/*
 * detail.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Shared detail utilities for encoding algorithms.
             ByteContainer concept is now forwarded from
             atom/algorithm/common/concepts.hpp.

**************************************************/

#ifndef ATOM_ALGORITHM_ENCODING_DETAIL_HPP
#define ATOM_ALGORITHM_ENCODING_DETAIL_HPP

#include "atom/algorithm/common/concepts.hpp"  // ByteContainer

namespace atom::algorithm {

namespace detail {

// ByteContainer concept is now defined in atom/algorithm/common/concepts.hpp.
// Import it into the detail namespace for backward compatibility.
using atom::algorithm::ByteContainer;

}  // namespace detail

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_ENCODING_DETAIL_HPP
