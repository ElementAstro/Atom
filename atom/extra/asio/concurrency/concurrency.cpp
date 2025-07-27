#include "concurrency.hpp"

namespace atom::extra::asio::concurrency {

// Static member definitions for concurrency_manager
std::unique_ptr<concurrency_manager> concurrency_manager::instance_;
std::once_flag concurrency_manager::init_flag_;

// Static member definitions for memory_manager  
std::unique_ptr<memory_manager> memory_manager::instance_;
std::once_flag memory_manager::init_flag_;

// Static member definitions for performance_monitor
std::unique_ptr<performance_monitor> performance_monitor::instance_;
std::once_flag performance_monitor::init_flag_;

} // namespace atom::extra::asio::concurrency
