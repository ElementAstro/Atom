// pch.hpp - Precompiled Header for Atom Library
// This file contains commonly used headers that rarely change
// Include this file in your source files to speed up compilation

#ifndef ATOM_PCH_HPP
#define ATOM_PCH_HPP

// =============================================================================
// C++ Standard Library Headers
// =============================================================================

// Containers
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Strings
#include <string>
#include <string_view>

// Streams
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// Utilities
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

// Threading (conditional - can be expensive)
#ifndef ATOM_PCH_NO_THREADING
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#endif

// Filesystem (C++17)
#if __cplusplus >= 201703L
#include <filesystem>
#endif

// Concepts and Ranges (C++20)
#if __cplusplus >= 202002L
#include <concepts>
#include <ranges>
#include <span>
#endif

// =============================================================================
// Platform-specific Headers
// =============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Note: Windows.h is intentionally not included in PCH
// as it can cause conflicts and is very large
#endif

// =============================================================================
// Third-party Library Headers (commonly used)
// =============================================================================

// fmt library (if available)
#if __has_include(<fmt/format.h>)
#include <fmt/chrono.h>
#include <fmt/format.h>

#endif

// spdlog (if available and configured)
#if __has_include(<spdlog/spdlog.h>) && !defined(ATOM_PCH_NO_SPDLOG)
#include <spdlog/spdlog.h>
#endif

#endif  // ATOM_PCH_HPP
