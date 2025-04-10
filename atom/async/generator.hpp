/*
 * generator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: C++20 coroutine-based generator implementation

**************************************************/

#ifndef ATOM_ASYNC_GENERATOR_HPP
#define ATOM_ASYNC_GENERATOR_HPP

#include <concepts>
#include <coroutine>
#include <exception>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>

#ifdef ATOM_USE_BOOST_LOCKS
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/shared_lock_guard.hpp>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/atomic.hpp>
#endif

namespace atom::async {

/**
 * @brief A generator class using C++20 coroutines
 *
 * This generator provides a convenient way to create and use coroutines that
 * yield values of type T, similar to Python generators.
 *
 * @tparam T The type of values yielded by the generator
 */
template <typename T>
class Generator {
public:
    struct promise_type;  // Forward declaration

    /**
     * @brief Iterator class for the generator
     */
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::remove_reference_t<T>;
        using pointer = value_type*;
        using reference = value_type&;

        explicit iterator(std::coroutine_handle<promise_type> handle = nullptr)
            : handle_(handle) {}

        iterator& operator++() {
            if (!handle_ || handle_.done()) {
                handle_ = nullptr;
                return *this;
            }
            handle_.resume();
            if (handle_.done()) {
                handle_ = nullptr;
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return handle_ == other.handle_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        const T& operator*() const { return handle_.promise().value(); }

        const T* operator->() const { return &handle_.promise().value(); }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

    /**
     * @brief Promise type for the generator coroutine
     */
    struct promise_type {
        T value_;
        std::exception_ptr exception_;

        Generator get_return_object() {
            return Generator{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        template <std::convertible_to<T> From>
        std::suspend_always yield_value(From&& from) {
            value_ = std::forward<From>(from);
            return {};
        }

        void unhandled_exception() { exception_ = std::current_exception(); }

        void return_void() {}

        const T& value() const {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return value_;
        }
    };

    /**
     * @brief Constructs a generator from a coroutine handle
     */
    explicit Generator(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    /**
     * @brief Destructor that cleans up the coroutine
     */
    ~Generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Rule of five - prevent copy, allow move
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Returns an iterator pointing to the beginning of the generator
     */
    iterator begin() {
        if (handle_) {
            handle_.resume();
            if (handle_.done()) {
                return end();
            }
        }
        return iterator{handle_};
    }

    /**
     * @brief Returns an iterator pointing to the end of the generator
     */
    iterator end() { return iterator{nullptr}; }

private:
    std::coroutine_handle<promise_type> handle_;
};

/**
 * @brief A generator that can also receive values from the caller
 *
 * @tparam Yield Type yielded by the coroutine
 * @tparam Receive Type received from the caller
 */
template <typename Yield, typename Receive = void>
class TwoWayGenerator {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        Yield value_to_yield_;
        std::optional<Receive> value_to_receive_;
        std::exception_ptr exception_;

        TwoWayGenerator get_return_object() {
            return TwoWayGenerator{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        template <std::convertible_to<Yield> From>
        auto yield_value(From&& from) {
            value_to_yield_ = std::forward<From>(from);
            struct awaiter {
                promise_type& promise;

                bool await_ready() noexcept { return false; }

                void await_suspend(handle_type) noexcept {}

                Receive await_resume() {
                    if (!promise.value_to_receive_.has_value()) {
                        throw std::logic_error("No value received");
                    }
                    auto result = std::move(promise.value_to_receive_.value());
                    promise.value_to_receive_.reset();
                    return result;
                }
            };
            return awaiter{*this};
        }

        void unhandled_exception() { exception_ = std::current_exception(); }

        void return_void() {}
    };

    explicit TwoWayGenerator(handle_type handle) : handle_(handle) {}

    ~TwoWayGenerator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Rule of five - prevent copy, allow move
    TwoWayGenerator(const TwoWayGenerator&) = delete;
    TwoWayGenerator& operator=(const TwoWayGenerator&) = delete;

    TwoWayGenerator(TwoWayGenerator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    TwoWayGenerator& operator=(TwoWayGenerator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Advances the generator and returns the next value
     *
     * @param value Value to send to the generator
     * @return The yielded value
     * @throws std::logic_error if the generator is done
     */
    Yield next(Receive value = Receive{}) {
        if (!handle_ || handle_.done()) {
            throw std::logic_error("Generator is done");
        }

        handle_.promise().value_to_receive_ = std::move(value);
        handle_.resume();

        if (handle_.done()) {
            if (handle_.promise().exception_) {
                std::rethrow_exception(handle_.promise().exception_);
            }
            throw std::logic_error("Generator is done");
        }

        return std::move(handle_.promise().value_to_yield_);
    }

    /**
     * @brief Checks if the generator is done
     */
    bool done() const { return !handle_ || handle_.done(); }

private:
    handle_type handle_;
};

// Specialization for generators that don't receive values
template <typename Yield>
class TwoWayGenerator<Yield, void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        Yield value_to_yield_;
        std::exception_ptr exception_;

        TwoWayGenerator get_return_object() {
            return TwoWayGenerator{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        template <std::convertible_to<Yield> From>
        std::suspend_always yield_value(From&& from) {
            value_to_yield_ = std::forward<From>(from);
            return {};
        }

        void unhandled_exception() { exception_ = std::current_exception(); }

        void return_void() {}
    };

    explicit TwoWayGenerator(handle_type handle) : handle_(handle) {}

    ~TwoWayGenerator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Rule of five - prevent copy, allow move
    TwoWayGenerator(const TwoWayGenerator&) = delete;
    TwoWayGenerator& operator=(const TwoWayGenerator&) = delete;

    TwoWayGenerator(TwoWayGenerator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    TwoWayGenerator& operator=(TwoWayGenerator&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Advances the generator and returns the next value
     *
     * @return The yielded value
     * @throws std::logic_error if the generator is done
     */
    Yield next() {
        if (!handle_ || handle_.done()) {
            throw std::logic_error("Generator is done");
        }

        handle_.resume();

        if (handle_.done()) {
            if (handle_.promise().exception_) {
                std::rethrow_exception(handle_.promise().exception_);
            }
            throw std::logic_error("Generator is done");
        }

        return std::move(handle_.promise().value_to_yield_);
    }

    /**
     * @brief Checks if the generator is done
     */
    bool done() const { return !handle_ || handle_.done(); }

private:
    handle_type handle_;
};

/**
 * @brief Creates a generator that yields each element in a range
 *
 * @tparam Range The type of the range
 * @param range The range to yield elements from
 * @return A generator that yields elements from the range
 */
template <std::ranges::range Range>
Generator<std::ranges::range_value_t<Range>> from_range(Range&& range) {
    for (auto&& element : range) {
        co_yield element;
    }
}

/**
 * @brief Creates a generator that yields elements from begin to end
 *
 * @tparam T The type of the elements
 * @param begin The first element
 * @param end One past the last element
 * @param step The step between elements
 * @return A generator that yields elements from begin to end
 */
template <std::regular T>
Generator<T> range(T begin, T end, T step = T{1}) {
    for (T i = begin; i < end; i += step) {
        co_yield i;
    }
}

/**
 * @brief Creates a generator that yields elements infinitely
 *
 * @tparam T The type of the elements
 * @param start The starting element
 * @param step The step between elements
 * @return A generator that yields elements infinitely
 */
template <std::regular T>
Generator<T> infinite_range(T start = T{}, T step = T{1}) {
    T value = start;
    while (true) {
        co_yield value;
        value += step;
    }
}

#ifdef ATOM_USE_BOOST_LOCKS
/**
 * @brief A thread-safe generator class using C++20 coroutines and Boost.thread
 * 
 * This variant provides thread-safety for generators that might be accessed
 * from multiple threads. It uses Boost.thread synchronization primitives.
 *
 * @tparam T The type of values yielded by the generator
 */
template <typename T>
class ThreadSafeGenerator {
public:
    struct promise_type;  // Forward declaration

    /**
     * @brief Thread-safe iterator class for the generator
     */
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::remove_reference_t<T>;
        using pointer = value_type*;
        using reference = value_type&;

        explicit iterator(std::coroutine_handle<promise_type> handle = nullptr)
            : handle_(handle) {}

        iterator& operator++() {
            if (!handle_ || handle_.done()) {
                handle_ = nullptr;
                return *this;
            }
            
            // Use a lock to ensure thread-safety during resumption
            {
                boost::lock_guard<boost::mutex> lock(handle_.promise().mutex_);
                handle_.resume();
                if (handle_.done()) {
                    handle_ = nullptr;
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return handle_ == other.handle_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        const T& operator*() const { 
            boost::shared_lock_guard<boost::shared_mutex> lock(handle_.promise().value_mutex_);
            return handle_.promise().value(); 
        }

        const T* operator->() const { 
            boost::shared_lock_guard<boost::shared_mutex> lock(handle_.promise().value_mutex_);
            return &handle_.promise().value(); 
        }

    private:
        std::coroutine_handle<promise_type> handle_;
    };

    /**
     * @brief Thread-safe promise type for the generator coroutine
     */
    struct promise_type {
        T value_;
        std::exception_ptr exception_;
        mutable boost::mutex mutex_;              // Protects coroutine state
        mutable boost::shared_mutex value_mutex_; // Protects value access

        ThreadSafeGenerator get_return_object() {
            return ThreadSafeGenerator{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        template <std::convertible_to<T> From>
        std::suspend_always yield_value(From&& from) {
            boost::lock_guard<boost::shared_mutex> lock(value_mutex_);
            value_ = std::forward<From>(from);
            return {};
        }

        void unhandled_exception() { 
            boost::lock_guard<boost::mutex> lock(mutex_);
            exception_ = std::current_exception(); 
        }

        void return_void() {}

        const T& value() const {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return value_;
        }
    };

    /**
     * @brief Constructs a thread-safe generator from a coroutine handle
     */
    explicit ThreadSafeGenerator(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    /**
     * @brief Destructor that cleans up the coroutine
     */
    ~ThreadSafeGenerator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Rule of five - prevent copy, allow move
    ThreadSafeGenerator(const ThreadSafeGenerator&) = delete;
    ThreadSafeGenerator& operator=(const ThreadSafeGenerator&) = delete;

    ThreadSafeGenerator(ThreadSafeGenerator&& other) noexcept {
        boost::lock_guard<boost::mutex> lock(mutex_);
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }

    ThreadSafeGenerator& operator=(ThreadSafeGenerator&& other) noexcept {
        if (this != &other) {
            boost::lock_guard<boost::mutex> lock(mutex_);
            boost::lock_guard<boost::mutex> other_lock(other.mutex_);
            
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Returns an iterator pointing to the beginning of the generator
     */
    iterator begin() {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (handle_) {
            handle_.resume();
            if (handle_.done()) {
                return end();
            }
        }
        return iterator{handle_};
    }

    /**
     * @brief Returns an iterator pointing to the end of the generator
     */
    iterator end() { return iterator{nullptr}; }

private:
    std::coroutine_handle<promise_type> handle_;
    mutable boost::mutex mutex_;  // Protects handle_ access
};
#endif // ATOM_USE_BOOST_LOCKS

#ifdef ATOM_USE_BOOST_LOCKFREE
/**
 * @brief A concurrent generator that allows consumption from multiple threads
 * 
 * This generator variant uses lock-free data structures to enable efficient
 * multi-threaded consumption of generated values.
 *
 * @tparam T The type of values yielded by the generator
 * @tparam QueueSize Size of the internal lock-free queue (default: 128)
 */
template <typename T, size_t QueueSize = 128>
class ConcurrentGenerator {
public:
    struct producer_token {};
    using value_type = T;

    /**
     * @brief Constructs a concurrent generator with a producer coroutine
     * 
     * @param generator Function that produces values
     */
    template <typename Func>
    explicit ConcurrentGenerator(Func&& generator) 
        : queue_(QueueSize), done_(false), exception_ptr_(nullptr) {
        
        producer_ = std::thread([this, g = std::forward<Func>(generator)]() {
            try {
                Generator<T> gen = g();
                for (const auto& item : gen) {
                    T value = item;
                    while (!queue_.push(value) && !done_.load()) {
                        // If queue is full, yield to other threads briefly
                        std::this_thread::yield();
                    }
                    if (done_.load()) break;
                }
            } catch (...) {
                exception_ptr_ = std::current_exception();
            }
            
            // Mark producer as done
            is_producing_.store(false);
        });
    }

    /**
     * @brief Destructor that cleans up resources
     */
    ~ConcurrentGenerator() {
        done_.store(true);
        if (producer_.joinable()) {
            producer_.join();
        }
    }

    // Rule of five - prevent copy, allow move
    ConcurrentGenerator(const ConcurrentGenerator&) = delete;
    ConcurrentGenerator& operator=(const ConcurrentGenerator&) = delete;

    ConcurrentGenerator(ConcurrentGenerator&& other) noexcept {
        queue_ = std::move(other.queue_);
        producer_ = std::move(other.producer_);
        done_.store(other.done_.load());
        is_producing_.store(other.is_producing_.load());
        exception_ptr_ = other.exception_ptr_;
        other.done_.store(true);
        other.is_producing_.store(false);
        other.exception_ptr_ = nullptr;
    }

    ConcurrentGenerator& operator=(ConcurrentGenerator&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            done_.store(true);
            if (producer_.joinable()) {
                producer_.join();
            }
            
            // Move from other
            queue_ = std::move(other.queue_);
            producer_ = std::move(other.producer_);
            done_.store(other.done_.load());
            is_producing_.store(other.is_producing_.load());
            exception_ptr_ = other.exception_ptr_;
            
            // Invalidate other
            other.done_.store(true);
            other.is_producing_.store(false);
            other.exception_ptr_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Try to get the next value from the generator
     * 
     * @param value Reference to store the retrieved value
     * @return true if a value was retrieved, false if no more values
     * @throws Any exception thrown by the producer coroutine
     */
    bool try_next(T& value) {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        if (queue_.pop(value)) {
            return true;
        }
        
        // No value in queue, check if producer is done
        if (!is_producing_.load()) {
            // Double-check queue after producer is done
            return queue_.pop(value);
        }
        
        return false;
    }

    /**
     * @brief Get the next value, blocking if necessary
     * 
     * @return The next value
     * @throws std::runtime_error if no more values
     * @throws Any exception thrown by the producer coroutine
     */
    T next() {
        T value;
        while (is_producing_.load() && !done_.load()) {
            if (try_next(value)) {
                return value;
            }
            std::this_thread::yield();
        }
        
        // Last chance to get a value
        if (try_next(value)) {
            return value;
        }
        
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        throw std::runtime_error("No more values in concurrent generator");
    }

    /**
     * @brief Check if the generator is done producing values
     * 
     * @return true if the generator is done, false otherwise
     */
    bool done() const {
        return !is_producing_.load() && queue_.empty();
    }

private:
    boost::lockfree::queue<T> queue_;
    std::thread producer_;
    boost::atomic<bool> done_{false};
    boost::atomic<bool> is_producing_{true};
    std::exception_ptr exception_ptr_;
};

/**
 * @brief A lock-free two-way generator for producer-consumer pattern
 * 
 * @tparam Yield Type yielded by the producer
 * @tparam Receive Type received from the consumer
 * @tparam QueueSize Size of the internal lock-free queues
 */
template <typename Yield, typename Receive = void, size_t QueueSize = 128>
class LockFreeTwoWayGenerator {
public:
    /**
     * @brief Constructs a lock-free two-way generator
     * 
     * @param coroutine The coroutine function that will produce/consume values
     */
    template <typename Func>
    explicit LockFreeTwoWayGenerator(Func&& coroutine)
        : yield_queue_(QueueSize), 
          receive_queue_(QueueSize),
          done_(false),
          exception_ptr_(nullptr) {
        
        worker_ = std::thread([this, f = std::forward<Func>(coroutine)]() {
            try {
                TwoWayGenerator<Yield, Receive> gen = f();
                while (!done_.load() && !gen.done()) {
                    Yield value = gen.next(get_next_receive_value());
                    
                    while (!yield_queue_.push(value) && !done_.load()) {
                        // If queue is full, yield to other threads briefly
                        std::this_thread::yield();
                    }
                    
                    if (done_.load()) break;
                }
            } catch (...) {
                exception_ptr_ = std::current_exception();
            }
            
            active_.store(false);
        });
    }
    
    /**
     * @brief Destructor that cleans up resources
     */
    ~LockFreeTwoWayGenerator() {
        done_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }
    
    // Rule of five - prevent copy, allow move
    LockFreeTwoWayGenerator(const LockFreeTwoWayGenerator&) = delete;
    LockFreeTwoWayGenerator& operator=(const LockFreeTwoWayGenerator&) = delete;
    
    LockFreeTwoWayGenerator(LockFreeTwoWayGenerator&& other) noexcept
        : yield_queue_(std::move(other.yield_queue_)),
          receive_queue_(std::move(other.receive_queue_)),
          worker_(std::move(other.worker_)) {
        done_.store(other.done_.load());
        active_.store(other.active_.load());
        exception_ptr_ = other.exception_ptr_;
        
        other.done_.store(true);
        other.active_.store(false);
        other.exception_ptr_ = nullptr;
    }
    
    LockFreeTwoWayGenerator& operator=(LockFreeTwoWayGenerator&& other) noexcept {
        if (this != &other) {
            // Clean up current resources
            done_.store(true);
            if (worker_.joinable()) {
                worker_.join();
            }
            
            // Move from other
            yield_queue_ = std::move(other.yield_queue_);
            receive_queue_ = std::move(other.receive_queue_);
            worker_ = std::move(other.worker_);
            done_.store(other.done_.load());
            active_.store(other.active_.load());
            exception_ptr_ = other.exception_ptr_;
            
            // Invalidate other
            other.done_.store(true);
            other.active_.store(false);
            other.exception_ptr_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Send a value to the generator and get the next yielded value
     * 
     * @param value Value to send to the generator
     * @return The next yielded value
     * @throws std::runtime_error if the generator is done
     */
    Yield send(Receive value) {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        if (!active_.load()) {
            throw std::runtime_error("Generator is done");
        }
        
        // Send value to the coroutine
        while (!receive_queue_.push(value) && active_.load()) {
            std::this_thread::yield();
        }
        
        Yield result;
        // Wait for result
        while (!yield_queue_.pop(result) && active_.load()) {
            std::this_thread::yield();
        }
        
        if (!active_.load() && exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        return result;
    }
    
    /**
     * @brief Check if the generator is done
     * 
     * @return true if the generator is done, false otherwise
     */
    bool done() const {
        return !active_.load();
    }

private:
    boost::lockfree::spsc_queue<Yield> yield_queue_;
    boost::lockfree::spsc_queue<Receive> receive_queue_;
    std::thread worker_;
    boost::atomic<bool> done_{false};
    boost::atomic<bool> active_{true};
    std::exception_ptr exception_ptr_;
    
    Receive get_next_receive_value() {
        Receive value;
        while (!receive_queue_.pop(value) && !done_.load()) {
            std::this_thread::yield();
        }
        return value;
    }
};

// Specialization for generators that don't receive values
template <typename Yield, size_t QueueSize>
class LockFreeTwoWayGenerator<Yield, void, QueueSize> {
public:
    template <typename Func>
    explicit LockFreeTwoWayGenerator(Func&& coroutine)
        : yield_queue_(QueueSize),
          done_(false),
          exception_ptr_(nullptr) {
        
        worker_ = std::thread([this, f = std::forward<Func>(coroutine)]() {
            try {
                TwoWayGenerator<Yield, void> gen = f();
                while (!done_.load() && !gen.done()) {
                    Yield value = gen.next();
                    
                    while (!yield_queue_.push(value) && !done_.load()) {
                        std::this_thread::yield();
                    }
                    
                    if (done_.load()) break;
                }
            } catch (...) {
                exception_ptr_ = std::current_exception();
            }
            
            active_.store(false);
        });
    }
    
    ~LockFreeTwoWayGenerator() {
        done_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }
    
    // Rule of five - prevent copy, allow move
    LockFreeTwoWayGenerator(const LockFreeTwoWayGenerator&) = delete;
    LockFreeTwoWayGenerator& operator=(const LockFreeTwoWayGenerator&) = delete;
    LockFreeTwoWayGenerator(LockFreeTwoWayGenerator&&) noexcept = default;
    LockFreeTwoWayGenerator& operator=(LockFreeTwoWayGenerator&&) noexcept = default;
    
    Yield next() {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        if (!active_.load()) {
            throw std::runtime_error("Generator is done");
        }
        
        Yield result;
        // Wait for result
        while (!yield_queue_.pop(result) && active_.load()) {
            std::this_thread::yield();
        }
        
        if (!active_.load() && exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        
        return result;
    }
    
    bool done() const {
        return !active_.load();
    }

private:
    boost::lockfree::spsc_queue<Yield> yield_queue_;
    std::thread worker_;
    boost::atomic<bool> done_{false};
    boost::atomic<bool> active_{true};
    std::exception_ptr exception_ptr_;
};

/**
 * @brief Creates a concurrent generator from a regular generator function
 *
 * @tparam Func The type of the generator function
 * @tparam T The type of values yielded by the generator
 * @param func The generator function
 * @return A concurrent generator that yields the same values
 */
template <typename Func, typename T = std::invoke_result_t<Func>>
ConcurrentGenerator<typename T::promise_type::value_type> 
make_concurrent_generator(Func&& func) {
    return ConcurrentGenerator<typename T::promise_type::value_type>(std::forward<Func>(func));
}
#endif // ATOM_USE_BOOST_LOCKFREE

}  // namespace atom::async

#endif  // ATOM_ASYNC_GENERATOR_HPP
