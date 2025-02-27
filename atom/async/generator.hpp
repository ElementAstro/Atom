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
#include <ranges>
#include <stdexcept>
#include <type_traits>

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

}  // namespace atom::async

#endif  // ATOM_ASYNC_GENERATOR_HPP
