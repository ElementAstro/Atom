/*
 * command.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "command.hpp"

#include <mutex>

namespace atom::system {

// Global mutex for environment operations (used by advanced_executor)
std::mutex envMutex;

}  // namespace atom::system
