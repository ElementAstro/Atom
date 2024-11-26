#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <source_location>
#include <string>
#include <thread>

struct Perf {
    struct Location {
        const char *func;
        const char *file;
        int line;
        const char *tag;

#if __cpp_lib_source_location
        explicit Location(
            std::source_location const &loc = std::source_location::current(),
            const char *tag = "");

        explicit Location(
            const char *func,
            std::source_location const &loc = std::source_location::current(),
            const char *tag = "");

#define PERF_TAG(tag) Perf({__func__, __FILE__, __LINE__, tag})
#define PERF Perf()
#else

#define PERF_TAG(tag) Perf({__func__, __FILE__, __LINE__, tag})
#define PERF Perf({__func__, __FILE__, __LINE__})
#endif
        Location(const char *func = "?", const char *file = "???", int line = 0,
                 const char *tag = "");

        bool operator<(const Location &rhs) const;
    };

private:
    Location location_;
    std::uint64_t t0_;

    struct PerfTableEntry {
        std::thread::id threadId;
        std::uint64_t t0;
        std::uint64_t t1;
        Location location;

        PerfTableEntry(std::uint64_t t0, std::uint64_t t1, Location location);
    };

    struct PerfFilter {
        std::uint64_t minDuration = 0;
        std::string funcContains;

        bool match(const PerfTableEntry &entry) const;
    };

    static void generateFilteredReport(const PerfFilter &filter);

    struct PerfThreadLocal {
        std::deque<PerfTableEntry> table;
        std::vector<std::uint64_t> stack;

        PerfThreadLocal() = default;

        void startNested(std::uint64_t t0);
        void endNested(std::uint64_t t1);
    };

    static inline thread_local PerfThreadLocal perthread;

    struct PerfAsyncLogger {
        std::queue<PerfTableEntry> queue;
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        std::thread worker;

        PerfAsyncLogger();
        ~PerfAsyncLogger();

        void log(const PerfTableEntry &entry);
        void run();
        void stop();
    };

    static inline PerfAsyncLogger asyncLogger;

    struct PerfGather {
        std::deque<PerfTableEntry> table;
        std::mutex lock;
        char const *output;

        PerfGather();

        void exportToJSON(const std::string &filename);
        void generateThreadReport();
    };

    static inline PerfGather gathered;

public:
    Perf(Location location = Location(std::source_location::current()));
    ~Perf();

    static void finalize();
};
