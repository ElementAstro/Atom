#ifndef ATOM_ASYNC_SIGNAL_HPP
#define ATOM_ASYNC_SIGNAL_HPP

#include <algorithm>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <deque>
#include <exception>
#include <execution>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace atom::async {

template <typename T>
class Slot {
public:
    explicit Slot(size_t capacity = 1)
        : capacity_(capacity == 0 ? 1 : capacity) {}

    bool hasValue() const {
        std::scoped_lock lock(mutex_);
        return !queue_.empty();
    }

    bool empty() const {
        std::scoped_lock lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::scoped_lock lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::scoped_lock lock(mutex_);
        queue_.clear();
        not_full_.notify_all();
    }

    void waitForEmpty() {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] { return queue_.empty(); });
    }

    void waitForSpace() {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] { return queue_.size() < capacity_; });
    }

    void put(const T& value) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] { return queue_.size() < capacity_; });
        queue_.push_back(value);
        lock.unlock();
        not_empty_.notify_one();
    }

    void put(T&& value) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] { return queue_.size() < capacity_; });
        queue_.push_back(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
    }

    bool tryPut(const T& value) {
        std::unique_lock lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(value);
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    bool tryPut(T&& value) {
        std::unique_lock lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    T get() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, [&] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    template <typename Rep, typename Period>
    std::optional<T> getWithTimeout(
        const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock lock(mutex_);
        if (!not_empty_.wait_for(lock, timeout,
                                 [&] { return !queue_.empty(); })) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    std::optional<T> tryGet() {
        std::unique_lock lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> queue_;
    size_t capacity_;
};

class SlotConnectionError : public std::runtime_error {
public:
    explicit SlotConnectionError(const std::string& message)
        : std::runtime_error(message) {}
};

class SlotEmissionError : public std::runtime_error {
public:
    explicit SlotEmissionError(const std::string& message)
        : std::runtime_error(message) {}
};

template <typename T, typename... Args>
concept SlotInvocable = std::invocable<T, Args...>;

namespace detail {

/**
 * @brief Base class providing common signal functionality.
 *
 * This class encapsulates the common slot storage and thread-safe operations
 * shared by all Signal variants, reducing code duplication.
 *
 * @tparam Args The argument types for the slots.
 * @tparam MutexType The mutex type to use (std::mutex or std::shared_mutex).
 */
template <typename MutexType, typename... Args>
class SignalBase {
public:
    using SlotType = std::function<void(Args...)>;

    SignalBase() = default;
    virtual ~SignalBase() = default;

    SignalBase(const SignalBase&) = delete;
    SignalBase& operator=(const SignalBase&) = delete;
    SignalBase(SignalBase&&) = default;
    SignalBase& operator=(SignalBase&&) = default;

    /**
     * @brief Connect a slot to the signal.
     * @param slot The slot to connect.
     * @throws SlotConnectionError if the slot is invalid.
     */
    void connect(SlotType slot) {
        if (!slot) {
            throw SlotConnectionError("Cannot connect invalid slot");
        }
        std::lock_guard lock(mutex_);
        slots_.push_back(std::move(slot));
    }

    /**
     * @brief Disconnect a slot from the signal by target type comparison.
     * @param slot The slot to disconnect.
     * @note This uses target_type() comparison which may not work for lambdas.
     */
    void disconnect(const SlotType& slot) noexcept {
        if (!slot) {
            return;
        }
        std::lock_guard lock(mutex_);
        slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                    [&](const SlotType& s) {
                                        return s.target_type() ==
                                               slot.target_type();
                                    }),
                     slots_.end());
    }

    /**
     * @brief Clear all slots connected to this signal.
     */
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        slots_.clear();
    }

    /**
     * @brief Get the number of connected slots.
     * @return size_t The number of slots.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.size();
    }

    /**
     * @brief Check if the signal has no connected slots.
     * @return bool True if the signal has no slots, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.empty();
    }

protected:
    /**
     * @brief Get a copy of slots for iteration outside lock.
     * @return std::vector<SlotType> Copy of all slots.
     */
    [[nodiscard]] std::vector<SlotType> getSlotsCopy() const {
        std::lock_guard lock(mutex_);
        return slots_;
    }

    /**
     * @brief Execute all slots with the given arguments.
     * @param args The arguments to pass to slots.
     * @throws SlotEmissionError if any slot throws.
     */
    void executeSlots(Args... args) {
        try {
            std::lock_guard lock(mutex_);
            for (const auto& slot : slots_) {
                if (slot) {
                    slot(args...);
                }
            }
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during slot emission: ") + e.what());
        }
    }

    std::vector<SlotType> slots_;
    mutable MutexType mutex_;
};

}  // namespace detail

/**
 * @brief A signal class that allows connecting, disconnecting, and emitting
 * slots.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class Signal : public detail::SignalBase<std::mutex, Args...> {
public:
    using Base = detail::SignalBase<std::mutex, Args...>;
    using SlotType = typename Base::SlotType;

    using Base::clear;
    using Base::connect;
    using Base::disconnect;
    using Base::empty;
    using Base::size;

    /**
     * @brief Emit the signal, calling all connected slots.
     * @param args The arguments to pass to the slots.
     */
    void emit(Args... args) { Base::executeSlots(args...); }
};

/**
 * @brief A signal class that allows asynchronous slot execution.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class AsyncSignal : public detail::SignalBase<std::mutex, Args...> {
public:
    using Base = detail::SignalBase<std::mutex, Args...>;
    using SlotType = typename Base::SlotType;

    using Base::clear;
    using Base::connect;
    using Base::disconnect;
    using Base::empty;
    using Base::size;

    /**
     * @brief Emit the signal asynchronously, calling all connected slots.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any asynchronous execution fails
     */
    void emit(Args... args) {
        auto slots_copy = Base::getSlotsCopy();
        std::vector<std::future<void>> futures;
        futures.reserve(slots_copy.size());

        for (const auto& slot : slots_copy) {
            if (slot) {
                futures.push_back(
                    std::async(std::launch::async, [slot, args...]() {
                        try {
                            slot(args...);
                        } catch (const std::exception& e) {
                            throw SlotEmissionError(
                                std::string("Async slot execution failed: ") +
                                e.what());
                        }
                    }));
            }
        }

        // Wait for all futures to complete
        for (auto& future : futures) {
            try {
                future.get();
            } catch (const std::exception& e) {
                throw SlotEmissionError(
                    std::string("Async slot execution failed: ") + e.what());
            }
        }
    }

    /**
     * @brief Wait for all slots to finish execution.
     */
    void waitForCompletion() noexcept {
        // Purposefully empty - futures are waited for in emit
    }
};

/**
 * @brief A signal class that allows automatic disconnection of slots.
 *
 * Uses ID-based slot management for reliable disconnection.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class AutoDisconnectSignal {
public:
    using SlotType = std::function<void(Args...)>;
    using ConnectionId = int64_t;

    /**
     * @brief Connect a slot to the signal and return its unique ID.
     *
     * @param slot The slot to connect.
     * @return ConnectionId The unique ID of the connected slot.
     * @throws SlotConnectionError if the slot is invalid
     */
    auto connect(SlotType slot) -> ConnectionId {
        if (!slot) {
            throw SlotConnectionError("Cannot connect invalid slot");
        }

        std::lock_guard lock(mutex_);
        auto id = nextId_++;
        slots_.emplace(id, std::move(slot));
        return id;
    }

    /**
     * @brief Disconnect a slot from the signal using its unique ID.
     *
     * @param id The unique ID of the slot to disconnect.
     * @return bool True if the slot was disconnected, false if it wasn't found.
     */
    [[nodiscard]] bool disconnect(ConnectionId id) noexcept {
        std::lock_guard lock(mutex_);
        return slots_.erase(id) > 0;
    }

    /**
     * @brief Emit the signal, calling all connected slots.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        try {
            std::lock_guard lock(mutex_);
            for (const auto& [id, slot] : slots_) {
                if (slot) {
                    slot(args...);
                }
            }
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during slot emission: ") + e.what());
        }
    }

    /**
     * @brief Clear all slots connected to this signal.
     */
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        slots_.clear();
    }

    /**
     * @brief Get the number of connected slots.
     * @return size_t The number of slots.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.size();
    }

    /**
     * @brief Check if the signal has no connected slots.
     * @return bool True if the signal has no slots, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.empty();
    }

private:
    std::map<ConnectionId, SlotType> slots_;
    mutable std::mutex mutex_;
    ConnectionId nextId_ = 0;
};

/**
 * @brief A signal class that allows chaining of signals.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class ChainedSignal {
public:
    using SlotType = std::function<void(Args...)>;
    using SignalPtr = std::shared_ptr<ChainedSignal<Args...>>;
    using WeakSignalPtr = std::weak_ptr<ChainedSignal<Args...>>;

    /**
     * @brief Connect a slot to the signal.
     *
     * @param slot The slot to connect.
     * @throws SlotConnectionError if the slot is invalid
     */
    void connect(SlotType slot) noexcept(false) {
        if (!slot) {
            throw SlotConnectionError("Cannot connect invalid slot");
        }

        std::lock_guard lock(mutex_);
        slots_.push_back(std::move(slot));
    }

    /**
     * @brief Add a chained signal to be emitted after this signal.
     *
     * @param nextSignal The next signal to chain.
     */
    void addChain(ChainedSignal<Args...>& nextSignal) noexcept {
        std::lock_guard lock(mutex_);
        // Non-owning wrapper to create a weak_ptr without owning the signal
        struct NonOwningShared : std::enable_shared_from_this<NonOwningShared> {
            ChainedSignal<Args...>* ptr;
        };
        auto sp = std::shared_ptr<ChainedSignal<Args...>>(
            &nextSignal, [](ChainedSignal<Args...>*) {});
        chains_.push_back(WeakSignalPtr(sp));
    }

    /**
     * @brief Add a chained signal using shared_ptr to be emitted after this
     * signal.
     *
     * @param nextSignal The next signal to chain.
     */
    void addChain(const SignalPtr& nextSignal) noexcept {
        if (!nextSignal) {
            return;
        }

        std::lock_guard lock(mutex_);
        chains_.push_back(WeakSignalPtr(nextSignal));
    }

    /**
     * @brief Emit the signal, calling all connected slots and chained signals.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        try {
            // Process local slots
            {
                std::lock_guard lock(mutex_);
                for (const auto& slot : slots_) {
                    if (slot) {
                        slot(args...);
                    }
                }
            }

            // Process chained signals
            std::vector<SignalPtr> validChains;
            {
                std::lock_guard lock(mutex_);
                validChains.reserve(chains_.size());
                for (auto it = chains_.begin(); it != chains_.end();) {
                    if (auto signal = it->lock()) {
                        validChains.push_back(signal);
                        ++it;
                    } else {
                        // Remove expired weak pointers
                        it = chains_.erase(it);
                    }
                }
            }

            // Emit on valid chains
            for (const auto& signal : validChains) {
                signal->emit(args...);
            }
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during chained slot emission: ") + e.what());
        }
    }

    /**
     * @brief Clear all slots and chains connected to this signal.
     */
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        slots_.clear();
        chains_.clear();
    }

private:
    std::vector<SlotType> slots_;
    std::vector<WeakSignalPtr> chains_;
    mutable std::mutex mutex_;
};

/**
 * @brief A template for signals with advanced thread-safety for readers and
 * writers.
 *
 * Uses shared_mutex for better read concurrency and parallel execution
 * for emitting to multiple slots.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class ThreadSafeSignal : public detail::SignalBase<std::shared_mutex, Args...> {
public:
    using Base = detail::SignalBase<std::shared_mutex, Args...>;
    using SlotType = typename Base::SlotType;

    using Base::clear;
    using Base::connect;
    using Base::disconnect;
    using Base::empty;
    using Base::size;

    /// Threshold for parallel execution
    static constexpr size_t PARALLEL_THRESHOLD = 4;

    /**
     * @brief Emit the signal using parallel execution for multiple slots.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        try {
            auto slots_copy = Base::getSlotsCopy();

            // Use C++17 parallel execution if there are enough slots
            if (slots_copy.size() > PARALLEL_THRESHOLD) {
                std::for_each(std::execution::par_unseq, slots_copy.begin(),
                              slots_copy.end(),
                              [&args...](const SlotType& slot) {
                                  if (slot) {
                                      slot(args...);
                                  }
                              });
            } else {
                for (const auto& slot : slots_copy) {
                    if (slot) {
                        slot(args...);
                    }
                }
            }
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during thread-safe slot emission: ") +
                e.what());
        }
    }
};

/**
 * @brief A signal class that limits the number of times it can be emitted.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class LimitedSignal : public detail::SignalBase<std::mutex, Args...> {
public:
    using Base = detail::SignalBase<std::mutex, Args...>;
    using SlotType = typename Base::SlotType;

    using Base::clear;
    using Base::connect;
    using Base::disconnect;
    using Base::empty;
    using Base::size;

    /**
     * @brief Construct a new Limited Signal object.
     *
     * @param maxCalls The maximum number of times the signal can be emitted.
     * @throws std::invalid_argument if maxCalls is zero
     */
    explicit LimitedSignal(size_t maxCalls) : maxCalls_(maxCalls) {
        if (maxCalls == 0) {
            throw std::invalid_argument(
                "Maximum calls must be greater than zero");
        }
    }

    /**
     * @brief Emit the signal, calling all connected slots up to the maximum
     * number of calls.
     *
     * @param args The arguments to pass to the slots.
     * @return bool True if the signal was emitted, false if the call limit was
     * reached
     * @throws SlotEmissionError if any slot execution fails
     */
    [[nodiscard]] bool emit(Args... args) {
        try {
            std::lock_guard lock(Base::mutex_);
            if (callCount_ >= maxCalls_) {
                return false;
            }

            for (const auto& slot : Base::slots_) {
                if (slot) {
                    slot(args...);
                }
            }

            ++callCount_;
            return true;
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during limited slot emission: ") + e.what());
        }
    }

    /**
     * @brief Check if the signal has reached its call limit.
     * @return bool True if the call limit has been reached
     */
    [[nodiscard]] bool isExhausted() const noexcept {
        std::lock_guard lock(Base::mutex_);
        return callCount_ >= maxCalls_;
    }

    /**
     * @brief Get remaining call count before limit is reached.
     * @return size_t Number of remaining emissions
     */
    [[nodiscard]] size_t remainingCalls() const noexcept {
        std::lock_guard lock(Base::mutex_);
        return (callCount_ < maxCalls_) ? (maxCalls_ - callCount_) : 0;
    }

    /**
     * @brief Reset the call counter.
     */
    void reset() noexcept {
        std::lock_guard lock(Base::mutex_);
        callCount_ = 0;
    }

private:
    const size_t maxCalls_;
    size_t callCount_{0};
};

/**
 * @brief A signal class that uses C++20 coroutines for asynchronous slot
 * execution
 *
 * @tparam Args The argument types for the slots
 */
template <typename... Args>
class CoroutineSignal {
public:
    using SlotType = std::function<void(Args...)>;

    // Coroutine support structure
    struct EmitTask {
        struct promise_type {
            EmitTask get_return_object() {
                return {
                    std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() {
                exception_ = std::current_exception();
            }

            std::exception_ptr exception_;
        };

        std::coroutine_handle<promise_type> handle;

        EmitTask(std::coroutine_handle<promise_type> h) : handle(h) {}
        ~EmitTask() {
            if (handle) {
                handle.destroy();
            }
        }
    };

    /**
     * @brief Connect a slot to the signal.
     *
     * @param slot The slot to connect.
     * @throws SlotConnectionError if the slot is invalid
     */
    void connect(SlotType slot) noexcept(false) {
        if (!slot) {
            throw SlotConnectionError("Cannot connect invalid slot");
        }

        std::lock_guard lock(mutex_);
        slots_.push_back(std::move(slot));
    }

    /**
     * @brief Disconnect a slot from the signal.
     *
     * @param slot The slot to disconnect.
     */
    void disconnect(const SlotType& slot) noexcept {
        if (!slot) {
            return;
        }

        std::lock_guard lock(mutex_);
        slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                    [&](const SlotType& s) {
                                        return s.target_type() ==
                                               slot.target_type();
                                    }),
                     slots_.end());
    }

    /**
     * @brief Emit the signal asynchronously using C++20 coroutines
     *
     * @param args The arguments to pass to the slots
     * @return EmitTask Coroutine task that completes when all slots are
     * executed
     */
    [[nodiscard]] EmitTask emit(Args... args) {
        std::vector<SlotType> slots_copy;
        {
            std::lock_guard lock(mutex_);
            slots_copy = slots_;
        }

        for (const auto& slot : slots_copy) {
            if (slot) {
                // 修复：避免在 try-catch 块中使用 co_yield
                bool had_exception = false;
                std::exception_ptr eptr;

                try {
                    slot(args...);
                } catch (...) {
                    had_exception = true;
                    eptr = std::current_exception();
                }

                // 在 try-catch 块外处理异常
                if (had_exception && eptr) {
                    // 设置协程的异常状态
                    std::rethrow_exception(eptr);
                }

                // Yield to allow other coroutines to execute
                co_await std::suspend_always{};
            }
        }
    }

private:
    std::vector<SlotType> slots_;
    mutable std::mutex mutex_;
};

/**
 * @brief A signal class that uses shared_ptr for scoped slot management.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class ScopedSignal {
public:
    using SlotType = std::function<void(Args...)>;
    using SlotPtr = std::shared_ptr<SlotType>;
    using WeakSlotPtr = std::weak_ptr<SlotType>;

    /**
     * @brief Connect a slot to the signal using a shared pointer.
     *
     * @param slotPtr The shared pointer to the slot to connect.
     * @throws SlotConnectionError if the slot pointer is null
     */
    void connect(SlotPtr slotPtr) noexcept(false) {
        if (!slotPtr || !(*slotPtr)) {
            throw SlotConnectionError("Cannot connect null slot");
        }

        std::lock_guard lock(mutex_);
        slots_.push_back(WeakSlotPtr(slotPtr));
    }

    /**
     * @brief Create a slot from a callable and connect it to the signal.
     *
     * @tparam Callable The callable type
     * @param callable The callable object
     * @throws SlotConnectionError if the callable cannot be converted to a slot
     */
    template <SlotInvocable<Args...> Callable>
    void connect(Callable&& callable) {
        try {
            auto slot =
                std::make_shared<SlotType>(std::forward<Callable>(callable));
            connect(std::move(slot));
        } catch (const std::exception& e) {
            throw SlotConnectionError(std::string("Failed to create slot: ") +
                                      e.what());
        }
    }

    /**
     * @brief Emit the signal, calling all connected slots.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        try {
            std::lock_guard lock(mutex_);
            // Remove expired weak_ptr slots
            auto it = std::remove_if(
                slots_.begin(), slots_.end(),
                [](const WeakSlotPtr& weakSlot) { return weakSlot.expired(); });
            slots_.erase(it, slots_.end());

            for (const auto& weakSlot : slots_) {
                if (auto slot = weakSlot.lock()) {
                    (*slot)(args...);
                }
            }
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during scoped slot emission: ") + e.what());
        }
    }

    /**
     * @brief Clear all slots connected to this signal.
     */
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        slots_.clear();
    }

    /**
     * @brief Get the number of connected slots.
     *
     * @return size_t The number of valid slots.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return std::count_if(
            slots_.begin(), slots_.end(),
            [](const WeakSlotPtr& weakSlot) { return !weakSlot.expired(); });
    }

private:
    std::vector<WeakSlotPtr> slots_;
    mutable std::mutex mutex_;
};

}  // namespace atom::async

#endif
