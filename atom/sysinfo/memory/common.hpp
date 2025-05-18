/**
 * @file common.hpp
 * @brief Common definitions for memory information module
 *
 * This file contains common definitions and utilities used by the memory
 * information module across different platforms.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <atomic>

// 引入必要的日志库
#include "atom/log/loguru.hpp"

namespace atom::system {

// 声明前置
struct MemoryInfo;
struct MemoryPerformance;

// 内部工具函数
namespace internal {

// 监控线程控制变量
extern std::atomic<bool> g_monitoringActive;

// 将字节转换为可读的大小字符串 (KB, MB, GB等)
auto formatByteSize(unsigned long long bytes) -> std::string;

// 计算内存性能的基准测试
auto benchmarkMemoryPerformance(size_t testSizeBytes = 1024 * 1024) -> double;

} // namespace internal

} // namespace atom::system

#endif // ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP
