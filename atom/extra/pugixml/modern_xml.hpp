#pragma once

// Main include header for the modern XML library with advanced concurrency support
#include "xml_builder.hpp"
#include "xml_document.hpp"
#include "xml_node_wrapper.hpp"
#include "xml_query.hpp"
#include "concurrent/thread_safe_xml.hpp"
#include "concurrent/lock_free_pool.hpp"
#include "concurrent/parallel_processor.hpp"
#include "performance/metrics_collector.hpp"

// Atom framework includes for preferred data types
#include "atom/containers/high_performance.hpp"
#include "atom/error/exception.hpp"
#include "atom/memory/memory.hpp"

#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace atom::extra::pugixml {

/**
 * @brief Data Types Usage Guidelines for Atom Framework Integration
 *
 * To maintain consistency with the Atom framework, use these preferred types:
 *
 * CONTAINERS (from atom/containers/high_performance.hpp):
 * - atom::containers::String instead of std::string
 * - atom::containers::Vector<T> instead of std::vector<T>
 * - atom::containers::HashMap<K,V> instead of std::unordered_map<K,V>
 * - atom::containers::HashSet<T> instead of std::unordered_set<T>
 * - atom::containers::Map<K,V> instead of std::map<K,V>
 * - atom::containers::SmallVector<T,N> for small fixed-size vectors
 *
 * EXCEPTIONS (from atom/error/exception.hpp):
 * - Use THROW_RUNTIME_ERROR(...) macro instead of throw std::runtime_error
 * - Use THROW_LOGIC_ERROR(...) for logic errors
 * - Use THROW_INVALID_ARGUMENT(...) for invalid arguments
 * - Use THROW_FILE_NOT_FOUND(...) for file operations
 * - Use THROW_PARSE_ERROR(...) for parsing errors (if available)
 *
 * MEMORY MANAGEMENT:
 * - Use atom::memory smart pointers when available
 * - Prefer RAII and move semantics
 *
 * STRING HANDLING:
 * - Use std::string_view for read-only string parameters
 * - Use atom::containers::String for owned strings
 * - Use StringLike concept for template parameters accepting string types
 *
 * OPTIONAL VALUES:
 * - Continue using std::optional<T> as it's standard and well-integrated
 *
 * SMART POINTERS:
 * - Continue using std::unique_ptr and std::shared_ptr unless atom provides alternatives
 */

// Version information
namespace version {
constexpr int major = 2;
constexpr int minor = 0;
constexpr int patch = 0;
constexpr std::string_view string = "2.0.0-concurrent";
}  // namespace version

// Concurrency configuration
namespace config {
inline const size_t default_thread_pool_size = std::thread::hardware_concurrency();
constexpr size_t default_node_pool_size = 1024 * 1024;  // 1M nodes
constexpr size_t default_cache_size = 512 * 1024;       // 512K cache entries
inline const std::chrono::milliseconds default_timeout{5000};
}  // namespace config

// Global performance metrics
inline std::atomic<uint64_t> g_operations_count{0};
inline std::atomic<uint64_t> g_cache_hits{0};
inline std::atomic<uint64_t> g_cache_misses{0};

// Convenience aliases
using XmlDocument = Document;
using XmlNode = Node;
using XmlAttribute = Attribute;
using XmlBuilder = NodeBuilder;
using XmlDocumentBuilder = DocumentBuilder;

// Concurrent aliases
using ConcurrentDocument = concurrent::ThreadSafeDocument;
using ConcurrentNode = concurrent::ThreadSafeNode;
using ParallelProcessor = concurrent::ParallelXmlProcessor;
using MetricsCollector = performance::MetricsCollector;

}  // namespace atom::extra::pugixml
