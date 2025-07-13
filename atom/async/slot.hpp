#ifndef ATOM_ASYNC_SIGNAL_HPP
#define ATOM_ASYNC_SIGNAL_HPP

#include <algorithm>
#include <concepts>
#include <exception>
#include <execution>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

namespace atom::async {

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

/**
 * @brief A signal class that allows connecting, disconnecting, and emitting
 * slots. Uses a single mutex for thread safety.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;

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
     * @brief Emit the signal, calling all connected slots.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        // Copy slots under lock to allow concurrent connect/disconnect during
        // emission
        std::vector<SlotType> slots_copy;
        {
            std::lock_guard lock(mutex_);
            slots_copy = slots_;
        }

        try {
            for (const auto& slot : slots_copy) {
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
     *
     * @return size_t The number of slots.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.size();
    }

    /**
     * @brief Check if the signal has no connected slots.
     *
     * @return bool True if the signal has no slots, false otherwise.
     */
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.empty();
    }

private:
    std::vector<SlotType> slots_;
    mutable std::mutex mutex_;
};

/**
 * @brief A signal class that allows asynchronous slot execution using
 * std::async. Emission is non-blocking, returning futures for each slot.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class AsyncSignal {
public:
    using SlotType = std::function<void(Args...)>;

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
     * @brief Emit the signal asynchronously, calling all connected slots.
     * Returns a vector of futures, allowing the caller to wait for specific
     * slots or all of them later.
     *
     * @param args The arguments to pass to the slots.
     * @return std::vector<std::future<void>> A vector of futures, one for each
     * launched slot task.
     */
    [[nodiscard]] std::vector<std::future<void>> emit(Args... args) {
        std::vector<SlotType> slots_copy;
        {
            std::lock_guard lock(mutex_);
            slots_copy = slots_;
        }

        std::vector<std::future<void>> futures;
        futures.reserve(slots_copy.size());
        for (const auto& slot : slots_copy) {
            if (slot) {
                futures.push_back(
                    std::async(std::launch::async, [slot, args...]() {
                        try {
                            slot(args...);
                        } catch (const std::exception& e) {
                            // Log or handle exception within the async task
                            // Re-throwing here won't be caught by the emitter
                            // unless future.get() is called.
                            // For simplicity, we rethrow so future.get() can
                            // propagate it.
                            throw SlotEmissionError(
                                std::string("Async slot execution failed: ") +
                                e.what());
                        }
                    }));
            }
        }
        return futures;  // Return futures immediately, do not block
    }

    /**
     * @brief Clear all slots connected to this signal.
     */
    void clear() noexcept {
        std::lock_guard lock(mutex_);
        slots_.clear();
    }

private:
    std::vector<SlotType> slots_;
    mutable std::mutex mutex_;
};

/**
 * @brief A signal class that allows automatic disconnection of slots using
 * unique IDs.
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
    auto connect(SlotType slot) noexcept(false) -> ConnectionId {
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
        // Copy slots under lock to allow concurrent connect/disconnect during
        // emission
        std::map<ConnectionId, SlotType> slots_copy;
        {
            std::lock_guard lock(mutex_);
            slots_copy = slots_;
        }

        try {
            for (const auto& [id, slot] : slots_copy) {
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
     *
     * @return size_t The number of slots.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return slots_.size();
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
        // Store as weak_ptr to prevent circular references
        chains_.push_back(WeakSignalPtr(&nextSignal));
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
        chains_.push_back(nextSignal);
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
            std::vector<SlotType> slots_copy;
            {
                std::lock_guard lock(mutex_);
                slots_copy = slots_;
            }

            for (const auto& slot : slots_copy) {
                if (slot) {
                    slot(args...);
                }
            }

            // Process chained signals
            std::vector<SignalPtr> validChains;
            {
                std::lock_guard lock(mutex_);
                // Use erase-remove idiom with weak_ptr lock check
                auto it = std::remove_if(chains_.begin(), chains_.end(),
                                         [&](const WeakSignalPtr& wp) {
                                             if (auto signal = wp.lock()) {
                                                 validChains.push_back(signal);
                                                 return false;  // Keep valid
                                             }
                                             return true;  // Erase expired
                                         });
                chains_.erase(it, chains_.end());
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
 * writers using std::shared_mutex and parallel execution.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class ThreadSafeSignal {
public:
    using SlotType = std::function<void(Args...)>;

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

        std::unique_lock lock(mutex_);
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

        std::unique_lock lock(mutex_);
        slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                    [&](const SlotType& s) {
                                        return s.target_type() ==
                                               slot.target_type();
                                    }),
                     slots_.end());
    }

    /**
     * @brief Emit the signal using parallel execution for slots.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        std::vector<SlotType> slots_copy;
        {
            std::shared_lock lock(mutex_);  // Read-only lock for copying
            slots_copy = slots_;
        }

        try {
            // Use C++17 parallel execution if there are enough slots
            if (slots_copy.size() > 4) {  // Heuristic threshold
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

    /**
     * @brief Get the number of connected slots.
     *
     * @return size_t The number of slots.
     */
    [[nodiscard]] size_t size() const noexcept {
        std::shared_lock lock(mutex_);
        return slots_.size();
    }

    /**
     * @brief Clear all slots connected to this signal.
     */
    void clear() noexcept {
        std::unique_lock lock(mutex_);
        slots_.clear();
    }

private:
    std::vector<SlotType> slots_;
    mutable std::shared_mutex  // Allows multiple readers or single writer
        mutex_;
};

/**
 * @brief A signal class that limits the number of times it can be emitted.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class LimitedSignal {
public:
    using SlotType = std::function<void(Args...)>;

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
     * @brief Emit the signal, calling all connected slots up to the maximum
     * number of calls.
     *
     * @param args The arguments to pass to the slots.
     * @return bool True if the signal was emitted, false if the call limit was
     * reached
     * @throws SlotEmissionError if any slot execution fails
     */
    [[nodiscard]] bool emit(Args... args) {
        std::vector<SlotType> slots_copy;
        {
            std::lock_guard lock(mutex_);
            if (callCount_ >= maxCalls_) {
                return false;
            }
            slots_copy = slots_;
            ++callCount_;
        }

        try {
            for (const auto& slot : slots_copy) {
                if (slot) {
                    slot(args...);
                }
            }
            return true;
        } catch (const std::exception& e) {
            throw SlotEmissionError(
                std::string("Error during limited slot emission: ") + e.what());
        }
    }

    /**
     * @brief Check if the signal has reached its call limit.
     *
     * @return bool True if the call limit has been reached
     */
    [[nodiscard]] bool isExhausted() const noexcept {
        std::lock_guard lock(mutex_);
        return callCount_ >= maxCalls_;
    }

    /**
     * @brief Get remaining call count before limit is reached.
     *
     * @return size_t Number of remaining emissions
     */
    [[nodiscard]] size_t remainingCalls() const noexcept {
        std::lock_guard lock(mutex_);
        return (callCount_ < maxCalls_) ? (maxCalls_ - callCount_) : 0;
    }

    /**
     * @brief Reset the call counter.
     */
    void reset() noexcept {
        std::lock_guard lock(mutex_);
        callCount_ = 0;
    }

private:
    std::vector<SlotType> slots_;
    const size_t maxCalls_;
    size_t callCount_{0};
    mutable std::mutex mutex_;
};

/**
 * @brief A signal class that uses shared_ptr for scoped slot management.
 * Slots are automatically disconnected when the shared_ptr is released.
 *
 * @tparam Args The argument types for the slots.
 */
template <typename... Args>
class ScopedSignal {
public:
    using SlotType = std::function<void(Args...)>;
    using SlotPtr = std::shared_ptr<SlotType>;

    /**
     * @brief Connect a slot to the signal using a shared pointer.
     *
     * @param slotPtr The shared pointer to the slot to connect.
     * @throws SlotConnectionError if the slot pointer is null or contains an
     * invalid function
     */
    void connect(SlotPtr slotPtr) noexcept(false) {
        if (!slotPtr || !(*slotPtr)) {
            throw SlotConnectionError("Cannot connect null or invalid slot");
        }

        std::lock_guard lock(mutex_);
        slots_.push_back(std::move(slotPtr));
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
     * @brief Emit the signal, calling all connected slots. Invalid (expired)
     * slots are removed during emission.
     *
     * @param args The arguments to pass to the slots.
     * @throws SlotEmissionError if any slot execution fails
     */
    void emit(Args... args) {
        std::vector<SlotPtr> slots_copy;
        {
            std::lock_guard lock(mutex_);
            // Remove expired slots using C++20 erase_if
            std::erase_if(slots_, [](const auto& slot) { return !slot; });
            slots_copy = slots_;
        }

        try {
            for (const auto& slot : slots_copy) {
                // Check again in case a slot became invalid between copy and
                // call
                if (slot && (*slot)) {
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
        // Count valid slots
        return std::count_if(
            slots_.begin(), slots_.end(),
            [](const auto& slot) { return static_cast<bool>(slot); });
    }

private:
    std::vector<SlotPtr> slots_;
    mutable std::mutex mutex_;
};

}  // namespace atom::async

#endif
