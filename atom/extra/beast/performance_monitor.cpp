#include "performance_monitor.hpp"
#include <spdlog/spdlog.h>

namespace atom::beast::monitoring {

/**
 * @brief Global performance monitor instance
 */
PerformanceMonitor& get_global_performance_monitor() {
    static PerformanceMonitor instance;
    return instance;
}

} // namespace atom::beast::monitoring
