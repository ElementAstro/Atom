#include "benchmark.hpp"

// Keep necessary includes for platform-specific implementations
#include <cstring>  // Needed for memset
#include <fstream>  // Needed for getMemoryUsage on Linux
// 添加对 filesystem 的支持
#include <filesystem>
// 添加对 JSON 库的支持
#include <nlohmann/json.hpp>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <intrin.h> // For __cpuid, __readpmc
#include <psapi.h> // For GetProcessMemoryInfo
#if defined (__MINGW64__) || defined(__MINGW32__)
// #include <x86intrin.h> // Included via intrin.h with GCC/Clang? Check compiler docs if needed.
#endif
// clang-format on
#elif defined(__linux__)
#include <linux/hw_breakpoint.h> /* Definition of HW_* constants */
#include <linux/perf_event.h>    /* Definition of PERF_* constants */
#include <sys/ioctl.h>
#include <sys/resource.h>  // For getrusage
#include <sys/syscall.h>   /* Definition of SYS_* constants */
#include <unistd.h>        // For sysconf, close, read
#elif defined(__APPLE__)
#include <mach/mach.h>     // For task_info
#include <sys/resource.h>  // For getrusage
#include <sys/sysctl.h>    // For sysctlbyname
#endif

void Benchmark::clearResults() noexcept {
    std::scoped_lock lock(resultsMutex);
    results.clear();
    staticLog(LogLevel::Normal, "Cleared all benchmark results");
}

auto Benchmark::getResults()
    -> const std::map<std::string, std::vector<Result>> {
    std::scoped_lock lock(resultsMutex);
    return results;
}

void Benchmark::setGlobalLogLevel(LogLevel level) noexcept {
    globalLogLevel.store(level, std::memory_order_relaxed);
}

bool Benchmark::isCpuStatsSupported() noexcept {
#ifdef _WIN32
    // Check if CPUID supports leaf 0x0A (Architectural Performance Monitoring)
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);  // Get highest basic calling parameter
    if (cpuInfo[0] >= 0x0A) {
        __cpuid(cpuInfo, 0x0A);
        // Check if Architectural Performance Monitoring is available (EAX[7:0]
        // > 0) and if core cycle and instruction counters are available
        // (EAX[15:8] < 7 implies counters 0 and 1 are available)
        return (cpuInfo[0] & 0xFF) > 0 && ((cpuInfo[0] >> 8) & 0xFF) < 7;
    }
    return false;
#elif defined(__linux__)
    // Try opening a perf event for instructions. If successful, assume support.
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    int fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd == -1) {
        // Consider logging errno here if needed (e.g., EACCES, ENOENT)
        return false;
    }
    close(fd);
    return true;
#elif defined(__APPLE__)
    // macOS does not provide easy access to hardware performance counters like
    // instructions retired. We might be able to get cycles via
    // sysctlbyname("hw.cycles"), but not instructions. Return false for now.
    uint64_t val = 0;
    size_t size = sizeof(val);
    // Check if hw.perflevel0.cycles exists (newer Apple Silicon) or hw.cycles
    // (Intel)
    if (sysctlbyname("hw.perflevel0.cycles", &val, &size, nullptr, 0) == 0)
        return true;
    if (sysctlbyname("hw.cycles", &val, &size, nullptr, 0) == 0)
        return true;  // Check legacy name too
    return false;     // Assume not fully supported if cycles aren't readable
#else
    // Unsupported platform
    return false;
#endif
}

void Benchmark::registerGlobalLogger(
    std::function<void(const std::string&)> logger) noexcept {
    std::scoped_lock lock(logMutex);
    globalLogger = std::move(logger);
}

auto Benchmark::totalDuration(std::span<const Duration> durations) noexcept
    -> Duration {
    return std::accumulate(durations.begin(), durations.end(),
                           Duration::zero());
}

void Benchmark::analyzeResults(std::span<const Duration> durations,
                               std::span<const MemoryStats> memoryStats,
                               std::span<const CPUStats> cpuStats,
                               std::size_t totalOpCount) {
    if (durations.empty()) {
        log(LogLevel::Minimal,
            "Warning: No data to analyze for benchmark: " + name_);
        return;
    }

    std::vector<double> microseconds(durations.size());
    std::ranges::transform(
        durations, microseconds.begin(), [](const Duration& duration) {
            return std::chrono::duration<double, std::micro>(duration).count();
        });

    std::ranges::sort(microseconds);
    const double totalMicroseconds =
        std::accumulate(microseconds.begin(), microseconds.end(), 0.0);
    const auto iterations = static_cast<int>(microseconds.size());

    Result result;
    result.name = name_;
    result.averageDuration = totalMicroseconds / iterations;
    result.minDuration = microseconds.front();
    result.maxDuration = microseconds.back();
    result.medianDuration = iterations % 2 == 0
                                ? (microseconds[iterations / 2 - 1] +
                                   microseconds[iterations / 2]) /
                                      2
                                : microseconds[iterations / 2];
    result.standardDeviation = calculateStandardDeviation(microseconds);
    result.iterations = iterations;
    result.throughput =
        static_cast<double>(totalOpCount) / (totalMicroseconds * 1e-6);
    result.timestamp = getCurrentTimestamp();

    // Format source location if available
    if (sourceLocation_.file_name() && sourceLocation_.file_name()[0] != '\0') {
        result.sourceLine =
            std::format("{}:{}",
                        std::filesystem::path(sourceLocation_.file_name())
                            .filename()
                            .string(),
                        sourceLocation_.line());
    }

    if (!memoryStats.empty() && config_.enableMemoryStats) {
        double totalMemory = 0.0;
        size_t maxPeakUsage = 0;

        for (const auto& stat : memoryStats) {
            totalMemory += stat.currentUsage;
            maxPeakUsage = std::max(maxPeakUsage, stat.peakUsage);
        }

        result.avgMemoryUsage = totalMemory / memoryStats.size();
        result.peakMemoryUsage = static_cast<double>(maxPeakUsage);
    }

    if (!cpuStats.empty() && config_.enableCpuStats) {
        result.avgCPUStats = calculateAverageCpuStats(cpuStats);
        if (result.avgCPUStats->cyclesElapsed > 0) {
            result.instructionsPerCycle =
                static_cast<double>(result.avgCPUStats->instructionsExecuted) /
                result.avgCPUStats->cyclesElapsed;
        }
    }

    // Store results
    {
        std::scoped_lock lock(resultsMutex);
        results[suiteName_].push_back(result);
    }

    // Log summary
    log(LogLevel::Normal,
        std::format("Results for {}: Avg: {:.4f} μs, Throughput: {:.2f} "
                    "ops/sec, Iterations: {}",
                    name_, result.averageDuration, result.throughput,
                    iterations));
}

auto Benchmark::calculateStandardDeviation(
    std::span<const double> values) noexcept -> double {
    if (values.empty()) {
        return 0.0;
    }

    const double mean =
        std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double squaredSum = 0.0;

    for (const double value : values) {
        const double diff = value - mean;
        squaredSum += diff * diff;
    }

    return std::sqrt(squaredSum / values.size());
}

auto Benchmark::calculateAverageCpuStats(
    std::span<const CPUStats> stats) noexcept -> CPUStats {
    if (stats.empty()) {
        return {};
    }

    CPUStats avg{};
    for (const auto& stat : stats) {
        avg.instructionsExecuted += stat.instructionsExecuted;
        avg.cyclesElapsed += stat.cyclesElapsed;
        avg.branchMispredictions += stat.branchMispredictions;
        avg.cacheMisses += stat.cacheMisses;
    }

    const size_t count = stats.size();
    avg.instructionsExecuted /= static_cast<int64_t>(count);
    avg.cyclesElapsed /= static_cast<int64_t>(count);
    avg.branchMispredictions /= static_cast<int64_t>(count);
    avg.cacheMisses /= static_cast<int64_t>(count);

    return avg;
}

void Benchmark::log(LogLevel level, const std::string& message) const {
    if (level <= config_.logLevel || level <= globalLogLevel) {
        if (config_.customLogger) {
            (*config_.customLogger)(message);
        } else {
            staticLog(level, message);
        }
    }
}

void Benchmark::staticLog(LogLevel level, const std::string& message) {
    if (level <= globalLogLevel) {
        if (globalLogger) {
            (*globalLogger)(message);
        } else {
            std::string prefix;
            switch (level) {
                case LogLevel::Minimal:
                    prefix = "[ERROR] ";
                    break;
                case LogLevel::Normal:
                    prefix = "[INFO] ";
                    break;
                case LogLevel::Verbose:
                    prefix = "[DEBUG] ";
                    break;
                default:
                    break;
            }

            std::scoped_lock lock(logMutex);
            std::cout << prefix << message << std::endl;
        }
    }
}

std::string Benchmark::getCurrentTimestamp() noexcept {
    const auto now = std::chrono::system_clock::now();
    const auto now_time_t = std::chrono::system_clock::to_time_t(now);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

auto Benchmark::getMemoryUsage() noexcept -> MemoryStats {
    MemoryStats stats{};

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    ZeroMemory(&pmc, sizeof(pmc));  // Important to zero out the structure
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        stats.currentUsage =
            pmc.WorkingSetSize;  // Current physical memory usage
        stats.peakUsage = pmc.PeakWorkingSetSize;  // Peak physical memory usage
        // pmc.PrivateUsage gives committed memory (Private Bytes) which might
        // also be relevant
    } else {
        // Failed to get memory info, log error?
        // DWORD error = GetLastError();
        // staticLog(LogLevel::Minimal, "Failed to get process memory info.
        // Error code: " + std::to_string(error));
    }
#elif defined(__linux__)
    // Using /proc/self/statm for resident set size (RSS)
    std::ifstream statm("/proc/self/statm");
    if (statm) {
        // Format: size resident shared text lib data dt
        size_t resident_pages = 0;
        statm >> resident_pages >>
            resident_pages;  // Read the first two values, we only need the
                             // second (resident)
        if (!statm.fail()) {
            const long page_size = sysconf(_SC_PAGESIZE);
            if (page_size > 0) {
                stats.currentUsage =
                    resident_pages * static_cast<size_t>(page_size);
            }
        }
        // Note: statm provides RSS, which might not be exactly what's desired
        // (e.g., vs PSS or USS).
    }

    // Using getrusage for peak RSS (ru_maxrss is in KB)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        stats.peakUsage =
            static_cast<size_t>(usage.ru_maxrss) * 1024;  // Convert KB to Bytes
    }
#elif defined(__APPLE__)
    // Using task_info for resident size on macOS
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info),
                  &infoCount) == KERN_SUCCESS) {
        stats.currentUsage = info.resident_size;
        // Peak resident size isn't directly available via task_info.
        // We can use getrusage for peak RSS similar to Linux.
    }

    // Using getrusage for peak RSS (ru_maxrss is in bytes on macOS, unlike
    // Linux)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        stats.peakUsage = static_cast<size_t>(
            usage.ru_maxrss);  // ru_maxrss is in bytes on macOS
        // If currentUsage wasn't set by task_info, we might use usage.ru_ixrss
        // + usage.ru_idrss + usage.ru_isrss as an approximation, but
        // resident_size is better.
    }
#endif

    return stats;
}

// Helper struct for managing perf_event file descriptors on Linux
#ifdef __linux__
struct PerfEvent {
    int fd = -1;
    uint64_t id = 0;  // Optional: Store event ID if using groups

    PerfEvent() = default;

    explicit PerfEvent(uint32_t type, uint64_t config) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(pe));
        pe.type = type;
        pe.size = sizeof(pe);
        pe.config = config;
        pe.disabled = 1;                  // Start disabled
        pe.exclude_kernel = 1;            // Exclude kernel space
        pe.exclude_hv = 1;                // Exclude hypervisor
        pe.read_format = PERF_FORMAT_ID;  // Include ID in read data

        fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1,
                     0);  // Measure current process, any CPU
        if (fd != -1) {
            // Read the event ID
            // This isn't strictly necessary for individual events but useful
            // for groups uint64_t read_buf[2]; // format: { value, id } if
            // (read(fd, read_buf, sizeof(read_buf)) == sizeof(read_buf)) {
            //     id = read_buf[1];
            // }
        } else {
            // Handle error (e.g., log, throw, or just leave fd as -1)
            // std::cerr << "Failed to open perf event: " << strerror(errno) <<
            // std::endl;
        }
    }

    ~PerfEvent() {
        if (fd != -1) {
            close(fd);
        }
    }

    // Disable copy/move semantics for simplicity
    PerfEvent(const PerfEvent&) = delete;
    PerfEvent& operator=(const PerfEvent&) = delete;
    PerfEvent(PerfEvent&&) = delete;
    PerfEvent& operator=(PerfEvent&&) = delete;

    void enable() {
        if (fd != -1) {
            ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        }
    }

    void disable() {
        if (fd != -1) {
            ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        }
    }

    void reset() {
        if (fd != -1) {
            ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        }
    }

    int64_t read_value() {
        int64_t value = 0;
        if (fd != -1) {
            // Read format depends on PERF_FORMAT flags used during open
            // With PERF_FORMAT_ID: { value, id }
            // Without: { value }
            // For simplicity, assuming no PERF_FORMAT_GROUP or complex formats
            uint64_t read_buf[2];  // Max size needed for {value, id}
            ssize_t bytes_read = read(fd, read_buf, sizeof(read_buf));

            if (bytes_read >= static_cast<ssize_t>(sizeof(uint64_t))) {
                value = static_cast<int64_t>(read_buf[0]);
            } else {
                // Handle read error
                // std::cerr << "Failed to read perf event: " << strerror(errno)
                // << std::endl;
                value = -1;  // Indicate error
            }
        }
        return value;
    }
};
#endif  // __linux__

auto Benchmark::getCpuStats() noexcept -> CPUStats {
    CPUStats stats = {};

#ifdef _WIN32
    // Using __rdpmc intrinsic requires specific setup (enabling counters).
    // QueryPerformanceCounter is generally available but only measures time.
    // For reliable hardware counters on Windows, external libraries (like PAPI)
    // or direct driver interaction might be needed.
    // The __rdpmc approach below is often restricted or requires kernel
    // privileges. Let's provide a basic implementation attempt, acknowledging
    // limitations.

    // Check if RDPMC is supported and enabled (requires kernel setup usually)
    // This check is basic and might not guarantee RDPMC works as expected.
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x80000001);  // Check extended features
    bool rdtscp_supported =
        (cpuInfo[3] & (1 << 27));  // Check RDTSCP bit (often related)

    // Attempt to read counters if supported (may fail or return zeros)
    // Note: The counter indices (0, 1, 2, 3) are examples and depend on the
    // specific CPU and MSR configuration. Common mappings (Intel): 0:
    // Instructions retired, 1: Unhalted core cycles These might not be
    // configured by default.
    if (rdtscp_supported) {  // Use RDTSCP support as a proxy for potential
                             // RDPMC usability
        unsigned int counter_index = 0;  // Example: Instructions Retired
        unsigned int aux = 0;
// stats.instructionsExecuted = static_cast<int64_t>(__rdpmc(counter_index)); //
// MSVC intrinsic

// GCC/Clang intrinsic style:
#if defined(__GNUC__) || defined(__clang__)
        unsigned int eax, edx;
        // Read Instructions Retired (Counter 0xC0) - Common but not guaranteed
        __asm__ volatile("rdpmc" : "=a"(eax), "=d"(edx) : "c"(0xC0));
        stats.instructionsExecuted =
            (static_cast<unsigned long long>(edx) << 32) | eax;

        // Read Unhalted Core Cycles (Counter 0x3C) - Common but not guaranteed
        __asm__ volatile("rdpmc" : "=a"(eax), "=d"(edx) : "c"(0x3C));
        stats.cyclesElapsed =
            (static_cast<unsigned long long>(edx) << 32) | eax;

        // Reading branch/cache misses requires knowing the specific event codes
        // for the CPU and ensuring the corresponding counters are programmed.
        // This is complex. Example placeholder for Branch Mispredictions
        // Retired (Counter 0xC5)
        // __asm__ volatile("rdpmc" : "=a"(eax), "=d"(edx) : "c"(0xC5));
        // stats.branchMispredictions = (static_cast<unsigned long long>(edx) <<
        // 32) | eax;

#elif defined(_MSC_VER)
        // MSVC intrinsic version (requires counter setup)
        stats.instructionsExecuted = static_cast<int64_t>(
            __rdpmc(0));  // Assuming counter 0 = instructions
        stats.cyclesElapsed =
            static_cast<int64_t>(__rdpmc(1));  // Assuming counter 1 = cycles
        // stats.branchMispredictions = static_cast<int64_t>(__rdpmc(2)); //
        // Assuming counter 2 = branch misses stats.cacheMisses =
        // static_cast<int64_t>(__rdpmc(3)); // Assuming counter 3 = cache
        // misses
#endif
    }
    // If RDPMC is unavailable/unreliable, these stats will likely remain 0.

#elif defined(__linux__)
    // Linux implementation using perf_event (more reliable than Windows RDPMC
    // approach) Create PerfEvent objects for each desired counter Note:
    // Creating/destroying these on every call might add overhead. Consider
    // creating them once per benchmark run if performance is critical.
    PerfEvent instr_event(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
    PerfEvent cycle_event(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
    PerfEvent branch_miss_event(PERF_TYPE_HARDWARE,
                                PERF_COUNT_HW_BRANCH_MISSES);
    PerfEvent cache_miss_event(
        PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);  // LLC misses usually

    // Enable counters
    instr_event.enable();
    cycle_event.enable();
    branch_miss_event.enable();
    cache_miss_event.enable();

    // --- Code being measured would execute here ---
    // In this function, we just read the current values.
    // The diff logic happens in the run() method.

    // Disable counters before reading
    instr_event.disable();
    cycle_event.disable();
    branch_miss_event.disable();
    cache_miss_event.disable();

    // Read values
    stats.instructionsExecuted = instr_event.read_value();
    stats.cyclesElapsed = cycle_event.read_value();
    stats.branchMispredictions = branch_miss_event.read_value();
    stats.cacheMisses = cache_miss_event.read_value();

    // Handle potential read errors (-1)
    if (stats.instructionsExecuted < 0)
        stats.instructionsExecuted = 0;
    if (stats.cyclesElapsed < 0)
        stats.cyclesElapsed = 0;
    if (stats.branchMispredictions < 0)
        stats.branchMispredictions = 0;
    if (stats.cacheMisses < 0)
        stats.cacheMisses = 0;

#elif defined(__APPLE__)
    // macOS implementation - limited CPU stats available via sysctl
    uint64_t cycles = 0;
    size_t size = sizeof(cycles);
    // Try newer M1/M2 style name first, then fallback to older Intel style name
    if (sysctlbyname("hw.perflevel0.cycles", &cycles, &size, nullptr, 0) == 0) {
        stats.cyclesElapsed = static_cast<int64_t>(cycles);
    } else if (sysctlbyname("hw.cycles", &cycles, &size, nullptr, 0) == 0) {
        stats.cyclesElapsed = static_cast<int64_t>(cycles);
    }

    // Instructions retired might be available on newer chips
    uint64_t instructions = 0;
    size = sizeof(instructions);
    if (sysctlbyname("hw.perflevel0.instructions", &instructions, &size,
                     nullptr, 0) == 0) {
        stats.instructionsExecuted = static_cast<int64_t>(instructions);
    } else if (sysctlbyname("hw.instructions", &instructions, &size, nullptr,
                            0) == 0) {
        stats.instructionsExecuted = static_cast<int64_t>(instructions);
    }

    // Branch and cache miss stats are generally not available via sysctl.
    stats.branchMispredictions = 0;  // Unavailable
    stats.cacheMisses = 0;           // Unavailable

#endif

    return stats;
}
