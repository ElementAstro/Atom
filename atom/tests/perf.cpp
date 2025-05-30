#include "perf.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <string_view>
#include <thread>
#include <vector>

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
    std::cout << "--- Filtered Performance Report ---\n"
              << "Filter: minDuration=" << filter.minDuration
              << "ns, funcContains='" << filter.funcContains << "'\n";

    bool found = false;
    std::lock_guard guard(gathered.lock);
    for (const auto& entry : gathered.table) {
        if (filter.match(entry)) {
            found = true;
            const auto duration_ns = entry.t1 - entry.t0;
            std::cout << entry.location.func << " (" << entry.location.file
                      << ":" << entry.location.line << ") Tag: ["
                      << entry.location.tag << "] Duration: " << duration_ns
                      << " ns\n";
        }
    }

    if (!found) {
        std::cout << "No entries matched the filter.\n";
    }
    std::cout << "--- End Filtered Report ---\n";
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
    worker = std::thread([this]() { this->run(); });
}

Perf::PerfAsyncLogger::~PerfAsyncLogger() { stop(); }

void Perf::PerfAsyncLogger::log(const PerfTableEntry& entry) {
    if (!worker.joinable())
        return;

    {
        std::lock_guard lock(mutex);
        if (queue.size() >= getConfig().maxQueueSize)
            return;
        queue.push(entry);
    }
    cv.notify_one();
}

void Perf::PerfAsyncLogger::run() {
    const std::string log_filename = "perf_async.log";
    std::ofstream fout(log_filename);
    if (!fout)
        return;

    fout << "StartTimestamp,EndTimestamp,Duration(ns),Function,File,Line,Tag,"
            "ThreadID\n";

    while (true) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return !queue.empty() || done; });

        std::queue<PerfTableEntry> local_queue;
        local_queue.swap(queue);
        lock.unlock();

        while (!local_queue.empty()) {
            const auto& entry = local_queue.front();
            const auto duration_ns = entry.t1 - entry.t0;

            fout << entry.t0 << ',' << entry.t1 << ',' << duration_ns << ','
                 << entry.location.func << ',' << entry.location.file << ','
                 << entry.location.line << ',' << entry.location.tag << ','
                 << std::hash<std::thread::id>()(entry.threadId) << '\n';
            local_queue.pop();
        }

        lock.lock();
        if (done && queue.empty())
            break;
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
}

const Perf::Config& Perf::getConfig() { return config_; }

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

void Perf::PerfGather::exportToJSON(const std::string& filename) {
    nlohmann::json j = nlohmann::json::array();
    const auto& config = Perf::getConfig();
    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    try {
        std::lock_guard guard(lock);

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
    } catch (const std::exception& e) {
        std::cerr << "Error processing performance data for JSON export: "
                  << e.what() << '\n';
        return;
    }

    try {
        const auto path = fs::path(filename);
        if (!path.parent_path().empty() && !fs::exists(path.parent_path())) {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            if (ec) {
                std::cerr << "Error creating directory "
                          << path.parent_path().string()
                          << " for JSON export: " << ec.message() << '\n';
                return;
            }
        }

        std::ofstream out(path);
        if (!out) {
            std::cerr << "Failed to open file for writing: " << filename
                      << '\n';
            return;
        }
        out << j.dump(4);
        std::cout << "Exported performance data to " << filename << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error writing JSON file " << filename << ": " << e.what()
                  << '\n';
    }
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
        std::cout << "No performance data recorded (or none above minimum "
                     "threshold of "
                  << min_duration_ns << " ns)" << std::endl;
        return;
    }

    std::cout << "==========================================\n"
              << "Performance Summary Report\n"
              << "==========================================\n"
              << "Configuration:\n"
              << "  Minimum Duration: " << min_duration_ns << " ns\n"
              << "  Async Logging: "
              << (config.asyncLogging ? "Enabled" : "Disabled") << "\n"
              << "------------------------------------------\n"
              << "Total threads with recorded entries: " << threadData.size()
              << "\n\n";

    for (const auto& [id, entries] : threadData) {
        std::cout << "--- Thread " << std::hash<std::thread::id>()(id)
                  << " ---\n"
                  << "  Total entries recorded: " << entries.size() << "\n";

        const uint64_t totalDuration =
            std::accumulate(entries.begin(), entries.end(), uint64_t(0),
                            [](uint64_t sum, const auto& entry_ref) {
                                const auto& entry = entry_ref.get();
                                return sum + (entry.t1 - entry.t0);
                            });

        std::cout << "  Total duration recorded: " << totalDuration << " ns\n";

        auto sortedEntries = entries;
        std::sort(sortedEntries.begin(), sortedEntries.end(),
                  [](const auto& a_ref, const auto& b_ref) {
                      const auto& a = a_ref.get();
                      const auto& b = b_ref.get();
                      return (a.t1 - a.t0) > (b.t1 - b.t0);
                  });

        const auto topCount = std::min(size_t(10), sortedEntries.size());
        std::cout << "  Top " << topCount << " entries by duration:\n";

        for (size_t i = 0; i < topCount; ++i) {
            const auto& entry = sortedEntries[i].get();
            const auto duration = entry.t1 - entry.t0;
            std::cout << "    " << entry.location.func;
            if (entry.location.tag && *entry.location.tag) {
                std::cout << " [" << entry.location.tag << "]";
            }
            std::cout << " - " << duration << " ns (" << entry.location.file
                      << ":" << entry.location.line << ")\n";
        }
        std::cout << "\n";
    }

    std::cout << "==========================================\n"
              << "Overall Top Functions (Across All Threads)\n"
              << "==========================================\n";

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
    std::cout << "Top " << topCount << " entries by duration:\n";

    for (size_t i = 0; i < topCount; ++i) {
        const auto& entry = allEntries[i].get();
        const auto duration = entry.t1 - entry.t0;
        std::cout << std::setw(2) << (i + 1) << ". " << entry.location.func;
        if (entry.location.tag && *entry.location.tag) {
            std::cout << " [" << entry.location.tag << "]";
        }
        std::cout << " - " << duration << " ns (Thread "
                  << std::hash<std::thread::id>()(entry.threadId) << ", "
                  << entry.location.file << ":" << entry.location.line << ")\n";
    }
    std::cout << "==========================================\n";
}

static void writeCsvData(std::ostream& csv, const Perf::PerfGather& gatherer,
                         const Perf::Config& config) {
    csv << "Function,File,Line,Start(ns),End(ns),Duration(ns),ThreadID,Tag\n";

    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    std::scoped_lock guard(gatherer.lock);
    for (const auto& entry : gatherer.table) {
        const auto duration = entry.t1 - entry.t0;
        if (duration < min_duration_ns)
            continue;

        csv << entry.location.func << "," << entry.location.file << ","
            << entry.location.line << "," << entry.t0 << "," << entry.t1 << ","
            << duration << "," << std::hash<std::thread::id>()(entry.threadId)
            << ",";

        if (entry.location.tag && *entry.location.tag) {
            csv << entry.location.tag;
        }
        csv << "\n";
    }
}

static void writeFlamegraphData(std::ostream& folded,
                                const Perf::PerfGather& gatherer,
                                const Perf::Config& config) {
    const auto min_duration_ns =
        static_cast<uint64_t>(config.minimumDuration.count());

    std::scoped_lock guard(gatherer.lock);
    for (const auto& entry : gatherer.table) {
        const auto duration = entry.t1 - entry.t0;
        if (duration < min_duration_ns)
            continue;

        folded << entry.location.func << ";" << entry.location.file << ":"
               << entry.location.line;

        if (entry.location.tag && *entry.location.tag) {
            folded << ";" << entry.location.tag;
        }

        folded << " " << duration << "\n";
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
                std::ofstream csv(csvFilename);
                if (csv) {
                    writeCsvData(csv, gathered, config);
                    std::cout << "Exported CSV data to " << csvFilename
                              << std::endl;
                } else {
                    std::cerr
                        << "Failed to open file for writing: " << csvFilename
                        << '\n';
                }
            } catch (const std::exception& e) {
                std::cerr << "Error exporting CSV to " << csvFilename << ": "
                          << e.what() << '\n';
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
                std::ofstream folded(foldedFilename);
                if (folded) {
                    writeFlamegraphData(folded, gathered, config);
                    std::cout << "Exported flamegraph data to "
                              << foldedFilename << std::endl;
                    std::cout << "Hint: Use 'flamegraph.pl " << foldedFilename
                              << " > " << svgFilename
                              << "' to generate visualization." << std::endl;
                } else {
                    std::cerr
                        << "Failed to open file for writing: " << foldedFilename
                        << '\n';
                }
            } catch (const std::exception& e) {
                std::cerr << "Error exporting flamegraph data to "
                          << foldedFilename << ": " << e.what() << '\n';
            }
        }
    }

    if (config.generateThreadReport) {
        gathered.generateThreadReport();
    }
}