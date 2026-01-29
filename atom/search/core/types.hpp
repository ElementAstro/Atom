#ifndef ATOM_SEARCH_CORE_TYPES_HPP
#define ATOM_SEARCH_CORE_TYPES_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "atom/containers/high_performance.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/container/string.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#endif

namespace atom::search {

using atom::containers::HashMap;
using atom::containers::HashSet;
using atom::containers::String;
using atom::containers::Vector;

/**
 * @brief Error codes for search operations.
 */
enum class SearchErrorCode {
    Success = 0,
    DocumentNotFound,
    DocumentValidationFailed,
    DuplicateDocument,
    InvalidQuery,
    IndexCorrupted,
    IOError,
    Timeout,
    InternalError
};

/**
 * @brief Result type for search operations using std::expected.
 */
template <typename T>
using SearchResult = std::expected<T, SearchErrorCode>;

/**
 * @brief Convert error code to string description.
 */
[[nodiscard]] constexpr std::string_view errorCodeToString(
    SearchErrorCode code) noexcept {
    switch (code) {
        case SearchErrorCode::Success:
            return "Success";
        case SearchErrorCode::DocumentNotFound:
            return "Document not found";
        case SearchErrorCode::DocumentValidationFailed:
            return "Document validation failed";
        case SearchErrorCode::DuplicateDocument:
            return "Duplicate document";
        case SearchErrorCode::InvalidQuery:
            return "Invalid query";
        case SearchErrorCode::IndexCorrupted:
            return "Index corrupted";
        case SearchErrorCode::IOError:
            return "I/O error";
        case SearchErrorCode::Timeout:
            return "Operation timeout";
        case SearchErrorCode::InternalError:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

namespace concepts {

/**
 * @brief Concept for searchable types.
 */
template <typename T>
concept Searchable = requires(T t) {
    { t.getId() } -> std::convertible_to<std::string_view>;
    { t.getContent() } -> std::convertible_to<std::string_view>;
};

/**
 * @brief Concept for indexable containers.
 */
template <typename T>
concept Indexable = requires(T t) {
    typename T::value_type;
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
};

/**
 * @brief Concept for scoring functions.
 */
template <typename F, typename T>
concept ScoreFunction = requires(F f, const T& doc, std::string_view term) {
    { f(doc, term) } -> std::convertible_to<double>;
};

}  // namespace concepts

#ifdef ATOM_USE_BOOST
namespace threading {
using thread = boost::thread;
using mutex = boost::mutex;
using shared_mutex = boost::shared_mutex;
using unique_lock = boost::unique_lock<mutex>;
using shared_lock = boost::shared_lock<shared_mutex>;

template <typename T>
using future = boost::future<T>;
template <typename T>
using shared_future = boost::shared_future<T>;
template <typename T>
using promise = boost::promise<T>;

#ifdef ATOM_HAS_BOOST_LOCKFREE
using atom::containers::hp::lockfree::queue;
#else
template <typename T, size_t Capacity = 1024>
using queue = boost::lockfree::queue<T, boost::lockfree::capacity<Capacity> >;
#endif
template <typename T>
using lockfree_queue = queue<T>;

}  // namespace threading
#else
namespace threading {
using thread = std::thread;
using mutex = std::mutex;
using shared_mutex = std::shared_mutex;
using unique_lock = std::unique_lock<mutex>;
using shared_lock = std::shared_lock<shared_mutex>;

template <typename T>
using future = std::future<T>;
template <typename T>
using shared_future = std::shared_future<T>;
template <typename T>
using promise = std::promise<T>;

/**
 * @brief Thread-safe queue implementation (fallback when Boost lockfree
 * unavailable).
 */
template <typename T>
class lockfree_queue {
private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    size_t capacity_;

public:
    explicit lockfree_queue(size_t capacity = 1024) : capacity_(capacity) {}

    [[nodiscard]] bool push(const T& item) {
        std::lock_guard lock(mutex_);
        if (queue_.size() >= capacity_)
            return false;
        queue_.push(item);
        return true;
    }

    [[nodiscard]] bool push(T&& item) {
        std::lock_guard lock(mutex_);
        if (queue_.size() >= capacity_)
            return false;
        queue_.push(std::move(item));
        return true;
    }

    [[nodiscard]] bool pop(T& item) {
        std::lock_guard lock(mutex_);
        if (queue_.empty())
            return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool consume(T& item) { return pop(item); }

    void clear() {
        std::lock_guard lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }
};
}  // namespace threading
#endif

/**
 * @brief Configuration for document validation.
 */
struct DocumentConfig {
    size_t maxIdLength = 256;
    size_t maxContentLength = 1024 * 1024;  // 1MB
    size_t maxTagLength = 100;
    size_t maxTagCount = 100;
    bool allowEmptyContent = false;
};

/**
 * @brief Search engine configuration options.
 */
struct SearchEngineConfig {
    unsigned maxThreads = 0;  // 0 = hardware concurrency
    size_t maxDocuments = 1'000'000;
    size_t indexBatchSize = 1000;
    bool enableParallelSearch = true;
    bool enableFuzzySearch = true;
    bool enableAutoComplete = true;
    double minScoreThreshold = 0.0;
    size_t maxSearchResults = 1000;
    std::chrono::milliseconds searchTimeout{5000};
};

/**
 * @brief Search options for customizing search behavior.
 */
struct SearchOptions {
    size_t maxResults = 100;
    double minScore = 0.0;
    bool sortByRelevance = true;
    bool includeExpired = false;
    std::optional<std::chrono::milliseconds> timeout;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_CORE_TYPES_HPP
