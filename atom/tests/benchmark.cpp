#include "benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

// Platform-specific includes
#ifdef _WIN32
#include <intrin.h>
#include <psapi.h>
#include <windows.h>
#elif defined(__linux__)
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#endif

#include "benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

// Platform-specific includes
#ifdef _WIN32
#include <intrin.h>
#include <psapi.h>
#include <windows.h>
#elif defined(__linux__)
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#endif

// =============================================================================
// Platform-specific PerfEvent implementation for Linux
// =============================================================================
#ifdef __linux__
struct PerfEvent {
    int fd = -1;
    uint64_t id = 0;

    PerfEvent() = default;

    explicit PerfEvent(uint32_t type, uint64_t config) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(pe));
        pe.type = type;
        pe.size = sizeof(pe);
        pe.config = config;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        pe.read_format = PERF_FORMAT_ID;

        fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    }

    ~PerfEvent() {
        if (fd != -1) {
            close(fd);
        }
    }

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
            uint64_t read_buf[2];
            ssize_t bytes_read = read(fd, read_buf, sizeof(read_buf));
            if (bytes_read >= static_cast<ssize_t>(sizeof(uint64_t))) {
                value = static_cast<int64_t>(read_buf[0]);
            } else {
                value = -1;
            }
        }
        return value;
    }
};
#endif  // __linux__

// =============================================================================
// Platform-specific system information functions
// =============================================================================
auto Benchmark::getMemoryUsage() noexcept -> MemoryStats {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        return MemoryStats{static_cast<size_t>(pmc.WorkingSetSize),
                           static_cast<size_t>(pmc.PeakWorkingSetSize)};
    }
#elif defined(__linux__)
    std::ifstream statm("/proc/self/statm");
    if (statm.is_open()) {
        size_t size, resident, shared, text, lib, data, dt;
        if (statm >> size >> resident >> shared >> text >> lib >> data >> dt) {
            size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
            return MemoryStats{
                resident * page_size,
                resident * page_size  // Linux doesn't easily give peak RSS
                                      // without parsing status
            };
        }
    }
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info),
                  &infoCount) == KERN_SUCCESS) {
        return MemoryStats{static_cast<size_t>(info.resident_size),
                           static_cast<size_t>(info.resident_size_max)};
    }
#endif
    return MemoryStats{};
}

auto Benchmark::getCpuStats() noexcept -> CPUStats {
    CPUStats stats;

#ifdef __linux__
    // Use thread-local storage for perf events to avoid overhead
    thread_local static std::unique_ptr<PerfEvent> instructions_event;
    thread_local static std::unique_ptr<PerfEvent> cycles_event;
    thread_local static std::unique_ptr<PerfEvent> branch_misses_event;
    thread_local static std::unique_ptr<PerfEvent> cache_misses_event;

    // Initialize events on first use
    if (!instructions_event) {
        instructions_event = std::make_unique<PerfEvent>(
            PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        cycles_event = std::make_unique<PerfEvent>(PERF_TYPE_HARDWARE,
                                                   PERF_COUNT_HW_CPU_CYCLES);
        branch_misses_event = std::make_unique<PerfEvent>(
            PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);
        cache_misses_event = std::make_unique<PerfEvent>(
            PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
    }

    if (instructions_event && instructions_event->fd != -1) {
        stats.instructionsExecuted = instructions_event->read_value();
    }
    if (cycles_event && cycles_event->fd != -1) {
        stats.cyclesElapsed = cycles_event->read_value();
    }
    if (branch_misses_event && branch_misses_event->fd != -1) {
        stats.branchMispredictions = branch_misses_event->read_value();
    }
    if (cache_misses_event && cache_misses_event->fd != -1) {
        stats.cacheMisses = cache_misses_event->read_value();
    }

#elif defined(_WIN32)
    // Use Windows performance counters if available
    LARGE_INTEGER frequency, counter;
    if (QueryPerformanceFrequency(&frequency) &&
        QueryPerformanceCounter(&counter)) {
        stats.cyclesElapsed = counter.QuadPart;
    }

// Try to use RDTSC for cycle counting on x86/x64
#if defined(_M_X64) || defined(_M_IX86)
    stats.cyclesElapsed = __rdtsc();
#endif

#elif defined(__APPLE__)
    // Use mach absolute time for cycle approximation
    stats.cyclesElapsed = static_cast<int64_t>(mach_absolute_time());
#endif

    return stats;
}

bool Benchmark::isCpuStatsSupported() noexcept {
#ifdef __linux__
    // Test if we can open a simple perf event
    PerfEvent test_event(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
    return test_event.fd != -1;
#elif defined(_WIN32)
    // Windows has basic performance counter support
    LARGE_INTEGER frequency;
    return QueryPerformanceFrequency(&frequency) != 0;
#elif defined(__APPLE__)
    // macOS has mach_absolute_time
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Utility functions
// =============================================================================
std::string Benchmark::getCurrentTimestamp() noexcept {
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
        return ss.str();
    } catch (...) {
        return "timestamp_error";
    }
}

auto Benchmark::calculateStandardDeviation(std::span<const double> values,
                                           double mean) noexcept -> double {
    if (values.size() < 2) {
        return 0.0;
    }

    double sq_sum = std::accumulate(values.begin(), values.end(), 0.0,
                                    [mean](double accumulator, double val) {
                                        double diff = val - mean;
                                        return accumulator + diff * diff;
                                    });
    return std::sqrt(sq_sum / (values.size() - 1));
}

auto Benchmark::calculateAverageCpuStats(
    std::span<const CPUStats> stats) noexcept -> CPUStats {
    if (stats.empty()) {
        return CPUStats{};
    }

    CPUStats total{};
    for (const auto& s : stats) {
        total.instructionsExecuted += s.instructionsExecuted;
        total.cyclesElapsed += s.cyclesElapsed;
        total.branchMispredictions += s.branchMispredictions;
        total.cacheMisses += s.cacheMisses;
    }

    size_t count = stats.size();
    CPUStats result;
    result.instructionsExecuted =
        total.instructionsExecuted / static_cast<int64_t>(count);
    result.cyclesElapsed = total.cyclesElapsed / static_cast<int64_t>(count);
    result.branchMispredictions =
        total.branchMispredictions / static_cast<int64_t>(count);
    result.cacheMisses = total.cacheMisses / static_cast<int64_t>(count);
    return result;
}

// =============================================================================
// Analysis and Results
// =============================================================================
void Benchmark::analyzeResults(std::span<const Duration> durations,
                               std::span<const MemoryStats> memoryStats,
                               std::span<const CPUStats> cpuStats,
                               std::size_t totalOpCount) {
    if (durations.empty()) {
        throw std::invalid_argument("No duration data to analyze");
    }

    Result result;
    result.name = name_;
    result.iterations = static_cast<int>(durations.size());
    result.timestamp = getCurrentTimestamp();
    result.sourceLine = std::string(sourceLocation_.file_name()) + ":" +
                        std::to_string(sourceLocation_.line());

    // Convert durations to microseconds for analysis
    std::vector<double> durations_us;
    durations_us.reserve(durations.size());
    for (const auto& d : durations) {
        durations_us.push_back(
            std::chrono::duration<double, std::micro>(d).count());
    }

    // Basic time stats
    double total_duration_us =
        std::accumulate(durations_us.begin(), durations_us.end(), 0.0);
    result.averageDuration = total_duration_us / durations_us.size();
    std::sort(durations_us.begin(), durations_us.end());
    result.minDuration = durations_us.front();
    result.maxDuration = durations_us.back();
    result.medianDuration = (durations_us.size() % 2 != 0)
                                ? durations_us[durations_us.size() / 2]
                                : (durations_us[durations_us.size() / 2 - 1] +
                                   durations_us[durations_us.size() / 2]) /
                                      2.0;
    result.standardDeviation =
        calculateStandardDeviation(durations_us, result.averageDuration);

    // Throughput (ops per second)
    double total_duration_sec = total_duration_us / 1'000'000.0;
    if (total_duration_sec > 0 && totalOpCount > 0) {
        result.throughput =
            static_cast<double>(totalOpCount) / total_duration_sec;
    } else {
        result.throughput = 0.0;
    }

    // Memory stats analysis
    if (config_.enableMemoryStats && !memoryStats.empty()) {
        double avgCurrent = 0.0, avgPeak = 0.0;
        for (const auto& ms : memoryStats) {
            avgCurrent += static_cast<double>(ms.currentUsage);
            avgPeak += static_cast<double>(ms.peakUsage);
        }
        result.avgMemoryUsage = avgCurrent / memoryStats.size();
        result.peakMemoryUsage = avgPeak / memoryStats.size();
    }

    // CPU stats analysis
    if (config_.enableCpuStats && !cpuStats.empty()) {
        auto avgStats = calculateAverageCpuStats(cpuStats);
        result.avgCPUStats = avgStats;
        result.instructionsPerCycle = avgStats.getIPC();
    }

    // Store the result
    std::lock_guard lock(resultsMutex);
    results[suiteName_].push_back(std::move(result));
}

std::string Benchmark::Result::toString() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3);

    ss << "Benchmark: " << name << " (" << iterations << " iterations)\n";
    ss << "  Location: " << sourceLine << "\n";
    ss << "  Timestamp: " << timestamp << "\n";
    ss << "  Time (us): Avg=" << averageDuration << ", Min=" << minDuration
       << ", Max=" << maxDuration << ", Median=" << medianDuration
       << ", StdDev=" << standardDeviation << "\n";

    if (throughput > 0) {
        ss << "  Throughput: " << std::setprecision(0) << throughput
           << " ops/s\n";
    }

    if (avgMemoryUsage.has_value()) {
        ss << "  Memory: Avg=" << *avgMemoryUsage << " bytes";
        if (peakMemoryUsage.has_value()) {
            ss << ", Peak=" << *peakMemoryUsage << " bytes";
        }
        ss << "\n";
    }

    if (avgCPUStats.has_value()) {
        const auto& cpu = *avgCPUStats;
        ss << "  CPU: Instructions=" << cpu.instructionsExecuted
           << ", Cycles=" << cpu.cyclesElapsed;
        if (instructionsPerCycle.has_value()) {
            ss << ", IPC=" << std::setprecision(3) << *instructionsPerCycle;
        }
        ss << "\n";
    }

    return ss.str();
}

// =============================================================================
// Export and reporting functions
// =============================================================================
void Benchmark::printResults(const std::string& suite) {
    std::lock_guard lock(resultsMutex);
    if (results.empty()) {
        staticLog(LogLevel::Normal, "No benchmark results available");
        return;
    }

    staticLog(LogLevel::Normal, "--- Benchmark Results ---");
    for (const auto& [suiteName, suiteResults] : results) {
        if (!suite.empty() && suiteName != suite)
            continue;

        staticLog(LogLevel::Normal, "Suite: " + suiteName);
        for (const auto& result : suiteResults) {
            staticLog(LogLevel::Normal, result.toString());
        }
    }
    staticLog(LogLevel::Normal, "-------------------------");
}

void Benchmark::exportResults(const std::string& filename) {
    exportResults(filename, ExportFormat::PlainText);
}

void Benchmark::exportResults(const std::string& filename,
                              ExportFormat format) {
    std::lock_guard lock(resultsMutex);
    if (results.empty()) {
        staticLog(LogLevel::Normal, "No benchmark results to export");
        return;
    }

    // Auto-detect format from extension if not explicitly set
    if (format == ExportFormat::PlainText) {
        std::filesystem::path path(filename);
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".json")
            format = ExportFormat::Json;
        else if (ext == ".csv")
            format = ExportFormat::Csv;
        else if (ext == ".md" || ext == ".markdown")
            format = ExportFormat::Markdown;
    }

    std::ofstream outFile(filename);
    if (!outFile) {
        throw std::runtime_error("Failed to open file for writing: " +
                                 filename);
    }

    try {
        switch (format) {
            case ExportFormat::PlainText:
            default: {
                outFile << "=== Benchmark Results ===\n";
                outFile << "Generated: " << getCurrentTimestamp() << "\n\n";

                for (const auto& [suiteName, suiteResults] : results) {
                    outFile << "Suite: " << suiteName << "\n";
                    outFile << std::string(50, '-') << "\n";

                    for (const auto& result : suiteResults) {
                        outFile << result.toString() << "\n";
                    }
                    outFile << "\n";
                }
                break;
            }

            case ExportFormat::Csv: {
                // CSV header
                outFile << "Suite,Name,Iterations,AvgDuration(us),MinDuration("
                           "us),MaxDuration(us),"
                        << "MedianDuration(us),StdDev(us),Throughput(ops/"
                           "s),AvgMemory(bytes),"
                        << "PeakMemory(bytes),IPC,SourceLine,Timestamp\n";

                for (const auto& [suiteName, suiteResults] : results) {
                    for (const auto& result : suiteResults) {
                        outFile << suiteName << "," << result.name << ","
                                << result.iterations << "," << std::fixed
                                << std::setprecision(3)
                                << result.averageDuration << ","
                                << result.minDuration << ","
                                << result.maxDuration << ","
                                << result.medianDuration << ","
                                << result.standardDeviation << ","
                                << result.throughput << ",";

                        if (result.avgMemoryUsage.has_value()) {
                            outFile << *result.avgMemoryUsage;
                        }
                        outFile << ",";

                        if (result.peakMemoryUsage.has_value()) {
                            outFile << *result.peakMemoryUsage;
                        }
                        outFile << ",";

                        if (result.instructionsPerCycle.has_value()) {
                            outFile << *result.instructionsPerCycle;
                        }
                        outFile << ",";

                        outFile << "\"" << result.sourceLine << "\",\""
                                << result.timestamp << "\"\n";
                    }
                }
                break;
            }

            case ExportFormat::Markdown: {
                outFile << "# Benchmark Results\n\n";
                outFile << "Generated: " << getCurrentTimestamp() << "\n\n";

                for (const auto& [suiteName, suiteResults] : results) {
                    outFile << "## " << suiteName << "\n\n";
                    outFile << "| Name | Iterations | Avg (μs) | Min (μs) | "
                               "Max (μs) | Median (μs) | StdDev (μs) | "
                               "Throughput (ops/s) |\n";
                    outFile << "|------|------------|----------|----------|----"
                               "------|-------------|-------------|------------"
                               "--------|\n";

                    for (const auto& result : suiteResults) {
                        outFile
                            << "| " << result.name << " | " << result.iterations
                            << " | " << std::fixed << std::setprecision(3)
                            << result.averageDuration << " | "
                            << result.minDuration << " | " << result.maxDuration
                            << " | " << result.medianDuration << " | "
                            << result.standardDeviation << " | "
                            << result.throughput << " |\n";
                    }
                    outFile << "\n";
                }
                break;
            }

            case ExportFormat::Json: {
                outFile << "{\n";
                outFile << "  \"metadata\": {\n";
                outFile << "    \"timestamp\": \"" << getCurrentTimestamp()
                        << "\",\n";
                outFile << "    \"platform\": \"";
#ifdef _WIN32
                outFile << "Windows";
#elif defined(__linux__)
                outFile << "Linux";
#elif defined(__APPLE__)
                outFile << "macOS";
#else
                outFile << "Unknown";
#endif
                outFile << "\"\n  },\n";
                outFile << "  \"suites\": {\n";

                bool firstSuite = true;
                for (const auto& [suiteName, suiteResults] : results) {
                    if (!firstSuite)
                        outFile << ",\n";
                    outFile << "    \"" << suiteName << "\": [\n";

                    bool firstResult = true;
                    for (const auto& result : suiteResults) {
                        if (!firstResult)
                            outFile << ",\n";
                        outFile << "      {\n";
                        outFile << "        \"name\": \"" << result.name
                                << "\",\n";
                        outFile
                            << "        \"iterations\": " << result.iterations
                            << ",\n";
                        outFile << "        \"averageDuration\": "
                                << result.averageDuration << ",\n";
                        outFile
                            << "        \"minDuration\": " << result.minDuration
                            << ",\n";
                        outFile
                            << "        \"maxDuration\": " << result.maxDuration
                            << ",\n";
                        outFile << "        \"medianDuration\": "
                                << result.medianDuration << ",\n";
                        outFile << "        \"standardDeviation\": "
                                << result.standardDeviation << ",\n";
                        outFile
                            << "        \"throughput\": " << result.throughput
                            << ",\n";
                        outFile << "        \"sourceLine\": \""
                                << result.sourceLine << "\",\n";
                        outFile << "        \"timestamp\": \""
                                << result.timestamp << "\"";

                        if (result.avgMemoryUsage.has_value()) {
                            outFile << ",\n        \"avgMemoryUsage\": "
                                    << *result.avgMemoryUsage;
                        }
                        if (result.peakMemoryUsage.has_value()) {
                            outFile << ",\n        \"peakMemoryUsage\": "
                                    << *result.peakMemoryUsage;
                        }
                        if (result.instructionsPerCycle.has_value()) {
                            outFile << ",\n        \"instructionsPerCycle\": "
                                    << *result.instructionsPerCycle;
                        }

                        outFile << "\n      }";
                        firstResult = false;
                    }
                    outFile << "\n    ]";
                    firstSuite = false;
                }
                outFile << "\n  }\n}\n";
                break;
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to write benchmark results: " +
                                 std::string(e.what()));
    }

    staticLog(LogLevel::Normal, "Benchmark results exported to: " + filename);
}

void Benchmark::clearResults() noexcept {
    std::lock_guard lock(resultsMutex);
    results.clear();
}

auto Benchmark::getResults()
    -> const std::map<std::string, std::vector<Benchmark::Result>> {
    std::lock_guard lock(resultsMutex);
    return results;
}

// =============================================================================
// Logging functions
// =============================================================================
void Benchmark::setGlobalLogLevel(LogLevel level) noexcept {
    globalLogLevel.store(level, std::memory_order_relaxed);
}

void Benchmark::registerGlobalLogger(
    std::function<void(const std::string&)> logger) noexcept {
    std::lock_guard lock(logMutex);
    globalLogger = std::move(logger);
}

void Benchmark::staticLog(LogLevel level, const std::string& message) {
    if (level == LogLevel::Silent)
        return;

    LogLevel currentGlobalLevel =
        globalLogLevel.load(std::memory_order_relaxed);
    if (level > currentGlobalLevel && level != LogLevel::Minimal)
        return;

    std::lock_guard lock(logMutex);
    if (globalLogger) {
        (*globalLogger)(message);
    } else {
        std::cout << "[BENCHMARK] " << message << std::endl;
    }
}

void Benchmark::log(LogLevel level, const std::string& message) const {
    LogLevel effectiveLevel =
        (config_.logLevel != LogLevel::Normal)
            ? config_.logLevel
            : globalLogLevel.load(std::memory_order_relaxed);

    if (level == LogLevel::Silent || level > effectiveLevel)
        return;

    std::lock_guard lock(logMutex);
    if (config_.customLogger) {
        (*config_.customLogger)(message);
    } else {
        staticLog(level, message);
    }
}

void Benchmark::validateInputs() const {
    if (suiteName_.empty()) {
        throw std::invalid_argument("Suite name cannot be empty");
    }
    if (name_.empty()) {
        throw std::invalid_argument("Benchmark name cannot be empty");
    }
    if (config_.minIterations <= 0) {
        throw std::invalid_argument("minIterations must be positive");
    }
    if (config_.minDurationSec <= 0.0) {
        throw std::invalid_argument("minDurationSec must be positive");
    }
    if (config_.maxIterations &&
        *config_.maxIterations < static_cast<size_t>(config_.minIterations)) {
        throw std::invalid_argument(
            "maxIterations cannot be less than minIterations");
    }
    if (config_.maxDurationSec &&
        *config_.maxDurationSec < config_.minDurationSec) {
        throw std::invalid_argument(
            "maxDurationSec cannot be less than minDurationSec");
    }
    if (config_.enableCpuStats && !isCpuStatsSupported()) {
        log(LogLevel::Normal,
            "Warning: CPU statistics requested but not supported on this "
            "platform");
    }
}
