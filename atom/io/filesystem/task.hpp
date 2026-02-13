#ifndef ATOM_IO_FILESYSTEM_TASK_HPP
#define ATOM_IO_FILESYSTEM_TASK_HPP

/**
 * @file task.hpp
 * @brief Alias for the canonical coroutine Task type from atom::async.
 *
 * This header re-exports atom::async::Task into the atom::io namespace
 * so that filesystem components (e.g. DirectoryStack) can use it without
 * duplicating the coroutine machinery.
 */

#include "atom/async/execution/coroutine_task.hpp"

namespace atom::io {

/**
 * @brief Coroutine Task type for async filesystem operations.
 *
 * Reuses the canonical atom::async::Task implementation.
 * @tparam T The result type of the task.
 */
template <typename T>
using Task = atom::async::Task<T>;

}  // namespace atom::io

#endif  // ATOM_IO_FILESYSTEM_TASK_HPP
