#ifndef ATOM_IO_ASYNC_ASYNC_IO_HPP
#define ATOM_IO_ASYNC_ASYNC_IO_HPP

/**
 * @file async_io.hpp
 * @brief Aggregate header for all asynchronous I/O components.
 *
 * This header provides backward compatibility by including all split
 * async I/O components. For more targeted includes, use the individual
 * headers directly:
 *   - async_types.hpp      : Concepts, AsyncContext, AsyncResult, Task
 *   - async_file.hpp       : AsyncFile core file operations
 *   - async_directory.hpp  : AsyncDirectoryOps directory operations
 *   - async_batch.hpp      : AsyncBatchOps batch operations
 *   - async_stream.hpp     : AsyncStreamOps streaming operations
 *   - async_simd.hpp       : SIMD buffer utility functions
 */

#include "async_types.hpp"
#include "async_file.hpp"
#include "async_directory.hpp"
#include "async_batch.hpp"
#include "async_stream.hpp"
#include "async_simd.hpp"

#endif  // ATOM_IO_ASYNC_ASYNC_IO_HPP
