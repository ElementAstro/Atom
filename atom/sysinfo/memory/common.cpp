/*
 * common.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory Common Implementation

**************************************************/

#include "common.hpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>
#include "memory.hpp"


#ifdef _WIN32
#include <processthreadsapi.h>
#include <windows.h>
#endif

namespace atom::system {
namespace internal {

// 监控线程控制变量初始化
std::atomic<bool> g_monitoringActive(false);

// 将字节转换为可读的大小字符串
auto formatByteSize(unsigned long long bytes) -> std::string {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 6) {
        size /= 1024.0;
        unitIndex++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return ss.str();
}

// 执行内存性能的基准测试
auto benchmarkMemoryPerformance(size_t testSizeBytes) -> double {
    // 分配内存并执行读写操作来测试性能
    std::vector<char> testBuffer(testSizeBytes);

    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 写测试
    for (size_t i = 0; i < testSizeBytes; i++) {
        testBuffer[i] = static_cast<char>(i & 0xFF);
    }

    // 读测试
    volatile char sum = 0;  // 防止编译器优化掉读取操作
    for (size_t i = 0; i < testSizeBytes; i++) {
        sum += testBuffer[i];
    }

    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算每秒处理的数据量 (以MB/s为单位)
    double seconds = std::chrono::duration<double>(end - start).count();
    double mbProcessed = static_cast<double>(testSizeBytes * 2) /
                         (1024 * 1024);  // *2 因为读和写
    double throughput = mbProcessed / seconds;

    return throughput;  // 返回MB/s
}

}  // namespace internal

// 实现部分通用函数

// 监控函数实现
auto startMemoryMonitoring(std::function<void(const MemoryInfo&)> callback)
    -> void {
    // 如果已经在监控中，直接返回
    if (internal::g_monitoringActive.exchange(true)) {
        LOG_F(WARNING, "Memory monitoring is already active");
        return;
    }

    LOG_F(INFO, "Starting memory monitoring");

    // 启动监控线程
    std::thread monitorThread([callback]() {
        while (internal::g_monitoringActive) {
            try {
                // 获取内存信息
                MemoryInfo info = getDetailedMemoryStats();

                // 调用回调函数
                callback(info);

                // 每秒更新一次
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error in memory monitoring thread: %s", e.what());
                break;
            }
        }
        LOG_F(INFO, "Memory monitoring stopped");
    });

    // 分离线程，让它在后台运行
    monitorThread.detach();
}

// 停止监控
auto stopMemoryMonitoring() -> void {
    bool expected = true;
    if (internal::g_monitoringActive.compare_exchange_strong(expected, false)) {
        LOG_F(INFO, "Stopping memory monitoring");
    } else {
        LOG_F(WARNING, "Memory monitoring is not active");
    }
}

// 获取内存时间线
auto getMemoryTimeline(std::chrono::minutes duration)
    -> std::vector<MemoryInfo> {
    LOG_F(INFO, "Collecting memory timeline for %ld minutes", duration.count());

    std::vector<MemoryInfo> timeline;
    auto endTime = std::chrono::steady_clock::now() + duration;

    while (std::chrono::steady_clock::now() < endTime) {
        try {
            MemoryInfo info = getDetailedMemoryStats();
            timeline.push_back(info);

            // 每秒采样一次
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error collecting memory timeline: %s", e.what());
            break;
        }
    }

    LOG_F(INFO, "Collected %zu memory samples", timeline.size());
    return timeline;
}

// 内存泄漏检测 (简易实现)
auto detectMemoryLeaks() -> std::vector<std::string> {
    LOG_F(INFO, "Starting memory leak detection");
    std::vector<std::string> leaks;

    // 这里只是一个简单的示例实现，真正的内存泄漏检测需要更复杂的工具
    // 比如在Windows上使用ETW或在Linux上用valgrind等专业工具

    // 获取当前内存使用情况
    MemoryInfo before = getDetailedMemoryStats();

    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 再次获取内存使用情况
    MemoryInfo after = getDetailedMemoryStats();

    // 如果内存占用持续增长，可能存在泄漏
    if (after.workingSetSize >
        before.workingSetSize + 1024 * 1024) {  // 增加了超过1MB
        std::stringstream ss;
        ss << "Potential memory leak detected: Working set increased by "
           << internal::formatByteSize(after.workingSetSize -
                                       before.workingSetSize)
           << " in 5 seconds";
        leaks.push_back(ss.str());
    }

    LOG_F(INFO, "Memory leak detection completed, found %zu potential issues",
          leaks.size());
    return leaks;
}

// 内存碎片分析 (简易实现)
auto getMemoryFragmentation() -> double {
    LOG_F(INFO, "Calculating memory fragmentation");

    // 实际的内存碎片分析需要访问底层内存分配器的数据
    // 这里提供一个简化的估算方法

    // 获取当前可用内存和总内存
    auto total = getTotalMemorySize();
    auto available = getAvailableMemorySize();
    auto used = total - available;

    // 分配一大块内存，看能分配多少
    size_t allocatableSize = 0;
    try {
        // 尝试分配的最大大小
        const size_t MAX_ALLOC_SIZE = 1024 * 1024 * 100;  // 100 MB
        std::vector<char> testAlloc;
        testAlloc.reserve(MAX_ALLOC_SIZE);
        allocatableSize = testAlloc.capacity();
    } catch (...) {
        // 分配失败的情况下，尝试更小的分配
        allocatableSize = 0;
    }

    // 碎片率计算：(理论可用内存 - 实际可分配内存) / 理论可用内存
    double fragmentation = 0.0;
    if (available > 0) {
        fragmentation = 1.0 - (static_cast<double>(allocatableSize) /
                               static_cast<double>(available));
        fragmentation =
            std::max(0.0, std::min(fragmentation, 1.0));  // 限制在 0-1 范围
    }

    LOG_F(INFO, "Memory fragmentation estimated at %.2f%%",
          fragmentation * 100.0);
    return fragmentation * 100.0;  // 返回百分比
}

// 优化内存使用
auto optimizeMemoryUsage() -> bool {
    LOG_F(INFO, "Attempting to optimize memory usage");

    bool success = false;

#ifdef _WIN32
    // Windows平台的内存优化
    try {
        // 尝试强制垃圾回收
        SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
        success = true;
    } catch (...) {
        LOG_F(ERROR, "Failed to optimize memory on Windows");
    }
#elif defined(__linux__)
    // Linux平台的内存优化
    try {
        // 尝试释放缓存的内存页
        FILE* fp = fopen("/proc/self/oom_score_adj", "w");
        if (fp) {
            fprintf(fp, "500\n");  // 增加OOM killer的优先级
            fclose(fp);
            success = true;
        }
    } catch (...) {
        LOG_F(ERROR, "Failed to optimize memory on Linux");
    }
#endif

    LOG_F(INFO, "Memory optimization %s", success ? "succeeded" : "failed");
    return success;
}

// 分析内存瓶颈
auto analyzeMemoryBottlenecks() -> std::vector<std::string> {
    LOG_F(INFO, "Analyzing memory bottlenecks");

    std::vector<std::string> bottlenecks;

    // 获取内存性能指标
    MemoryPerformance perf = getMemoryPerformance();
    MemoryInfo info = getDetailedMemoryStats();

    // 内存使用率过高
    if (info.memoryLoadPercentage > 90.0) {
        bottlenecks.push_back(
            "High memory usage: " +
            std::to_string(static_cast<int>(info.memoryLoadPercentage)) +
            "% of physical memory is in use.");
    }

    // 交换空间使用率过高
    if (info.swapMemoryTotal > 0) {
        double swapUsagePercent = static_cast<double>(info.swapMemoryUsed) /
                                  info.swapMemoryTotal * 100.0;
        if (swapUsagePercent > 50.0) {
            bottlenecks.push_back(
                "High swap usage: " +
                std::to_string(static_cast<int>(swapUsagePercent)) +
                "% of swap space is in use, which may indicate insufficient "
                "RAM.");
        }
    }

    // 内存延迟过高
    if (perf.latency > 100.0) {  // 假设100纳秒是一个阈值
        bottlenecks.push_back(
            "High memory latency: " +
            std::to_string(static_cast<int>(perf.latency)) +
            " ns, which may slow down memory-intensive operations.");
    }

    // 内存带宽使用率过高
    if (perf.bandwidthUsage > 80.0) {
        bottlenecks.push_back(
            "High memory bandwidth usage: " +
            std::to_string(static_cast<int>(perf.bandwidthUsage)) +
            "%, which may indicate memory bandwidth bottleneck.");
    }

    // 检查内存碎片率
    double fragPercent = getMemoryFragmentation();
    if (fragPercent > 30.0) {
        bottlenecks.push_back("High memory fragmentation: " +
                              std::to_string(static_cast<int>(fragPercent)) +
                              "%, which may cause memory allocation failures.");
    }

    LOG_F(INFO, "Memory bottleneck analysis completed, found %zu issues",
          bottlenecks.size());
    return bottlenecks;
}

}  // namespace atom::system
