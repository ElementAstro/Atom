/**
 * @file index.hpp
 * @brief Barrel export header for the atom::io module.
 *
 * This header re-exports all public sub-module headers so that consumers
 * can include a single file:
 *
 *   #include "atom/io/index.hpp"
 *
 * Individual sub-module headers remain available for fine-grained inclusion.
 */

#ifndef ATOM_IO_INDEX_HPP
#define ATOM_IO_INDEX_HPP

// ---------------------------------------------------------------------------
// Core I/O
// ---------------------------------------------------------------------------
#include "core/io.hpp"
#include "core/glob.hpp"

// ---------------------------------------------------------------------------
// Filesystem utilities
// ---------------------------------------------------------------------------
#include "filesystem/file_info.hpp"
#include "filesystem/file_permission.hpp"
#include "filesystem/pushd.hpp"

// ---------------------------------------------------------------------------
// Compression
// ---------------------------------------------------------------------------
#include "compression/compress.hpp"

// ---------------------------------------------------------------------------
// Async (available only when ASIO is present)
// ---------------------------------------------------------------------------
#ifdef ATOM_USE_ASIO
#include "async/async_io.hpp"
#include "async/async_compress.hpp"
#include "async/async_glob.hpp"
#endif  // ATOM_USE_ASIO

#endif  // ATOM_IO_INDEX_HPP
