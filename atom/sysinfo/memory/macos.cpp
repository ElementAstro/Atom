/*
 * macos.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory macOS Implementation

**************************************************/

#include "macos.hpp"
#include "common.hpp"

#ifdef __APPLE__
#include "atom/log/loguru.hpp"

#include <mach/mach_init.h>
#include <mach/task_info.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <cstdio>
#include <chrono>
#include <thread>

namespace atom::system {
namespace macos {

auto getMemoryUsage() -> float {
    LOG_F(INFO, "Starting getMemoryUsage function (macOS)");
    float memoryUsage = 0.0;

    struct statfs stats;
    statfs("/", &stats);

    unsigned long long total_space = stats.f_blocks * stats.f_bsize;
    unsigned long long free_space = stats.f_bfree * stats.f_bsize;

    unsigned long long used_space = total_space - free_space;
    memoryUsage = static_cast<float>(used_space) / total_space * 100.0;
    LOG_F(INFO,
          "Total Space: {} bytes, Free Space: {} bytes, Used Space: {} "
          "bytes, Memory Usage: %.2f%%",
          total_space, free_space, used_space, memoryUsage);

    LOG_F(INFO, "Finished getMemoryUsage function (macOS)");
    return memoryUsage;
}

auto getTotalMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getTotalMemorySize function (macOS)");
    unsigned long long totalMemorySize = 0;

    FILE *pipe = popen("sysctl -n hw.memsize", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            totalMemorySize = strtoull(buffer, nullptr, 10);
            LOG_F(INFO, "Total Memory Size: {} bytes", totalMemorySize);
        } else {
            LOG_F(ERROR, "GetTotalMemorySize error: fgets error");
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Finished getTotalMemorySize function (macOS)");
    return totalMemorySize;
}

auto getAvailableMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getAvailableMemorySize function (macOS)");
    unsigned long long availableMemorySize = 0;

    FILE *pipe = popen("vm_stat | grep 'Pages free:' | awk '{print $3}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // macOS页大小通常为4096字节
            availableMemorySize = strtoull(buffer, nullptr, 10) * 4096;
            LOG_F(INFO, "Available Memory Size: {} bytes", availableMemorySize);
        } else {
            LOG_F(ERROR, "GetAvailableMemorySize error: fgets error");
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Finished getAvailableMemorySize function (macOS)");
    return availableMemorySize;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    LOG_F(INFO, "Starting getPhysicalMemoryInfo function (macOS)");
    MemoryInfo::MemorySlot slot;

    FILE *pipe = popen("sysctl hw.memsize | awk '{print $2}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            unsigned long long totalBytes = strtoull(buffer, nullptr, 10);
            slot.capacity = std::to_string(totalBytes / (1024 * 1024)); // Convert to MB
            LOG_F(INFO, "Physical Memory Capacity: {} MB", slot.capacity);
        } else {
            LOG_F(ERROR, "GetPhysicalMemoryInfo error: fgets error");
        }
        pclose(pipe);
    }

    // 获取RAM类型和速度 (macOS中需要使用system_profiler)
    FILE *typePipe = popen("system_profiler SPMemoryDataType | grep 'Type:' | head -n 1 | awk -F': ' '{print $2}'", "r");
    if (typePipe != nullptr) {
        char buffer[128] = {0};
        if (fgets(buffer, sizeof(buffer), typePipe) != nullptr) {
            // 移除末尾的换行符
            buffer[strcspn(buffer, "\r\n")] = 0;
            slot.type = buffer;
        }
        pclose(typePipe);
    }

    FILE *speedPipe = popen("system_profiler SPMemoryDataType | grep 'Speed:' | head -n 1 | awk -F': ' '{print $2}'", "r");
    if (speedPipe != nullptr) {
        char buffer[128] = {0};
        if (fgets(buffer, sizeof(buffer), speedPipe) != nullptr) {
            // 移除末尾的换行符
            buffer[strcspn(buffer, "\r\n")] = 0;
            slot.clockSpeed = buffer;
        }
        pclose(speedPipe);
    }

    LOG_F(INFO, "Finished getPhysicalMemoryInfo function (macOS)");
    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryMax function (macOS)");
    unsigned long long virtualMemoryMax = 0;

    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $4}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // 解析类似于"4096.00M"的字符串
            double value;
            char unit;
            if (sscanf(buffer, "%lf%c", &value, &unit) == 2) {
                if (unit == 'M' || unit == 'm') {
                    virtualMemoryMax = static_cast<unsigned long long>(value);
                } else if (unit == 'G' || unit == 'g') {
                    virtualMemoryMax = static_cast<unsigned long long>(value * 1024);
                }
            }
            LOG_F(INFO, "Virtual Memory Max: {} MB", virtualMemoryMax);
        } else {
            LOG_F(ERROR, "GetVirtualMemoryMax error: fgets error");
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Finished getVirtualMemoryMax function (macOS)");
    return virtualMemoryMax;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryUsed function (macOS)");
    unsigned long long virtualMemoryUsed = 0;

    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $7}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // 解析类似于"100.00M"的字符串
            double value;
            char unit;
            if (sscanf(buffer, "%lf%c", &value, &unit) == 2) {
                if (unit == 'M' || unit == 'm') {
                    virtualMemoryUsed = static_cast<unsigned long long>(value);
                } else if (unit == 'G' || unit == 'g') {
                    virtualMemoryUsed = static_cast<unsigned long long>(value * 1024);
                }
            }
            LOG_F(INFO, "Virtual Memory Used: {} MB", virtualMemoryUsed);
        } else {
            LOG_F(ERROR, "GetVirtualMemoryUsed error: fgets error");
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Finished getVirtualMemoryUsed function (macOS)");
    return virtualMemoryUsed;
}

auto getSwapMemoryTotal() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryTotal function (macOS)");
    unsigned long long swapMemoryTotal = 0;

    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $4}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // 解析类似于"4096.00M"的字符串
            double value;
            char unit;
            if (sscanf(buffer, "%lf%c", &value, &unit) == 2) {
                if (unit == 'M' || unit == 'm') {
                    swapMemoryTotal = static_cast<unsigned long long>(value);
                } else if (unit == 'G' || unit == 'g') {
                    swapMemoryTotal = static_cast<unsigned long long>(value * 1024);
                }
            }
            LOG_F(INFO, "Swap Memory Total: {} MB", swapMemoryTotal);
        } else {
            LOG_F(ERROR, "GetSwapMemoryTotal error: fgets error");
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Finished getSwapMemoryTotal function (macOS)");
    return swapMemoryTotal;
}

auto getSwapMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryUsed function (macOS)");
    unsigned long long swapMemoryUsed = 0;

    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $7}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // 解析类似于"100.00M"的字符串
            double value;
            char unit;
            if (sscanf(buffer, "%lf%c", &value, &unit) == 2) {
                if (unit == 'M' || unit == 'm') {
                    swapMemoryUsed = static_cast<unsigned long long>(value);
                } else if (unit == 'G' || unit == 'g') {
                    swapMemoryUsed = static_cast<unsigned long long>(value * 1024);
                }
            }
            LOG_F(INFO, "Swap Memory Used: {} MB", swapMemoryUsed);
        } else {
            LOG_F(ERROR, "GetSwapMemoryUsed error: fgets error");
        }
        pclose(pipe);
    }

    LOG_F(INFO, "Finished getSwapMemoryUsed function (macOS)");
    return swapMemoryUsed;
}

auto getCommittedMemory() -> size_t {
    LOG_F(INFO, "Starting getCommittedMemory function (macOS)");
    
    // 在macOS中，可以通过总内存减去可用内存来计算已提交内存
    unsigned long long totalMemory = getTotalMemorySize();
    unsigned long long availableMemory = getAvailableMemorySize();
    size_t committedMemory = totalMemory - availableMemory;
    
    LOG_F(INFO, "Committed Memory: {} bytes (macOS)", committedMemory);
    return committedMemory;
}

auto getUncommittedMemory() -> size_t {
    LOG_F(INFO, "Starting getUncommittedMemory function (macOS)");
    
    // 在macOS中，未提交内存就是可用内存
    size_t uncommittedMemory = getAvailableMemorySize();
    
    LOG_F(INFO, "Uncommitted Memory: {} bytes (macOS)", uncommittedMemory);
    return uncommittedMemory;
}

auto getDetailedMemoryStats() -> MemoryInfo {
    LOG_F(INFO, "Starting getDetailedMemoryStats function (macOS)");
    MemoryInfo info;

    // 获取物理内存总量和可用量
    info.totalPhysicalMemory = getTotalMemorySize();
    info.availablePhysicalMemory = getAvailableMemorySize();
    
    // 计算内存使用率
    if (info.totalPhysicalMemory > 0) {
        info.memoryLoadPercentage = ((double)(info.totalPhysicalMemory - info.availablePhysicalMemory) / 
                                    info.totalPhysicalMemory) * 100.0;
    }
    
    // 获取交换内存信息
    info.swapMemoryTotal = getSwapMemoryTotal() * 1024 * 1024; // MB to bytes
    info.swapMemoryUsed = getSwapMemoryUsed() * 1024 * 1024;   // MB to bytes
    
    // 获取虚拟内存信息
    info.virtualMemoryMax = info.totalPhysicalMemory + info.swapMemoryTotal;
    info.virtualMemoryUsed = 
        (info.totalPhysicalMemory - info.availablePhysicalMemory) + info.swapMemoryUsed;
    
    // 获取进程内存信息
    task_basic_info_data_t task_info_data;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&task_info_data, &count) 
        == KERN_SUCCESS) {
        info.workingSetSize = task_info_data.resident_size;
        info.peakWorkingSetSize = info.workingSetSize; // macOS没有直接等效的峰值工作集大小
    }
    
    // macOS中没有直接等效的页错误计数，使用mach接口
    task_events_info_data_t events_info;
    count = TASK_EVENTS_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_EVENTS_INFO, (task_info_t)&events_info, &count) 
        == KERN_SUCCESS) {
        info.pageFaultCount = events_info.faults;
    }
    
    // 获取物理内存插槽信息
    MemoryInfo::MemorySlot slot = getPhysicalMemoryInfo();
    info.slots.push_back(slot);

    LOG_F(INFO, "Finished getDetailedMemoryStats function (macOS)");
    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getPeakWorkingSetSize function (macOS)");
    size_t peakSize = 0;

    // macOS没有直接的峰值工作集大小API，使用当前大小作为估计值
    task_basic_info_data_t task_info_data;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&task_info_data, &count) 
        == KERN_SUCCESS) {
        peakSize = task_info_data.resident_size;
    }

    LOG_F(INFO, "Peak working set size: {} bytes (macOS)", peakSize);
    return peakSize;
}

auto getCurrentWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getCurrentWorkingSetSize function (macOS)");
    size_t currentSize = 0;

    task_basic_info_data_t task_info_data;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&task_info_data, &count) 
        == KERN_SUCCESS) {
        currentSize = task_info_data.resident_size;
    }

    LOG_F(INFO, "Current working set size: {} bytes (macOS)", currentSize);
    return currentSize;
}

auto getPageFaultCount() -> size_t {
    LOG_F(INFO, "Starting getPageFaultCount function (macOS)");
    size_t pageFaults = 0;

    task_events_info_data_t events_info;
    mach_msg_type_number_t count = TASK_EVENTS_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_EVENTS_INFO, (task_info_t)&events_info, &count) 
        == KERN_SUCCESS) {
        pageFaults = events_info.faults;
    }

    LOG_F(INFO, "Page fault count: {} (macOS)", pageFaults);
    return pageFaults;
}

auto getMemoryLoadPercentage() -> double {
    LOG_F(INFO, "Starting getMemoryLoadPercentage function (macOS)");
    
    unsigned long long total = getTotalMemorySize();
    unsigned long long available = getAvailableMemorySize();
    
    double memoryLoad = 0.0;
    if (total > 0) {
        memoryLoad = ((double)(total - available) / total) * 100.0;
    }
    
    LOG_F(INFO, "Memory load: {}% (macOS)", memoryLoad);
    return memoryLoad;
}

auto getMemoryPerformance() -> MemoryPerformance {
    LOG_F(INFO, "Getting memory performance metrics (macOS)");
    MemoryPerformance perf{};

    // macOS没有直接的内存性能计数器，使用简单的基准测试
    const int TEST_SIZE = 1024 * 1024;  // 1MB
    std::vector<int> testData(TEST_SIZE);
    
    // 测量写入性能
    auto writeStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < TEST_SIZE; i++) {
        testData[i] = i;
    }
    auto writeEnd = std::chrono::high_resolution_clock::now();
    double writeTime = std::chrono::duration<double>(writeEnd - writeStart).count();
    double writeSpeed = (TEST_SIZE * sizeof(int)) / (1024 * 1024) / writeTime;
    
    // 测量读取性能
    volatile int sum = 0; // 防止编译器优化
    auto readStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < TEST_SIZE; i++) {
        sum += testData[i];
    }
    auto readEnd = std::chrono::high_resolution_clock::now();
    double readTime = std::chrono::duration<double>(readEnd - readStart).count();
    double readSpeed = (TEST_SIZE * sizeof(int)) / (1024 * 1024) / readTime;
    
    perf.readSpeed = readSpeed;
    perf.writeSpeed = writeSpeed;
    perf.latency = (readTime + writeTime) / (2 * TEST_SIZE) * 1e9; // 纳秒

    // 估算带宽使用率
    double maxBandwidth = 25600.0; // 假设25.6 GB/s的最大理论带宽
    perf.bandwidthUsage = ((readSpeed + writeSpeed) * 1024) / maxBandwidth * 100.0;
    
    LOG_F(INFO,
          "Memory performance metrics: Read: {:.2f} MB/s, Write: {:.2f} MB/s, "
          "Bandwidth: {:.1f}%, Latency: {:.2f} ns (macOS)",
          perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    return perf;
}

} // namespace macos
} // namespace atom::system

#endif // __APPLE__
