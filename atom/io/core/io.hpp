/*
 * io.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file io.hpp
 * @brief Aggregator header for all core I/O functionality.
 *
 * This header includes all sub-component headers for backward compatibility.
 * New code should prefer including the specific sub-header it needs.
 */

#ifndef ATOM_IO_IO_HPP
#define ATOM_IO_IO_HPP

#include "atom/io/core/types.hpp"
#include "atom/io/core/path_convert.hpp"
#include "atom/io/core/file_query.hpp"
#include "atom/io/core/file_ops.hpp"
#include "atom/io/core/directory_ops.hpp"
#include "atom/io/core/directory_walk.hpp"
#include "atom/io/core/file_split_merge.hpp"

#endif  // ATOM_IO_IO_HPP
