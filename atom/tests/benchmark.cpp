#include "benchmark.hpp"

#include <cstring>  // Needed for memset
#include <fstream>  // Needed for getMemoryUsage on Linux
#include <filesystem>
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
