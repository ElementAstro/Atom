#include "perf.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

#if __cplusplus >= 202002L
#if defined(__has_include)
#if __has_include(<source_location>)
#include <source_location>
#endif
#endif
#endif

#include "atom/type/json.hpp"

Perf::Location::Location(std::source_location const &loc, const char *tag)
    : Location(loc.function_name(), loc.file_name(), loc.line(), tag) {}

Perf::Location::Location(const char *func, std::source_location const &loc,
                         const char *tag)
    : Location(func, loc.file_name(), loc.line(), tag) {}

Perf::Location::Location(const char *func, const char *file, int line,
                         const char *tag)
    : func(func), file(file), line(line), tag(tag) {}

bool Perf::Location::operator<(const Location &rhs) const {
    return std::tie(func, file, line) < std::tie(rhs.func, rhs.file, rhs.line);
}

Perf::PerfTableEntry::PerfTableEntry(std::uint64_t t0, std::uint64_t t1,
                                     Location location)
    : threadId(std::this_thread::get_id()),
      t0(t0),
      t1(t1),
      location(location) {}

bool Perf::PerfFilter::match(const PerfTableEntry &entry) const {
    auto duration = entry.t1 - entry.t0;
    return duration >= minDuration &&
           (funcContains.empty() ||
            std::string(entry.location.func).find(funcContains) !=
                std::string::npos);
}

void Perf::generateFilteredReport(const PerfFilter &filter) {
    for (const auto &entry : gathered.table) {
        if (filter.match(entry)) {
            std::printf("%s %s:%d %llu\n", entry.location.func,
                        entry.location.file, entry.location.line,
                        static_cast<unsigned long long>(entry.t1 - entry.t0));
        }
    }
}

void Perf::PerfThreadLocal::startNested(std::uint64_t t0) {
    stack.push_back(t0);
}

void Perf::PerfThreadLocal::endNested(std::uint64_t t1) {
    if (!stack.empty()) {
        stack.pop_back();
    }
}

Perf::PerfAsyncLogger::PerfAsyncLogger() {
    worker = std::thread([this] { this->run(); });
}

Perf::PerfAsyncLogger::~PerfAsyncLogger() { stop(); }

void Perf::PerfAsyncLogger::log(const PerfTableEntry &entry) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(entry);
    }
    cv.notify_one();
}

void Perf::PerfAsyncLogger::run() {
    std::ofstream fout("perf_async.log");
    while (true) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return !queue.empty() || done; });
        while (!queue.empty()) {
            auto entry = queue.front();
            queue.pop();
            fout << entry.t0 << ',' << entry.t1 << ',' << entry.location.func
                 << ',' << entry.location.file << ',' << entry.location.line
                 << ',' << entry.location.tag << '\n';
        }
        if (done) {
            break;
        }
    }
}

void Perf::PerfAsyncLogger::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        done = true;
    }
    cv.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
}

Perf::PerfGather::PerfGather() { output = std::getenv("PERF_OUTPUT"); }

void Perf::PerfGather::exportToJSON(const std::string &filename) {
    std::lock_guard<std::mutex> guard(lock);
    nlohmann::json j;
    for (const auto &entry : table) {
        j.push_back(
            {{"func", entry.location.func},
             {"file", entry.location.file},
             {"line", entry.location.line},
             {"start", entry.t0},
             {"end", entry.t1},
             {"duration", entry.t1 - entry.t0},
             {"thread_id", std::hash<std::thread::id>()(entry.threadId)},
             {"tag", entry.location.tag}});
    }
    std::ofstream fout(filename);
    fout << j.dump(4);
}

void Perf::PerfGather::generateThreadReport() {
    std::map<std::thread::id, std::vector<PerfTableEntry>> threadData;
    {
        std::lock_guard<std::mutex> guard(lock);
        for (const auto &entry : table) {
            threadData[entry.threadId].push_back(entry);
        }
    }
    for (const auto &[id, entries] : threadData) {
        std::printf("Thread %zu:\n", std::hash<std::thread::id>()(id));
        for (const auto &entry : entries) {
            std::printf("  %s %s:%d %llu ns\n", entry.location.func,
                        entry.location.file, entry.location.line,
                        static_cast<unsigned long long>(entry.t1 - entry.t0));
        }
    }
}

Perf::Perf(Location location)
    : location_(location),
      t0_(std::chrono::high_resolution_clock::now()
              .time_since_epoch()
              .count()) {
    perthread.startNested(t0_);
}

Perf::~Perf() {
    auto t1 =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    perthread.endNested(t1);
    PerfTableEntry entry(t0_, t1, location_);
    perthread.table.push_back(entry);
    asyncLogger.log(entry);
    {
        std::lock_guard<std::mutex> guard(gathered.lock);
        gathered.table.push_back(entry);
    }
}

void Perf::finalize() {
    asyncLogger.stop();
    if (gathered.output != nullptr) {
        gathered.exportToJSON(gathered.output);
    }
    gathered.generateThreadReport();
}

void longRunningTask() {
    PERF_TAG("Long Running Task");
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::printf("Step %d completed\n", i + 1);
    }
}

int main() {
    std::thread monitor([] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::printf("Monitoring...\n");
        }
    });

    longRunningTask();
    monitor.detach();
    Perf::finalize();
    return 0;
}