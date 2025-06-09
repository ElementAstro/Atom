#include "perf.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <numeric>
#include <string_view>
#include <thread>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;
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
    if (auto cmp = std::strcmp(func, rhs.func); cmp != 0)
        return cmp < 0;
    if (auto cmp = std::strcmp(file, rhs.file); cmp != 0)
        return cmp < 0;
    return line < rhs.line;
}

// --- Perf::PerfTableEntry Implementation ---
Perf::PerfTableEntry::PerfTableEntry(std::uint64_t start, std::uint64_t end,
                                     Location loc)
    : threadId(std::this_thread::get_id()),
      t0(start),
      t1(end),
      location(std::move(loc)) {}

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
    std::lock_guard guard(gathered.lock);
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
    stack.push_back(t0);
}

void Perf::PerfThreadLocal::endNested(std::uint64_t t1) {
    if (!stack.empty()) {
        stack.pop_back();
    }
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

Perf::PerfAsyncLogger::PerfAsyncLogger() : done(false) {
    try {
        logger = spdlog::basic_logger_mt("perf_async_logger", "perf_async.log",
                                         true);
        logger->set_pattern("%v");
        logger->info(
            "StartTimestamp,EndTimestamp,Duration(ns),Function,File,Line,Tag,"
            "ThreadID");
    } catch (const spdlog::spdlog_ex& ex) {
        Perf::logger->error("Failed to create async logger: {}", ex.what());
        return;
    }

    worker = std::thread([this]() { this->run(); });
}

Perf::PerfAsyncLogger::~PerfAsyncLogger() { stop(); }

void Perf::PerfAsyncLogger::log(const PerfTableEntry& entry) {
    if (!worker.joinable() || !logger)
        return;

    {
        std::lock_guard lock(mutex);
        if (queue.size() >= Perf::getConfig().maxQueueSize)
            return;
        queue.push(entry);
    }
    cv.notify_one();
}

void Perf::PerfAsyncLogger::run() {
    if (!logger)
        return;

    while (true) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return !queue.empty() || done; });

        std::queue<PerfTableEntry> local_queue;
        local_queue.swap(queue);
        lock.unlock();

        while (!local_queue.empty()) {
            const auto& entry = local_queue.front();
            const auto duration_ns = entry.t1 - entry.t0;

            logger->info("{},{},{},{},{},{},{},{}", entry.t0, entry.t1,
                         duration_ns, entry.location.func, entry.location.file,
                         entry.location.line, entry.location.tag,
                         std::hash<std::thread::id>()(entry.threadId));
            local_queue.pop();
        }

        lock.lock();
        if (done && queue.empty())
            break;
    }

    if (logger) {
        logger->flush();
    }
}

void Perf::PerfAsyncLogger::stop() {
    {
        std::lock_guard lock(mutex);
        done = true;
    }
    cv.notify_all();
    if (worker.joinable()) {
        worker.join();
    }

    if (logger) {
        logger->flush();
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

        if (perthread.table.size() < getConfig().maxEventsPerThread) {
            perthread.table.push_back(entry);
        }

        if (getConfig().asyncLogging) {
            asyncLogger.log(entry);
        }

        {
            std::lock_guard guard(gathered.lock);
            gathered.table.push_back(entry);
        }
    }

    perthread.endNested(t1);
}

// --- Static Methods Implementation ---

void Perf::setConfig(const Config& config) {
    config_ = config;

    std::lock_guard guard(gathered.lock);
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
        std::lock_guard guard(lock);

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
        std::lock_guard guard(lock);
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

        message += fmt::format("- {} ns (Thread {}, {}:{})", duration,
                               std::hash<std::thread::id>()(entry.threadId),
                               entry.location.file, entry.location.line);

        Perf::logger->info(message);
    }

    Perf::logger->info("==========================================");
}

static void writeCsvData(std::shared_ptr<spdlog::logger> csv_logger,
                         const Perf::PerfGather& gatherer,
                         const Perf::Config& config) {
    csv_logger->info(
        "Function,File,Line,Start(ns),End(ns),Duration(ns),ThreadID,Tag");

    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    std::scoped_lock guard(gatherer.lock);
    for (const auto& entry : gatherer.table) {
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
    const Perf::PerfGather& gatherer, const Perf::Config& config) {
    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    std::scoped_lock guard(gatherer.lock);
    for (const auto& entry : gatherer.table) {
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
        std::lock_guard guard(gathered.lock);
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
                auto csv_logger =
                    spdlog::basic_logger_mt("perf_csv", csvFilename, true);
                csv_logger->set_pattern("%v");
                writeCsvData(csv_logger, gathered, config);
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
                auto folded_logger = spdlog::basic_logger_mt(
                    "perf_flamegraph", foldedFilename, true);
                folded_logger->set_pattern("%v");
                writeFlamegraphData(folded_logger, gathered, config);
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