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
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_lock_guard.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

#ifdef ATOM_USE_ASIO
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
// Assuming atom::async::internal::get_asio_thread_pool() is available
// from "atom/async/future.hpp" or a similar common header.
// If not, future.hpp needs to be included before this file, or the pool getter
// needs to be accessible.
#include "atom/async/future.hpp"
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
                        // This case should ideally be prevented by the logic in
                        // next() or the coroutine should handle the possibility
                        // of no value.
                        throw std::logic_error(
                            "No value received by coroutine logic");
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
    Yield next(
        Receive value = Receive{}) {  // Default construct Receive if possible
        if (!handle_ || handle_.done()) {
            throw std::logic_error("Generator is done");
        }

        handle_.promise().value_to_receive_ = std::move(value);
        handle_.resume();

        if (handle_.promise().exception_) {  // Check for exception after resume
            std::rethrow_exception(handle_.promise().exception_);
        }
        if (handle_.done()) {  // Check if done after resume (and potential
                               // exception)
            throw std::logic_error("Generator is done after resume");
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

        if (handle_.promise().exception_) {  // Check for exception after resume
            std::rethrow_exception(handle_.promise().exception_);
        }
        if (handle_.done()) {  // Check if done after resume (and potential
                               // exception)
            throw std::logic_error("Generator is done after resume");
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
template <
    std::ranges::input_range
        Range>  // Changed from std::ranges::range for broader compatibility
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
    if (step == T{0}) {
        throw std::invalid_argument("Step cannot be zero");
    }
    if (step > T{0}) {
        for (T i = begin; i < end; i += step) {
            co_yield i;
        }
    } else {  // step < T{0}
        for (T i = begin; i > end;
             i += step) {  // Note: condition i > end for negative step
            co_yield i;
        }
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
    if (step == T{0}) {
        throw std::invalid_argument("Step cannot be zero for infinite_range");
    }
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

        explicit iterator(std::coroutine_handle<promise_type> handle = nullptr,
                          ThreadSafeGenerator* owner =
                              nullptr)  // Store owner for mutex access
            : handle_(handle), owner_(owner) {}

        iterator& operator++() {
            if (!handle_ || handle_.done() || !owner_) {
                handle_ = nullptr;
                return *this;
            }

            // Use a lock to ensure thread-safety during resumption
            {
                boost::lock_guard<boost::mutex> lock(
                    owner_->iter_mutex_);  // Lock on owner's mutex
                if (handle_.done()) {      // Re-check after acquiring lock
                    handle_ = nullptr;
                    return *this;
                }
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

        // operator* and operator-> need to access promise's value safely
        // The promise_type itself should manage safe access to its value_
        const T& operator*() const {
            if (!handle_ || !owner_)
                throw std::logic_error("Dereferencing invalid iterator");
            // The promise's value method should be thread-safe
            return handle_.promise().value();
        }

        const T* operator->() const {
            if (!handle_ || !owner_)
                throw std::logic_error("Dereferencing invalid iterator");
            return &handle_.promise().value();
        }

    private:
        std::coroutine_handle<promise_type> handle_;
        ThreadSafeGenerator*
            owner_;  // Pointer to the generator instance for mutex
    };

    /**
     * @brief Thread-safe promise type for the generator coroutine
     */
    struct promise_type {
        T value_;
        std::exception_ptr exception_;
        mutable boost::shared_mutex
            value_access_mutex_;  // Protects value_ and exception_

        ThreadSafeGenerator get_return_object() {
            return ThreadSafeGenerator{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        template <std::convertible_to<T> From>
        std::suspend_always yield_value(From&& from) {
            boost::unique_lock<boost::shared_mutex> lock(value_access_mutex_);
            value_ = std::forward<From>(from);
            return {};
        }

        void unhandled_exception() {
            boost::unique_lock<boost::shared_mutex> lock(value_access_mutex_);
            exception_ = std::current_exception();
        }

        void return_void() {}

        const T& value() const {  // Called by iterator::operator*
            boost::shared_lock<boost::shared_mutex> lock(value_access_mutex_);
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return value_;
        }
    };

    explicit ThreadSafeGenerator(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    ~ThreadSafeGenerator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    ThreadSafeGenerator(const ThreadSafeGenerator&) = delete;
    ThreadSafeGenerator& operator=(const ThreadSafeGenerator&) = delete;

    ThreadSafeGenerator(ThreadSafeGenerator&& other) noexcept
        : handle_(nullptr) {
        boost::lock_guard<boost::mutex> lock(
            other.iter_mutex_);  // Lock other before moving
        handle_ = other.handle_;
        other.handle_ = nullptr;
    }

    ThreadSafeGenerator& operator=(ThreadSafeGenerator&& other) noexcept {
        if (this != &other) {
            boost::lock_guard<boost::mutex> lock_this(iter_mutex_);
            boost::lock_guard<boost::mutex> lock_other(other.iter_mutex_);

            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    iterator begin() {
        boost::lock_guard<boost::mutex> lock(iter_mutex_);
        if (handle_) {
            handle_.resume();  // Initial resume
            if (handle_.done()) {
                return end();
            }
        }
        return iterator{handle_, this};
    }

    iterator end() { return iterator{nullptr, nullptr}; }

private:
    std::coroutine_handle<promise_type> handle_;
    mutable boost::mutex
        iter_mutex_;  // Protects handle_ and iterator operations like resume
};
#endif  // ATOM_USE_BOOST_LOCKS

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

    template <typename Func>
    explicit ConcurrentGenerator(Func&& generator_func)
        : queue_(QueueSize),
          done_(false),
          is_producing_(true),
          exception_ptr_(nullptr) {
        auto producer_lambda =
            [this, func = std::forward<Func>(generator_func)](
                std::shared_ptr<std::promise<void>> task_promise) {
                try {
                    Generator<T> gen = func();  // func returns a Generator<T>
                    for (const auto& item : gen) {
                        if (done_.load(boost::memory_order_acquire))
                            break;
                        T value = item;  // Ensure copy or move as appropriate
                        while (!queue_.push(value) &&
                               !done_.load(boost::memory_order_acquire)) {
                            std::this_thread::yield();
                        }
                        if (done_.load(boost::memory_order_acquire))
                            break;
                    }
                } catch (...) {
                    exception_ptr_ = std::current_exception();
                }
                is_producing_.store(false, boost::memory_order_release);
                if (task_promise)
                    task_promise->set_value();
            };

#ifdef ATOM_USE_ASIO
        auto p = std::make_shared<std::promise<void>>();
        task_completion_signal_ = p->get_future();
        asio::post(atom::async::internal::get_asio_thread_pool(),
                   [producer_lambda,
                    p_task = p]() mutable {  // Pass the promise to lambda
                       producer_lambda(p_task);
                   });
#else
        producer_thread_ = std::thread(
            producer_lambda,
            nullptr);  // Pass nullptr for promise when not using ASIO join
#endif
    }

    ~ConcurrentGenerator() {
        done_.store(true, boost::memory_order_release);
#ifdef ATOM_USE_ASIO
        if (task_completion_signal_.valid()) {
            try {
                task_completion_signal_.wait();
            } catch (const std::future_error&) { /* Already set or no state */
            }
        }
#else
        if (producer_thread_.joinable()) {
            producer_thread_.join();
        }
#endif
    }

    ConcurrentGenerator(const ConcurrentGenerator&) = delete;
    ConcurrentGenerator& operator=(const ConcurrentGenerator&) = delete;

    ConcurrentGenerator(ConcurrentGenerator&& other) noexcept
        : queue_(QueueSize),  // New queue, contents are not moved from lockfree
                              // queue
          done_(other.done_.load(boost::memory_order_acquire)),
          is_producing_(other.is_producing_.load(boost::memory_order_acquire)),
          exception_ptr_(other.exception_ptr_)
#ifdef ATOM_USE_ASIO
          ,
          task_completion_signal_(std::move(other.task_completion_signal_))
#else
          ,
          producer_thread_(std::move(other.producer_thread_))
#endif
    {
        // The queue itself cannot be moved in a lock-free way easily.
        // The typical pattern for moving such concurrent objects is to
        // signal the old one to stop and create a new one, or make them
        // non-movable. For simplicity here, we move the thread/task handle and
        // state, but the queue_ is default-initialized or re-initialized. This
        // implies that items in `other.queue_` are lost if not consumed before
        // move. A fully correct move for a populated lock-free queue is
        // complex. The current boost::lockfree::queue is not movable in the way
        // std::vector is. We mark the other as done.
        other.done_.store(true, boost::memory_order_release);
        other.is_producing_.store(false, boost::memory_order_release);
        other.exception_ptr_ = nullptr;
    }

    ConcurrentGenerator& operator=(ConcurrentGenerator&& other) noexcept {
        if (this != &other) {
            done_.store(true, boost::memory_order_release);  // Signal current
                                                             // producer to stop
#ifdef ATOM_USE_ASIO
            if (task_completion_signal_.valid()) {
                task_completion_signal_.wait();
            }
#else
            if (producer_thread_.joinable()) {
                producer_thread_.join();
            }
#endif
            // queue_ is not directly assignable in a meaningful way for its
            // content. Re-initialize or rely on its own state after current
            // producer stops. For this example, we'll assume queue_ is
            // effectively reset by new producer.

            done_.store(other.done_.load(boost::memory_order_acquire),
                        boost::memory_order_relaxed);
            is_producing_.store(
                other.is_producing_.load(boost::memory_order_acquire),
                boost::memory_order_relaxed);
            exception_ptr_ = other.exception_ptr_;

#ifdef ATOM_USE_ASIO
            task_completion_signal_ = std::move(other.task_completion_signal_);
#else
            producer_thread_ = std::move(other.producer_thread_);
#endif

            other.done_.store(true, boost::memory_order_release);
            other.is_producing_.store(false, boost::memory_order_release);
            other.exception_ptr_ = nullptr;
        }
        return *this;
    }

    bool try_next(T& value) {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }

        if (queue_.pop(value)) {
            return true;
        }

        if (!is_producing_.load(boost::memory_order_acquire)) {
            return queue_.pop(value);  // Final check
        }
        return false;
    }

    T next() {
        T value;
        // Check for pending exception first
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }

        while (!done_.load(
            boost::memory_order_acquire)) {  // Check overall done flag
            if (queue_.pop(value)) {
                return value;
            }
            if (!is_producing_.load(boost::memory_order_acquire) &&
                queue_.empty()) {
                // Producer is done and queue is empty
                break;
            }
            std::this_thread::yield();
        }

        // After loop, try one last time from queue or rethrow pending exception
        if (queue_.pop(value)) {
            return value;
        }
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        throw std::runtime_error("No more values in concurrent generator");
    }

    bool done() const {
        return !is_producing_.load(boost::memory_order_acquire) &&
               queue_.empty();
    }

private:
    boost::lockfree::queue<T> queue_;
#ifdef ATOM_USE_ASIO
    std::future<void> task_completion_signal_;
#else
    std::thread producer_thread_;
#endif
    boost::atomic<bool> done_;
    boost::atomic<bool> is_producing_;
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
    template <typename Func>
    explicit LockFreeTwoWayGenerator(Func&& coroutine_func)
        : yield_queue_(QueueSize),
          receive_queue_(QueueSize),
          done_(false),
          active_(true),
          exception_ptr_(nullptr) {
        auto worker_lambda =
            [this, func = std::forward<Func>(coroutine_func)](
                std::shared_ptr<std::promise<void>> task_promise) {
                try {
                    TwoWayGenerator<Yield, Receive> gen =
                        func();  // func returns TwoWayGenerator
                    while (!done_.load(boost::memory_order_acquire) &&
                           !gen.done()) {
                        Receive recv_val;
                        // If Receive is void, this logic needs adjustment.
                        // Assuming Receive is not void for the general
                        // template. The specialization for Receive=void handles
                        // the no-receive case.
                        if constexpr (!std::is_void_v<Receive>) {
                            recv_val = get_next_receive_value_internal();
                            if (done_.load(boost::memory_order_acquire))
                                break;  // Check after potentially blocking
                        }

                        Yield to_yield_val =
                            gen.next(std::move(recv_val));  // Pass if not void

                        while (!yield_queue_.push(to_yield_val) &&
                               !done_.load(boost::memory_order_acquire)) {
                            std::this_thread::yield();
                        }
                        if (done_.load(boost::memory_order_acquire))
                            break;
                    }
                } catch (...) {
                    exception_ptr_ = std::current_exception();
                }
                active_.store(false, boost::memory_order_release);
                if (task_promise)
                    task_promise->set_value();
            };

#ifdef ATOM_USE_ASIO
        auto p = std::make_shared<std::promise<void>>();
        task_completion_signal_ = p->get_future();
        asio::post(
            atom::async::internal::get_asio_thread_pool(),
            [worker_lambda, p_task = p]() mutable { worker_lambda(p_task); });
#else
        worker_thread_ = std::thread(worker_lambda, nullptr);
#endif
    }

    ~LockFreeTwoWayGenerator() {
        done_.store(true, boost::memory_order_release);
#ifdef ATOM_USE_ASIO
        if (task_completion_signal_.valid()) {
            try {
                task_completion_signal_.wait();
            } catch (const std::future_error&) {
            }
        }
#else
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
#endif
    }

    LockFreeTwoWayGenerator(const LockFreeTwoWayGenerator&) = delete;
    LockFreeTwoWayGenerator& operator=(const LockFreeTwoWayGenerator&) = delete;

    LockFreeTwoWayGenerator(LockFreeTwoWayGenerator&& other) noexcept
        : yield_queue_(QueueSize),
          receive_queue_(QueueSize),  // Queues are not moved
          done_(other.done_.load(boost::memory_order_acquire)),
          active_(other.active_.load(boost::memory_order_acquire)),
          exception_ptr_(other.exception_ptr_)
#ifdef ATOM_USE_ASIO
          ,
          task_completion_signal_(std::move(other.task_completion_signal_))
#else
          ,
          worker_thread_(std::move(other.worker_thread_))
#endif
    {
        other.done_.store(true, boost::memory_order_release);
        other.active_.store(false, boost::memory_order_release);
        other.exception_ptr_ = nullptr;
    }

    LockFreeTwoWayGenerator& operator=(
        LockFreeTwoWayGenerator&& other) noexcept {
        if (this != &other) {
            done_.store(true, boost::memory_order_release);
#ifdef ATOM_USE_ASIO
            if (task_completion_signal_.valid()) {
                task_completion_signal_.wait();
            }
#else
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
#endif
            done_.store(other.done_.load(boost::memory_order_acquire),
                        boost::memory_order_relaxed);
            active_.store(other.active_.load(boost::memory_order_acquire),
                          boost::memory_order_relaxed);
            exception_ptr_ = other.exception_ptr_;
#ifdef ATOM_USE_ASIO
            task_completion_signal_ = std::move(other.task_completion_signal_);
#else
            worker_thread_ = std::move(other.worker_thread_);
#endif
            other.done_.store(true, boost::memory_order_release);
            other.active_.store(false, boost::memory_order_release);
            other.exception_ptr_ = nullptr;
        }
        return *this;
    }

    Yield send(Receive value) {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        if (!active_.load(boost::memory_order_acquire) &&
            yield_queue_.empty()) {  // More robust check
            throw std::runtime_error("Generator is done");
        }

        while (!receive_queue_.push(value) &&
               active_.load(boost::memory_order_acquire)) {
            if (done_.load(boost::memory_order_acquire))
                throw std::runtime_error("Generator shutting down during send");
            std::this_thread::yield();
        }

        Yield result;
        while (!yield_queue_.pop(result)) {
            if (!active_.load(boost::memory_order_acquire) &&
                yield_queue_
                    .empty()) {  // Check if worker stopped and queue is empty
                if (exception_ptr_)
                    std::rethrow_exception(exception_ptr_);
                throw std::runtime_error(
                    "Generator stopped while waiting for yield");
            }
            if (done_.load(boost::memory_order_acquire))
                throw std::runtime_error(
                    "Generator shutting down while waiting for yield");
            std::this_thread::yield();
        }

        // Final check for exception after potentially successful pop
        if (!active_.load(boost::memory_order_acquire) && exception_ptr_ &&
            yield_queue_.empty()) {
            // This case is tricky: value might have been popped just before an
            // exception was set and active_ turned false. The exception_ptr_
            // check at the beginning of the function is primary.
        }
        return result;
    }

    bool done() const {
        return !active_.load(boost::memory_order_acquire) &&
               yield_queue_.empty() && receive_queue_.empty();
    }

private:
    boost::lockfree::spsc_queue<Yield> yield_queue_;
    boost::lockfree::spsc_queue<Receive>
        receive_queue_;  // SPSC if one consumer (this class) and one producer
                         // (worker_lambda)
#ifdef ATOM_USE_ASIO
    std::future<void> task_completion_signal_;
#else
    std::thread worker_thread_;
#endif
    boost::atomic<bool> done_;
    boost::atomic<bool> active_;
    std::exception_ptr exception_ptr_;

    Receive get_next_receive_value_internal() {
        Receive value;
        while (!receive_queue_.pop(value) &&
               !done_.load(boost::memory_order_acquire)) {
            std::this_thread::yield();
        }
        if (done_.load(boost::memory_order_acquire) &&
            !receive_queue_.pop(
                value)) {  // Check if done and queue became empty
            // This situation means we were signaled to stop while waiting for a
            // receive value. The coroutine might not get a valid value. How it
            // handles this depends on its logic. For now, if Receive is default
            // constructible, return that, otherwise it's UB or an error.
            if constexpr (std::is_default_constructible_v<Receive>)
                return Receive{};
            else
                throw std::runtime_error(
                    "Generator stopped while waiting for receive value, and "
                    "value type not default constructible.");
        }
        return value;
    }
};

// Specialization for generators that don't receive values (Receive = void)
template <typename Yield, size_t QueueSize>
class LockFreeTwoWayGenerator<Yield, void, QueueSize> {
public:
    template <typename Func>
    explicit LockFreeTwoWayGenerator(Func&& coroutine_func)
        : yield_queue_(QueueSize),
          done_(false),
          active_(true),
          exception_ptr_(nullptr) {
        auto worker_lambda =
            [this, func = std::forward<Func>(coroutine_func)](
                std::shared_ptr<std::promise<void>> task_promise) {
                try {
                    TwoWayGenerator<Yield, void> gen =
                        func();  // func returns TwoWayGenerator<Yield, void>
                    while (!done_.load(boost::memory_order_acquire) &&
                           !gen.done()) {
                        Yield to_yield_val =
                            gen.next();  // No value sent to next()

                        while (!yield_queue_.push(to_yield_val) &&
                               !done_.load(boost::memory_order_acquire)) {
                            std::this_thread::yield();
                        }
                        if (done_.load(boost::memory_order_acquire))
                            break;
                    }
                } catch (...) {
                    exception_ptr_ = std::current_exception();
                }
                active_.store(false, boost::memory_order_release);
                if (task_promise)
                    task_promise->set_value();
            };

#ifdef ATOM_USE_ASIO
        auto p = std::make_shared<std::promise<void>>();
        task_completion_signal_ = p->get_future();
        asio::post(
            atom::async::internal::get_asio_thread_pool(),
            [worker_lambda, p_task = p]() mutable { worker_lambda(p_task); });
#else
        worker_thread_ = std::thread(worker_lambda, nullptr);
#endif
    }

    ~LockFreeTwoWayGenerator() {
        done_.store(true, boost::memory_order_release);
#ifdef ATOM_USE_ASIO
        if (task_completion_signal_.valid()) {
            try {
                task_completion_signal_.wait();
            } catch (const std::future_error&) {
            }
        }
#else
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
#endif
    }

    LockFreeTwoWayGenerator(const LockFreeTwoWayGenerator&) = delete;
    LockFreeTwoWayGenerator& operator=(const LockFreeTwoWayGenerator&) = delete;

    LockFreeTwoWayGenerator(LockFreeTwoWayGenerator&& other) noexcept
        : yield_queue_(QueueSize),  // Queue not moved
          done_(other.done_.load(boost::memory_order_acquire)),
          active_(other.active_.load(boost::memory_order_acquire)),
          exception_ptr_(other.exception_ptr_)
#ifdef ATOM_USE_ASIO
          ,
          task_completion_signal_(std::move(other.task_completion_signal_))
#else
          ,
          worker_thread_(std::move(other.worker_thread_))
#endif
    {
        other.done_.store(true, boost::memory_order_release);
        other.active_.store(false, boost::memory_order_release);
        other.exception_ptr_ = nullptr;
    }

    LockFreeTwoWayGenerator& operator=(
        LockFreeTwoWayGenerator&& other) noexcept {
        if (this != &other) {
            done_.store(true, boost::memory_order_release);
#ifdef ATOM_USE_ASIO
            if (task_completion_signal_.valid()) {
                task_completion_signal_.wait();
            }
#else
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
#endif
            done_.store(other.done_.load(boost::memory_order_acquire),
                        boost::memory_order_relaxed);
            active_.store(other.active_.load(boost::memory_order_acquire),
                          boost::memory_order_relaxed);
            exception_ptr_ = other.exception_ptr_;
#ifdef ATOM_USE_ASIO
            task_completion_signal_ = std::move(other.task_completion_signal_);
#else
            worker_thread_ = std::move(other.worker_thread_);
#endif
            other.done_.store(true, boost::memory_order_release);
            other.active_.store(false, boost::memory_order_release);
            other.exception_ptr_ = nullptr;
        }
        return *this;
    }

    Yield next() {
        if (exception_ptr_) {
            std::rethrow_exception(exception_ptr_);
        }
        if (!active_.load(boost::memory_order_acquire) &&
            yield_queue_.empty()) {
            throw std::runtime_error("Generator is done");
        }

        Yield result;
        while (!yield_queue_.pop(result)) {
            if (!active_.load(boost::memory_order_acquire) &&
                yield_queue_.empty()) {
                if (exception_ptr_)
                    std::rethrow_exception(exception_ptr_);
                throw std::runtime_error(
                    "Generator stopped while waiting for yield");
            }
            if (done_.load(boost::memory_order_acquire))
                throw std::runtime_error(
                    "Generator shutting down while waiting for yield");
            std::this_thread::yield();
        }
        return result;
    }

    bool done() const {
        return !active_.load(boost::memory_order_acquire) &&
               yield_queue_.empty();
    }

private:
    boost::lockfree::spsc_queue<Yield> yield_queue_;
#ifdef ATOM_USE_ASIO
    std::future<void> task_completion_signal_;
#else
    std::thread worker_thread_;
#endif
    boost::atomic<bool> done_;
    boost::atomic<bool> active_;
    std::exception_ptr exception_ptr_;
};

/**
 * @brief Creates a concurrent generator from a regular generator function
 *
 * @tparam Func The type of the generator function (must return a Generator<V>)
 * @param func The generator function
 * @return A concurrent generator that yields the same values
 */
template <typename Func>
// Helper to deduce V from Generator<V> = std::invoke_result_t<Func>
// This requires Func to be a no-argument callable returning Generator<V>
// e.g. auto my_gen_func() -> Generator<int> { co_yield 1; }
// make_concurrent_generator(my_gen_func);
auto make_concurrent_generator(Func&& func) {
    using GenType = std::invoke_result_t<Func>;  // Should be Generator<V>
    using ValueType = typename GenType::promise_type::value_type;  // Extracts V
    return ConcurrentGenerator<ValueType>(std::forward<Func>(func));
}
#endif  // ATOM_USE_BOOST_LOCKFREE

}  // namespace atom::async

#endif  // ATOM_ASYNC_GENERATOR_HPP
