#include "perf.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <string_view>
#include <thread>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// --- Platform-specific implementations ---

namespace perf_internal {

// String Pool implementation for memory efficiency
const char* StringPool::intern(std::string_view str) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pool_.find(std::string(str));
    if (it != pool_.end()) {
        return it->second.get();
    }

    auto ptr = std::make_unique<char[]>(str.size() + 1);
    std::memcpy(ptr.get(), str.data(), str.size());
    ptr[str.size()] = '\0';

    const char* result = ptr.get();
    pool_[std::string(str)] = std::move(ptr);
    return result;
}

void StringPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.clear();
}

// High-resolution timer implementation
std::atomic<double> HighResTimer::ticks_per_ns_{1.0};
std::atomic<bool> HighResTimer::calibrated_{false};

std::uint64_t HighResTimer::now() noexcept {
#ifdef _WIN32
    return PERF_RDTSC();
#elif defined(__x86_64__) || defined(__i386__)
    return PERF_RDTSC();
#else
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

double HighResTimer::to_nanoseconds(std::uint64_t ticks) noexcept {
    if (!calibrated_.load(std::memory_order_acquire)) {
        calibrate();
    }
    return static_cast<double>(ticks) /
           ticks_per_ns_.load(std::memory_order_acquire);
}

void HighResTimer::calibrate() {
    // Calibrate RDTSC against high_resolution_clock
    const int samples = 10;
    std::uint64_t total_ticks = 0;
    std::uint64_t total_ns = 0;

    for (int i = 0; i < samples; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();
        auto start_ticks = now();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        auto end_ticks = now();
        auto end_time = std::chrono::high_resolution_clock::now();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      end_time - start_time)
                      .count();
        auto ticks = end_ticks - start_ticks;

        if (ticks > 0 && ns > 0) {
            total_ticks += ticks;
            total_ns += ns;
        }
    }

    if (total_ns > 0) {
        ticks_per_ns_.store(static_cast<double>(total_ticks) / total_ns,
                            std::memory_order_release);
    }
    calibrated_.store(true, std::memory_order_release);
}

// SIMD-optimized string operations
namespace simd {

bool fast_strcmp(const char* a, const char* b) noexcept {
    if (a == b)
        return true;
    if (!a || !b)
        return false;

#if defined(PERF_HAS_SSE42)
    // Use SSE4.2 string comparison when available
    const size_t chunk_size = 16;
    while (true) {
        __m128i va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a));
        __m128i vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b));

        int cmp = _mm_cmpistrc(
            va, vb,
            _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY);
        if (cmp) {
            // Found difference, fall back to byte comparison
            for (size_t i = 0; i < chunk_size; ++i) {
                if (a[i] != b[i] || a[i] == '\0') {
                    return a[i] == b[i];
                }
            }
        }

        // Check for null terminator
        int null_a =
            _mm_cmpistrc(va, va, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);
        if (null_a)
            break;

        a += chunk_size;
        b += chunk_size;
    }
#endif

    // Fallback to standard comparison
    return std::strcmp(a, b) == 0;
}

size_t fast_strlen(const char* str) noexcept {
    if (!str)
        return 0;

#if defined(PERF_HAS_SSE42)
    const char* start = str;
    const size_t chunk_size = 16;

    // Align to 16-byte boundary for better performance
    while (reinterpret_cast<uintptr_t>(str) % 16 != 0) {
        if (*str == '\0')
            return str - start;
        ++str;
    }

    // Process 16 bytes at a time
    while (true) {
        __m128i chunk = _mm_load_si128(reinterpret_cast<const __m128i*>(str));
        __m128i zeros = _mm_setzero_si128();

        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, zeros));
        if (mask != 0) {
            return (str - start) +
                   std::countr_zero(static_cast<unsigned>(mask));
        }

        str += chunk_size;
    }
#endif

    // Fallback to standard strlen
    return std::strlen(str);
}

void fast_memcpy(void* dst, const void* src, size_t size) noexcept {
#if defined(PERF_HAS_AVX2)
    // Use AVX2 for large copies
    if (size >= 32) {
        const char* s = static_cast<const char*>(src);
        char* d = static_cast<char*>(dst);

        while (size >= 32) {
            __m256i chunk =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(d), chunk);
            s += 32;
            d += 32;
            size -= 32;
        }

        // Handle remaining bytes
        if (size > 0) {
            std::memcpy(d, s, size);
        }
        return;
    }
#endif

    // Fallback to standard memcpy
    std::memcpy(dst, src, size);
}

}  // namespace simd
}  // namespace perf_internal
// --- Perf::Location Implementation ---

#if __cpp_lib_source_location
Perf::Location::Location(std::source_location const& loc, const char* tag)
    : func(loc.function_name()),
      file(loc.file_name()),
      line(static_cast<int>(loc.line())),
      tag(tag ? tag : "") {}

Perf::Location::Location(const char* func, std::source_location const& loc,
                         const char* tag)
    : func(func),
      file(loc.file_name()),
      line(static_cast<int>(loc.line())),
      tag(tag ? tag : "") {}
#endif

Perf::Location::Location(const char* func, const char* file, int line,
                         const char* tag)
    : func(func ? func : "unknown_func"),
      file(file ? file : "unknown_file"),
      line(line),
      tag(tag ? tag : "") {}

bool Perf::Location::operator<(const Location& rhs) const {
    if (auto cmp = perf_internal::simd::fast_strcmp(func, rhs.func); !cmp)
        return func < rhs.func;  // pointer comparison for interned strings
    if (auto cmp = perf_internal::simd::fast_strcmp(file, rhs.file); !cmp)
        return file < rhs.file;
    return line < rhs.line;
}

bool Perf::Location::operator==(const Location& rhs) const noexcept {
    return line == rhs.line &&
           perf_internal::simd::fast_strcmp(func, rhs.func) &&
           perf_internal::simd::fast_strcmp(file, rhs.file);
}

std::size_t Perf::Location::hash() const noexcept {
    if (hash_cache_ != 0)
        return hash_cache_;

    // Compute hash
    std::size_t h1 = std::hash<const char*>{}(func);
    std::size_t h2 = std::hash<const char*>{}(file);
    std::size_t h3 = std::hash<int>{}(line);

    std::size_t result = h1 ^ (h2 << 1) ^ (h3 << 2);
    if (result == 0)
        result = 1;  // Avoid 0 as cached value

    hash_cache_ = result;
    return result;
}

// --- Perf::PerfTableEntry Implementation ---
Perf::PerfTableEntry::PerfTableEntry(std::uint64_t start, std::uint64_t end,
                                     Location loc)
    : threadId(std::this_thread::get_id()),
      t0(start),
      t1(end),
      location(loc) {}  // Copy instead of move for POD-like Location

// --- Perf::PerfFilter Implementation ---

bool Perf::PerfFilter::match(const PerfTableEntry& entry) const {
    const auto duration = entry.t1 - entry.t0;
    if (duration < minDuration)
        return false;

    if (!funcContains.empty() && entry.location.func) {
        return std::string_view(entry.location.func).find(funcContains) !=
               std::string_view::npos;
    }
    return true;
}

// --- Perf::generateFilteredReport Implementation ---

void Perf::generateFilteredReport(const PerfFilter& filter) {
    logger->info("--- Filtered Performance Report ---");
    logger->info("Filter: minDuration={}ns, funcContains='{}'",
                 filter.minDuration, filter.funcContains);

    bool found = false;
    std::shared_lock guard(gathered.table_mutex);  // Use the correct mutex
    for (const auto& entry : gathered.table) {
        if (filter.match(entry)) {
            found = true;
            const auto duration_ns = entry.t1 - entry.t0;
            logger->info("{} ({}:{}) Tag: [{}] Duration: {} ns",
                         entry.location.func, entry.location.file,
                         entry.location.line, entry.location.tag, duration_ns);
        }
    }

    if (!found) {
        logger->info("No entries matched the filter.");
    }
    logger->info("--- End Filtered Report ---");
}

// --- Perf::PerfThreadLocal Implementation ---

void Perf::PerfThreadLocal::startNested(std::uint64_t t0) {
    if (stack_size < stack.size()) {
        stack[stack_size++] = t0;
    }
}

void Perf::PerfThreadLocal::endNested(std::uint64_t t1) {
    if (stack_size > 0) {
        --stack_size;
    }
}

bool Perf::PerfThreadLocal::try_push(const PerfTableEntry& entry) {
    size_t current_tail = tail.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % entries.size();

    if (next_tail == head.load(std::memory_order_acquire)) {
        return false;  // Queue is full
    }

    entries[current_tail] = entry;
    tail.store(next_tail, std::memory_order_release);
    return true;
}

bool Perf::PerfThreadLocal::try_pop(PerfTableEntry& entry) {
    size_t current_head = head.load(std::memory_order_relaxed);

    if (current_head == tail.load(std::memory_order_acquire)) {
        return false;  // Queue is empty
    }

    entry = entries[current_head];
    head.store((current_head + 1) % entries.size(), std::memory_order_release);
    return true;
}

size_t Perf::PerfThreadLocal::size() const {
    size_t h = head.load(std::memory_order_acquire);
    size_t t = tail.load(std::memory_order_acquire);
    return (t + entries.size() - h) % entries.size();
}

// --- Perf::PerfEntry Implementation ---

Perf::PerfEntry::PerfEntry(std::chrono::high_resolution_clock::time_point start,
                           std::chrono::high_resolution_clock::time_point end,
                           Location location, std::thread::id threadId)
    : start_(start),
      end_(end),
      location_(std::move(location)),
      threadId_(threadId) {}

std::chrono::nanoseconds Perf::PerfEntry::duration() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end_ - start_);
}

auto Perf::PerfEntry::startTimeRaw() const {
    return start_.time_since_epoch().count();
}

auto Perf::PerfEntry::endTimeRaw() const {
    return end_.time_since_epoch().count();
}

const Perf::Location& Perf::PerfEntry::location() const { return location_; }

std::thread::id Perf::PerfEntry::threadId() const { return threadId_; }

// --- Perf::PerfAsyncLogger Implementation ---

Perf::PerfAsyncLogger::PerfAsyncLogger() {
    try {
        logger_ = spdlog::basic_logger_mt("perf_async_logger", "perf_async.log",
                                          true);
        logger_->set_pattern("%v");
        logger_->info(
            "StartTimestamp,EndTimestamp,Duration(ns),Function,File,Line,Tag,"
            "ThreadID");
    } catch (const spdlog::spdlog_ex& ex) {
        spdlog::get("console")->error("Failed to create async logger: {}",
                                      ex.what());
        return;
    }

    worker_ = std::thread([this]() { this->run(); });
}

Perf::PerfAsyncLogger::~PerfAsyncLogger() { stop(); }

bool Perf::PerfAsyncLogger::try_log(const PerfTableEntry& entry) noexcept {
    return try_enqueue(entry);
}

void Perf::PerfAsyncLogger::flush() {
    flush_requested_.store(true, std::memory_order_release);

    // Wait for flush to complete
    while (flush_requested_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

bool Perf::PerfAsyncLogger::try_enqueue(const PerfTableEntry& entry) {
    const size_t current_tail = tail_.load(std::memory_order_relaxed);
    const size_t next_tail = (current_tail + 1) % queue_.size();

    if (next_tail == head_.load(std::memory_order_acquire)) {
        entries_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;  // Queue full
    }

    queue_[current_tail] = entry;
    tail_.store(next_tail, std::memory_order_release);
    entries_logged.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool Perf::PerfAsyncLogger::try_dequeue(PerfTableEntry& entry) {
    const size_t current_head = head_.load(std::memory_order_relaxed);

    if (current_head == tail_.load(std::memory_order_acquire)) {
        return false;  // Queue empty
    }

    entry = queue_[current_head];
    head_.store((current_head + 1) % queue_.size(), std::memory_order_release);
    return true;
}

size_t Perf::PerfAsyncLogger::queue_size() const {
    const size_t head = head_.load(std::memory_order_acquire);
    const size_t tail = tail_.load(std::memory_order_acquire);
    if (tail >= head) {
        return tail - head;
    } else {
        return queue_.size() - head + tail;
    }
}

void Perf::PerfAsyncLogger::run() {
    if (!logger_)
        return;

    while (!done_.load(std::memory_order_acquire)) {
        // Process a batch of entries
        process_batch();

        // Check for flush request
        if (flush_requested_.load(std::memory_order_acquire)) {
            // Process remaining entries
            while (queue_size() > 0) {
                process_batch();
            }
            logger_->flush();
            flush_requested_.store(false, std::memory_order_release);
        }

        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Final flush on shutdown
    while (queue_size() > 0) {
        process_batch();
    }
    if (logger_) {
        logger_->flush();
    }
}

void Perf::PerfAsyncLogger::process_batch() {
    size_t batch_count = 0;
    const size_t max_batch = batch_buffer_.size();

    // Dequeue entries into batch buffer
    while (batch_count < max_batch) {
        if (!try_dequeue(batch_buffer_[batch_count])) {
            break;
        }
        ++batch_count;
    }

    // Log the batch
    for (size_t i = 0; i < batch_count; ++i) {
        const auto& entry = batch_buffer_[i];
        const auto duration_ns = entry.t1 - entry.t0;

        logger_->info("{},{},{},{},{},{},{},{}", entry.t0, entry.t1,
                      duration_ns, entry.location.func, entry.location.file,
                      entry.location.line, entry.location.tag,
                      std::hash<std::thread::id>()(entry.threadId));
    }
}

void Perf::PerfAsyncLogger::stop() {
    done_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }

    if (logger_) {
        logger_->flush();
    }
}

// --- Perf::PerfGather Implementation ---

Perf::PerfGather::PerfGather() : output(nullptr) {
    if (const char* env_path = std::getenv("PERF_OUTPUT")) {
        output_path = env_path;
        output = output_path.c_str();
    }
}

Perf::Perf(Location location) : location_(std::move(location)) {
    t0_ = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    perthread.startNested(t0_);
}

Perf::~Perf() {
    const auto t1 =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto duration = t1 - t0_;
    const auto min_duration =
        static_cast<uint64_t>(getConfig().minimumDuration.count());

    if (duration >= min_duration) {
        PerfTableEntry entry(t0_, t1, location_);

        // Try to store in thread-local circular buffer
        if (!perthread.try_push(entry)) {
            // Buffer full, drop entry
        }

        if (getConfig().asyncLogging) {
            asyncLogger.try_log(entry);
        }

        {
            std::lock_guard<std::shared_mutex> guard(gathered.table_mutex);
            gathered.table.push_back(entry);
        }
    }

    perthread.endNested(t1);
}

// --- Static Methods Implementation ---

void Perf::setConfig(const Config& config) {
    config_ = config;

    std::lock_guard<std::shared_mutex> guard(gathered.table_mutex);
    if (config.outputPath.has_value()) {
        gathered.output_path = config.outputPath.value().string();
        gathered.output = gathered.output_path.c_str();
    } else {
        gathered.output_path.clear();
        gathered.output = nullptr;
    }

    // Configure logger based on config
    logger->set_level(spdlog::level::info);
    if (config.outputPath.has_value()) {
        try {
            auto file_sink =
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                    config.outputPath.value().string() + ".log", true);
            logger->sinks().push_back(file_sink);
        } catch (const spdlog::spdlog_ex& ex) {
            logger->error("Failed to add file sink: {}", ex.what());
        }
    }
}

const Perf::Config& Perf::getConfig() { return config_; }

void Perf::PerfGather::exportToJSON(const std::string& filename) {
    const auto& config = Perf::getConfig();
    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    try {
        std::shared_lock<std::shared_mutex> guard(table_mutex);

        nlohmann::json j = nlohmann::json::array();

        for (const auto& entry : table) {
            const auto duration = entry.t1 - entry.t0;
            if (duration < min_duration_ns)
                continue;

            j.push_back(
                {{"func", entry.location.func},
                 {"file", entry.location.file},
                 {"line", entry.location.line},
                 {"start_ns", entry.t0},
                 {"end_ns", entry.t1},
                 {"duration_ns", duration},
                 {"thread_id", std::hash<std::thread::id>()(entry.threadId)},
                 {"tag", entry.location.tag}});
        }

        const auto path = fs::path(filename);
        if (!path.parent_path().empty() && !fs::exists(path.parent_path())) {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            if (ec) {
                Perf::logger->error(
                    "Error creating directory {} for JSON export: {}",
                    path.parent_path().string(), ec.message());
                return;
            }
        }

        std::ofstream out(path);
        if (!out) {
            Perf::logger->error("Failed to open file for writing: {}",
                                filename);
            return;
        }
        out << j.dump(4);
        Perf::logger->info("Exported performance data to {}", filename);

    } catch (const std::exception& e) {
        Perf::logger->error("Error exporting to JSON file {}: {}", filename,
                            e.what());
    }
}

static std::string formatDuration(std::chrono::nanoseconds duration_ns) {
    std::ostringstream oss;
    oss << duration_ns.count() << " ns";

    if (duration_ns >= 1s) {
        oss << " ("
            << std::chrono::duration_cast<std::chrono::duration<double>>(
                   duration_ns)
                   .count()
            << " s)";
    } else if (duration_ns >= 1ms) {
        oss << " ("
            << std::chrono::duration_cast<
                   std::chrono::duration<double, std::milli>>(duration_ns)
                   .count()
            << " ms)";
    } else if (duration_ns >= 1us) {
        oss << " ("
            << std::chrono::duration_cast<
                   std::chrono::duration<double, std::micro>>(duration_ns)
                   .count()
            << " us)";
    }
    return oss.str();
}

void Perf::PerfGather::generateThreadReport() {
    std::map<std::thread::id,
             std::vector<std::reference_wrapper<const PerfTableEntry>>>
        threadData;
    const auto& config = Perf::getConfig();
    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    {
        std::shared_lock<std::shared_mutex> guard(table_mutex);
        for (const auto& entry : table) {
            const auto duration = entry.t1 - entry.t0;
            if (duration >= min_duration_ns) {
                threadData[entry.threadId].emplace_back(entry);
            }
        }
    }

    if (threadData.empty()) {
        Perf::logger->info(
            "No performance data recorded (or none above minimum threshold of "
            "{} ns)",
            min_duration_ns);
        return;
    }

    Perf::logger->info("==========================================");
    Perf::logger->info("Performance Summary Report");
    Perf::logger->info("==========================================");
    Perf::logger->info("Configuration:");
    Perf::logger->info("  Minimum Duration: {} ns", min_duration_ns);
    Perf::logger->info("  Async Logging: {}",
                       config.asyncLogging ? "Enabled" : "Disabled");
    Perf::logger->info("------------------------------------------");
    Perf::logger->info("Total threads with recorded entries: {}",
                       threadData.size());

    for (const auto& [id, entries] : threadData) {
        Perf::logger->info("--- Thread {} ---",
                           std::hash<std::thread::id>()(id));
        Perf::logger->info("  Total entries recorded: {}", entries.size());

        const uint64_t totalDuration =
            std::accumulate(entries.begin(), entries.end(), uint64_t(0),
                            [](uint64_t sum, const auto& entry_ref) {
                                const auto& entry = entry_ref.get();
                                return sum + (entry.t1 - entry.t0);
                            });

        Perf::logger->info("  Total duration recorded: {} ns", totalDuration);

        auto sortedEntries = entries;
        std::sort(sortedEntries.begin(), sortedEntries.end(),
                  [](const auto& a_ref, const auto& b_ref) {
                      const auto& a = a_ref.get();
                      const auto& b = b_ref.get();
                      return (a.t1 - a.t0) > (b.t1 - b.t0);
                  });

        const auto topCount = std::min(size_t(10), sortedEntries.size());
        Perf::logger->info("  Top {} entries by duration:", topCount);

        for (size_t i = 0; i < topCount; ++i) {
            const auto& entry = sortedEntries[i].get();
            const auto duration = entry.t1 - entry.t0;
            std::string message = fmt::format("    {} ", entry.location.func);

            if (entry.location.tag && *entry.location.tag) {
                message += fmt::format("[{}] ", entry.location.tag);
            }

            message += fmt::format("- {} ns ({}:{})", duration,
                                   entry.location.file, entry.location.line);

            Perf::logger->info(message);
        }
    }

    Perf::logger->info("==========================================");
    Perf::logger->info("Overall Top Functions (Across All Threads)");
    Perf::logger->info("==========================================");

    std::vector<std::reference_wrapper<const PerfTableEntry>> allEntries;
    for (const auto& [id, entries] : threadData) {
        allEntries.insert(allEntries.end(), entries.begin(), entries.end());
    }

    std::sort(allEntries.begin(), allEntries.end(),
              [](const auto& a_ref, const auto& b_ref) {
                  const auto& a = a_ref.get();
                  const auto& b = b_ref.get();
                  return (a.t1 - a.t0) > (b.t1 - b.t0);
              });

    const size_t topCount = std::min(size_t(20), allEntries.size());
    Perf::logger->info("Top {} entries by duration:", topCount);

    for (size_t i = 0; i < topCount; ++i) {
        const auto& entry = allEntries[i].get();
        const auto duration = entry.t1 - entry.t0;

        std::string message =
            fmt::format("{:2}. {} ", (i + 1), entry.location.func);

        if (entry.location.tag && *entry.location.tag) {
            message += fmt::format("[{}] ", entry.location.tag);
        }

        message += fmt::format(" - {} ns (Thread {}, {}:{})", duration,
                               std::hash<std::thread::id>()(entry.threadId),
                               entry.location.file, entry.location.line);

        Perf::logger->info(message);
    }

    Perf::logger->info("==========================================");
}

static void writeCsvData(std::shared_ptr<spdlog::logger> csv_logger,
                         const std::vector<Perf::PerfTableEntry>& table,
                         const Perf::Config& config) {
    csv_logger->info(
        "Function,File,Line,Start(ns),End(ns),Duration(ns),ThreadID,Tag");

    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    for (const auto& entry : table) {
        const auto duration = entry.t1 - entry.t0;
        if (duration < min_duration_ns)
            continue;

        std::string tag_value = "";
        if (entry.location.tag && *entry.location.tag) {
            tag_value = entry.location.tag;
        }

        csv_logger->info(
            "{},{},{},{},{},{},{},{}", entry.location.func, entry.location.file,
            entry.location.line, entry.t0, entry.t1, duration,
            std::hash<std::thread::id>()(entry.threadId), tag_value);
    }
}

static void writeFlamegraphData(
    std::shared_ptr<spdlog::logger> flamegraph_logger,
    const std::vector<Perf::PerfTableEntry>& table,
    const Perf::Config& config) {
    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    for (const auto& entry : table) {
        const auto duration = entry.t1 - entry.t0;
        if (duration < min_duration_ns)
            continue;

        std::string line =
            fmt::format("{}:{};{}", entry.location.func, entry.location.file,
                        entry.location.line);

        if (entry.location.tag && *entry.location.tag) {
            line += fmt::format(";{}", entry.location.tag);
        }

        line += fmt::format(" {}", duration);
        flamegraph_logger->info(line);
    }
}

void Perf::finalize() {
    if (getConfig().asyncLogging) {
        asyncLogger.stop();
    }

    const auto& config = getConfig();
    std::string outputPathBase;

    {
        std::shared_lock<std::shared_mutex> guard(gathered.table_mutex);
        if (gathered.output) {
            outputPathBase = gathered.output;
        }
    }

    if (!outputPathBase.empty()) {
        const fs::path basePath(outputPathBase);
        const auto& formats = config.outputFormats;

        auto hasFormat = [&](OutputFormat fmt) {
            return std::any_of(formats.begin(), formats.end(),
                               [=](OutputFormat f) { return f == fmt; });
        };

        if (hasFormat(OutputFormat::JSON)) {
            gathered.exportToJSON((basePath.parent_path() /
                                   (basePath.filename().string() + ".json"))
                                      .string());
        }

        if (hasFormat(OutputFormat::CSV)) {
            const std::string csvFilename =
                (basePath.parent_path() /
                 (basePath.filename().string() + ".csv"))
                    .string();
            try {
                // Copy the table data to avoid holding the lock
                std::vector<PerfTableEntry> table_copy;
                {
                    std::shared_lock<std::shared_mutex> guard(
                        gathered.table_mutex);
                    table_copy = gathered.table;
                }

                auto csv_logger =
                    spdlog::basic_logger_mt("perf_csv", csvFilename, true);
                csv_logger->set_pattern("%v");
                writeCsvData(csv_logger, table_copy, config);
                logger->info("Exported CSV data to {}", csvFilename);
                spdlog::drop("perf_csv");
            } catch (const spdlog::spdlog_ex& ex) {
                logger->error("Failed to create CSV logger: {}", ex.what());
            }
        }

        if (hasFormat(OutputFormat::FLAMEGRAPH)) {
            const std::string foldedFilename =
                (basePath.parent_path() /
                 (basePath.filename().string() + ".folded"))
                    .string();
            const std::string svgFilename =
                (basePath.parent_path() /
                 (basePath.filename().string() + ".svg"))
                    .string();
            try {
                // Copy the table data to avoid holding the lock
                std::vector<PerfTableEntry> table_copy;
                {
                    std::shared_lock<std::shared_mutex> guard(
                        gathered.table_mutex);
                    table_copy = gathered.table;
                }

                auto folded_logger = spdlog::basic_logger_mt(
                    "perf_flamegraph", foldedFilename, true);
                folded_logger->set_pattern("%v");
                writeFlamegraphData(folded_logger, table_copy, config);
                logger->info("Exported flamegraph data to {}", foldedFilename);
                logger->info(
                    "Hint: Use 'flamegraph.pl {} > {}' to generate "
                    "visualization.",
                    foldedFilename, svgFilename);
                spdlog::drop("perf_flamegraph");
            } catch (const spdlog::spdlog_ex& ex) {
                logger->error("Failed to create flamegraph logger: {}",
                              ex.what());
            }
        }
    }

    if (config.generateThreadReport) {
        gathered.generateThreadReport();
    }

    spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) { l->flush(); });
}

// --- Static initialization ---
void Perf::initialize() {
    if (!logger) {
        try {
            logger = spdlog::stdout_color_mt("perf_main");
            logger->set_level(spdlog::level::info);
            logger->set_pattern("[%T] [%l] %v");
        } catch (const spdlog::spdlog_ex& ex) {
            // Fall back to default logger if creation fails
            logger = spdlog::default_logger();
        }
    }
}
