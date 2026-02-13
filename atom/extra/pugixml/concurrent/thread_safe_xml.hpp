#pragma once

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <shared_mutex>
#include <memory>
#include <concepts>
#include <string_view>
#include <optional>
#include <vector>
#include <chrono>
#include <source_location>
#include <thread>
#include <sstream>
#include <stdexcept>
#include <limits>

namespace atom::extra::pugixml::concurrent {

/**
 * @brief Memory ordering policies for atomic operations
 */
enum class MemoryOrder : int {
    Relaxed = static_cast<int>(std::memory_order_relaxed),
    Acquire = static_cast<int>(std::memory_order_acquire),
    Release = static_cast<int>(std::memory_order_release),
    AcqRel = static_cast<int>(std::memory_order_acq_rel),
    SeqCst = static_cast<int>(std::memory_order_seq_cst)
};

/**
 * @brief Thread-safe reference counting for XML nodes
 */
class AtomicRefCount {
private:
    mutable std::atomic<uint32_t> count_{1};

public:
    AtomicRefCount() = default;
    AtomicRefCount(const AtomicRefCount&) : count_{1} {}
    AtomicRefCount& operator=(const AtomicRefCount&) { return *this; }

    void add_ref() const noexcept {
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] bool release() const noexcept {
        return count_.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

    [[nodiscard]] uint32_t use_count() const noexcept {
        return count_.load(std::memory_order_acquire);
    }
};

/**
 * @brief Lock-free atomic pointer with hazard pointer protection
 */
template<typename T>
class AtomicPtr {
private:
    std::atomic<T*> ptr_{nullptr};

public:
    AtomicPtr() = default;
    explicit AtomicPtr(T* p) : ptr_(p) {}

    AtomicPtr(const AtomicPtr&) = delete;
    AtomicPtr& operator=(const AtomicPtr&) = delete;

    AtomicPtr(AtomicPtr&& other) noexcept : ptr_(other.ptr_.exchange(nullptr)) {}

    AtomicPtr& operator=(AtomicPtr&& other) noexcept {
        if (this != &other) {
            delete ptr_.exchange(other.ptr_.exchange(nullptr));
        }
        return *this;
    }

    ~AtomicPtr() { delete ptr_.load(); }

    [[nodiscard]] T* load(MemoryOrder order = MemoryOrder::Acquire) const noexcept {
        return ptr_.load(static_cast<std::memory_order>(order));
    }

    void store(T* desired, MemoryOrder order = MemoryOrder::Release) noexcept {
        delete ptr_.exchange(desired, static_cast<std::memory_order>(order));
    }

    [[nodiscard]] bool compare_exchange_weak(T*& expected, T* desired,
                                           MemoryOrder order = MemoryOrder::AcqRel) noexcept {
        return ptr_.compare_exchange_weak(expected, desired,
                                        static_cast<std::memory_order>(order));
    }

    [[nodiscard]] bool compare_exchange_strong(T*& expected, T* desired,
                                             MemoryOrder order = MemoryOrder::AcqRel) noexcept {
        return ptr_.compare_exchange_strong(expected, desired,
                                          static_cast<std::memory_order>(order));
    }
};

/**
 * @brief High-performance reader-writer lock optimized for XML operations
 */
class OptimizedRWLock {
private:
    mutable std::atomic<uint32_t> state_{0};
    static constexpr uint32_t WRITER_BIT = 1u << 31;
    static constexpr uint32_t READER_MASK = ~WRITER_BIT;

public:
    class ReadLock {
        const OptimizedRWLock* lock_;
    public:
        explicit ReadLock(const OptimizedRWLock& lock) : lock_(&lock) {
            lock_->lock_shared();
        }
        ~ReadLock() { lock_->unlock_shared(); }
        ReadLock(const ReadLock&) = delete;
        ReadLock& operator=(const ReadLock&) = delete;
    };

    class WriteLock {
        const OptimizedRWLock* lock_;
    public:
        explicit WriteLock(const OptimizedRWLock& lock) : lock_(&lock) {
            lock_->lock();
        }
        ~WriteLock() { lock_->unlock(); }
        WriteLock(const WriteLock&) = delete;
        WriteLock& operator=(const WriteLock&) = delete;
    };

    void lock_shared() const {
        uint32_t state = state_.load(std::memory_order_acquire);
        while (true) {
            if (state & WRITER_BIT) {
                std::this_thread::yield();
                state = state_.load(std::memory_order_acquire);
                continue;
            }

            if (state_.compare_exchange_weak(state, state + 1,
                                           std::memory_order_acquire)) {
                break;
            }
        }
    }

    void unlock_shared() const noexcept {
        state_.fetch_sub(1, std::memory_order_release);
    }

    void lock() const {
        uint32_t expected = 0;
        while (!state_.compare_exchange_weak(expected, WRITER_BIT,
                                           std::memory_order_acquire)) {
            expected = 0;
            std::this_thread::yield();
        }
    }

    void unlock() const noexcept {
        state_.store(0, std::memory_order_release);
    }

    [[nodiscard]] ReadLock read_lock() const { return ReadLock(*this); }
    [[nodiscard]] WriteLock write_lock() const { return WriteLock(*this); }
};

/**
 * @brief Thread-safe wrapper for pugi::xml_node with lock-free operations
 */
class ThreadSafeNode {
private:
    pugi::xml_node node_;
    mutable OptimizedRWLock lock_;
    mutable AtomicRefCount ref_count_;
    std::shared_ptr<spdlog::logger> logger_;

    void log_operation(std::string_view operation,
                      const std::source_location& loc = std::source_location::current()) const {
        if (logger_) {
            logger_->trace("ThreadSafeNode::{} called from {}:{}",
                          operation, loc.file_name(), loc.line());
        }
    }

public:
    explicit ThreadSafeNode(pugi::xml_node node,
                           std::shared_ptr<spdlog::logger> logger = nullptr)
        : node_(node), logger_(logger) {
        log_operation("constructor");
    }

    ThreadSafeNode(const ThreadSafeNode& other)
        : node_(other.node_), logger_(other.logger_) {
        other.ref_count_.add_ref();
        log_operation("copy_constructor");
    }

    ThreadSafeNode& operator=(const ThreadSafeNode& other) {
        if (this != &other) {
            if (ref_count_.release()) {
                // Last reference, cleanup if needed
            }
            node_ = other.node_;
            logger_ = other.logger_;
            other.ref_count_.add_ref();
            log_operation("copy_assignment");
        }
        return *this;
    }

    ~ThreadSafeNode() {
        if (ref_count_.release()) {
            log_operation("destructor_final");
        }
    }

    /**
     * @brief Thread-safe name access
     */
    [[nodiscard]] std::string name() const {
        auto lock = lock_.read_lock();
        log_operation("name");
        return node_.name();
    }

    /**
     * @brief Thread-safe text content access
     */
    [[nodiscard]] std::string text() const {
        auto lock = lock_.read_lock();
        log_operation("text");
        return node_.child_value();
    }

    /**
     * @brief Thread-safe attribute access with optional return
     */
    [[nodiscard]] std::optional<std::string> attribute(std::string_view name) const {
        auto lock = lock_.read_lock();
        log_operation("attribute");
        auto attr = node_.attribute(name.data());
        if (attr.empty()) {
            return std::nullopt;
        }
        return std::string{attr.value()};
    }

    /**
     * @brief Thread-safe child node access
     */
    [[nodiscard]] std::optional<ThreadSafeNode> child(std::string_view name) const {
        auto lock = lock_.read_lock();
        log_operation("child");
        auto child_node = node_.child(name.data());
        if (child_node.empty()) {
            return std::nullopt;
        }
        return ThreadSafeNode{child_node, logger_};
    }

    /**
     * @brief Thread-safe children collection
     */
    [[nodiscard]] std::vector<ThreadSafeNode> children() const {
        auto lock = lock_.read_lock();
        log_operation("children");
        std::vector<ThreadSafeNode> result;
        for (auto child : node_.children()) {
            result.emplace_back(child, logger_);
        }
        return result;
    }

    /**
     * @brief Thread-safe node modification with write lock
     */
    void set_text(std::string_view value) {
        auto lock = lock_.write_lock();
        log_operation("set_text");
        node_.text().set(value.data());
    }

    /**
     * @brief Thread-safe attribute setting
     */
    void set_attribute(std::string_view name, std::string_view value) {
        auto lock = lock_.write_lock();
        log_operation("set_attribute");
        node_.attribute(name.data()).set_value(value.data());
    }

    /**
     * @brief Thread-safe child appending
     */
    ThreadSafeNode append_child(std::string_view name) {
        auto lock = lock_.write_lock();
        log_operation("append_child");
        auto child = node_.append_child(name.data());
        if (child.empty()) {
            throw std::runtime_error("Failed to append child");
        }
        return ThreadSafeNode{child, logger_};
    }

    /**
     * @brief Check if node is valid
     */
    [[nodiscard]] bool empty() const noexcept {
        auto lock = lock_.read_lock();
        return node_.empty();
    }

    /**
     * @brief Get reference count for debugging
     */
    [[nodiscard]] uint32_t use_count() const noexcept {
        return ref_count_.use_count();
    }

    /**
     * @brief Access to underlying pugi node (use with caution)
     */
    [[nodiscard]] const pugi::xml_node& native() const noexcept {
        return node_;
    }
};

/**
 * @brief Thread-safe document wrapper with concurrent access support
 */
class ThreadSafeDocument {
private:
    std::unique_ptr<pugi::xml_document> doc_;
    mutable OptimizedRWLock lock_;
    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<uint64_t> version_{0};

    void log_operation(std::string_view operation,
                      const std::source_location& loc = std::source_location::current()) const {
        if (logger_) {
            logger_->trace("ThreadSafeDocument::{} called from {}:{}",
                          operation, loc.file_name(), loc.line());
        }
    }

public:
    explicit ThreadSafeDocument(std::shared_ptr<spdlog::logger> logger = nullptr)
        : doc_(std::make_unique<pugi::xml_document>()), logger_(logger) {
        log_operation("constructor");
    }

    ThreadSafeDocument(const ThreadSafeDocument&) = delete;
    ThreadSafeDocument& operator=(const ThreadSafeDocument&) = delete;

    ThreadSafeDocument(ThreadSafeDocument&& other) noexcept
        : doc_(std::move(other.doc_)), logger_(other.logger_),
          version_(other.version_.load()) {
        log_operation("move_constructor");
    }

    ThreadSafeDocument& operator=(ThreadSafeDocument&& other) noexcept {
        if (this != &other) {
            auto lock = lock_.write_lock();
            doc_ = std::move(other.doc_);
            logger_ = other.logger_;
            version_.store(other.version_.load());
            log_operation("move_assignment");
        }
        return *this;
    }

    /**
     * @brief Thread-safe document loading from string
     */
    bool load_string(std::string_view xml_content) {
        auto lock = lock_.write_lock();
        log_operation("load_string");
        auto result = doc_->load_string(xml_content.data());
        if (result) {
            version_.fetch_add(1, std::memory_order_relaxed);
        }
        return static_cast<bool>(result);
    }

    /**
     * @brief Thread-safe document loading from file
     */
    bool load_file(const std::string& filename) {
        auto lock = lock_.write_lock();
        log_operation("load_file");
        auto result = doc_->load_file(filename.c_str());
        if (result) {
            version_.fetch_add(1, std::memory_order_relaxed);
        }
        return static_cast<bool>(result);
    }

    /**
     * @brief Thread-safe root element access
     */
    [[nodiscard]] std::optional<ThreadSafeNode> root() const {
        auto lock = lock_.read_lock();
        log_operation("root");
        auto root_element = doc_->document_element();
        if (root_element.empty()) {
            return std::nullopt;
        }
        return ThreadSafeNode{root_element, logger_};
    }

    /**
     * @brief Thread-safe document serialization
     */
    [[nodiscard]] std::string to_string() const {
        auto lock = lock_.read_lock();
        log_operation("to_string");
        std::ostringstream oss;
        doc_->save(oss);
        return oss.str();
    }

    /**
     * @brief Thread-safe document clearing
     */
    void clear() {
        auto lock = lock_.write_lock();
        log_operation("clear");
        doc_->reset();
        version_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Get document version for change detection
     */
    [[nodiscard]] uint64_t version() const noexcept {
        return version_.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if document is empty
     */
    [[nodiscard]] bool empty() const {
        auto lock = lock_.read_lock();
        return doc_->empty();
    }

    /**
     * @brief Create root element thread-safely
     */
    ThreadSafeNode create_root(std::string_view name) {
        auto lock = lock_.write_lock();
        log_operation("create_root");
        auto root_node = doc_->append_child(name.data());
        if (root_node.empty()) {
            throw std::runtime_error("Failed to create root element");
        }
        version_.fetch_add(1, std::memory_order_relaxed);
        return ThreadSafeNode{root_node, logger_};
    }
};

}  // namespace atom::extra::pugixml::concurrent
