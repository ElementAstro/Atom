#ifndef ATOM_TYPE_WEAK_PTR_HPP
#define ATOM_TYPE_WEAK_PTR_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <execution>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#include <boost/type_traits.hpp>
#endif

#ifdef __has_include
#if __has_include(<expected>) && __cplusplus >= 202302L
#include <expected>
#define HAS_EXPECTED 1
#elif __has_include("expected.hpp")
#include "expected.hpp"
#define HAS_EXPECTED 1
#else
#define HAS_EXPECTED 0
#endif
#else
#define HAS_EXPECTED 0
#endif

namespace atom::type {

/**
 * @brief Error types for WeakPtr operations
 */
enum class WeakPtrErrorType { Expired, NullReference, Timeout, InvalidCast };

/**
 * @brief Error class for WeakPtr operations
 */
class WeakPtrError {
private:
    WeakPtrErrorType type_;
    std::string message_;

public:
    explicit WeakPtrError(WeakPtrErrorType type, std::string message = "")
        : type_(type), message_(std::move(message)) {}

    [[nodiscard]] WeakPtrErrorType type() const noexcept { return type_; }
    [[nodiscard]] const std::string& message() const noexcept {
        return message_;
    }
};

/**
 * @brief Retry policy for lock operations
 */
class RetryPolicy {
public:
    using Duration = std::chrono::steady_clock::duration;

private:
    size_t maxAttempts_;
    Duration interval_;
    Duration maxDuration_;

public:
    RetryPolicy()
        : maxAttempts_(std::numeric_limits<size_t>::max()),
          interval_(std::chrono::milliseconds(10)),
          maxDuration_(std::chrono::seconds(60)) {}

    RetryPolicy(size_t maxAttempts, Duration interval, Duration maxDuration)
        : maxAttempts_(maxAttempts),
          interval_(interval),
          maxDuration_(maxDuration) {}

    [[nodiscard]] size_t maxAttempts() const noexcept { return maxAttempts_; }
    [[nodiscard]] Duration interval() const noexcept { return interval_; }
    [[nodiscard]] Duration maxDuration() const noexcept { return maxDuration_; }

    /**
     * @brief Builder pattern interface for configuring retry policy
     */
    RetryPolicy& withMaxAttempts(size_t attempts) {
        maxAttempts_ = attempts;
        return *this;
    }

    RetryPolicy& withInterval(Duration interval) {
        interval_ = interval;
        return *this;
    }

    RetryPolicy& withMaxDuration(Duration duration) {
        maxDuration_ = duration;
        return *this;
    }

    /**
     * @brief Factory methods for common retry patterns
     */
    [[nodiscard]] static RetryPolicy none() {
        return RetryPolicy(1, Duration::zero(), Duration::zero());
    }

    [[nodiscard]] static RetryPolicy exponentialBackoff(
        size_t maxAttempts = 5,
        Duration initialInterval = std::chrono::milliseconds(10),
        Duration maxDuration = std::chrono::seconds(60)) {
        return RetryPolicy(maxAttempts, initialInterval, maxDuration);
    }
};

#ifdef ATOM_USE_BOOST
/**
 * @brief Exception class for EnhancedWeakPtr errors when using Boost.
 */
struct EnhancedWeakPtrException : virtual boost::exception,
                                  virtual std::exception {
    WeakPtrErrorType errorType;

    explicit EnhancedWeakPtrException(
        WeakPtrErrorType type = WeakPtrErrorType::Expired)
        : errorType(type) {}

    const char* what() const noexcept override {
        switch (errorType) {
            case WeakPtrErrorType::Expired:
                return "EnhancedWeakPtr expired";
            case WeakPtrErrorType::NullReference:
                return "EnhancedWeakPtr null reference";
            case WeakPtrErrorType::Timeout:
                return "EnhancedWeakPtr operation timeout";
            case WeakPtrErrorType::InvalidCast:
                return "EnhancedWeakPtr invalid cast";
            default:
                return "EnhancedWeakPtr unknown error";
        }
    }
};
#endif

namespace detail {
/**
 * @brief Statistics tracking for weak pointer operations
 */
class WeakPtrStats {
private:
    static inline std::atomic<size_t> totalInstances_{0};
    static inline std::atomic<size_t> totalLockAttempts_{0};
    static inline std::atomic<size_t> totalSuccessfulLocks_{0};
    static inline std::atomic<size_t> totalFailedLocks_{0};

public:
    static void incrementInstances() noexcept { ++totalInstances_; }
    static void decrementInstances() noexcept { --totalInstances_; }
    static void incrementLockAttempts() noexcept { ++totalLockAttempts_; }
    static void incrementSuccessfulLocks() noexcept { ++totalSuccessfulLocks_; }
    static void incrementFailedLocks() noexcept { ++totalFailedLocks_; }

    [[nodiscard]] static size_t getTotalInstances() noexcept {
        return totalInstances_.load();
    }
    [[nodiscard]] static size_t getTotalLockAttempts() noexcept {
        return totalLockAttempts_.load();
    }
    [[nodiscard]] static size_t getTotalSuccessfulLocks() noexcept {
        return totalSuccessfulLocks_.load();
    }
    [[nodiscard]] static size_t getTotalFailedLocks() noexcept {
        return totalFailedLocks_.load();
    }

    static void resetStats() noexcept {
        totalLockAttempts_.store(0);
        totalSuccessfulLocks_.store(0);
        totalFailedLocks_.store(0);
    }
};
}  // namespace detail

/**
 * @class EnhancedWeakPtr
 * @brief A modern, thread-safe wrapper around std::weak_ptr with extended
 * functionality.
 *
 * This class provides additional features beyond std::weak_ptr including:
 * - Thread-safe locking and access
 * - Waiting for object availability with timeouts
 * - Retry policies for lock operations
 * - Functional-style operations (map, filter)
 * - Advanced error handling
 *
 * @tparam T The type of the managed object.
 *
 * @note All operations are thread-safe unless explicitly stated otherwise.
 *
 * Example usage:
 * ```cpp
 * auto shared = std::make_shared<int>(42);
 * atom::type::EnhancedWeakPtr<int> weakPtr(shared);
 *
 * weakPtr.withLock([](int& value) {
 *     value *= 2;
 * });
 *
 * if (auto ptr = weakPtr.lock()) {
 *     std::cout << "Value: " << *ptr << std::endl;
 * }
 * ```
 */
template <typename T>
class EnhancedWeakPtr {
private:
    std::weak_ptr<T> ptr_;
    mutable std::shared_mutex mutex_;
    mutable std::condition_variable_any cv_;
    mutable std::atomic<size_t> lockAttempts_{0};

public:
    /**
     * @brief Default constructor.
     */
    EnhancedWeakPtr() noexcept : ptr_() {
        detail::WeakPtrStats::incrementInstances();
    }

    /**
     * @brief Constructs an EnhancedWeakPtr from a shared pointer.
     * @param shared The shared pointer to manage.
     */
    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    explicit EnhancedWeakPtr(const std::shared_ptr<U>& shared) noexcept
        : ptr_(shared) {
        detail::WeakPtrStats::incrementInstances();
    }

    /**
     * @brief Destructor.
     */
    ~EnhancedWeakPtr() noexcept { detail::WeakPtrStats::decrementInstances(); }

    /**
     * @brief Copy constructor.
     * @param other The other EnhancedWeakPtr to copy from.
     */
    EnhancedWeakPtr(const EnhancedWeakPtr& other) noexcept {
        std::shared_lock lock(other.mutex_);
        ptr_ = other.ptr_;
        detail::WeakPtrStats::incrementInstances();
    }

    /**
     * @brief Move constructor.
     * @param other The other EnhancedWeakPtr to move from.
     */
    EnhancedWeakPtr(EnhancedWeakPtr&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        ptr_ = std::move(other.ptr_);
        detail::WeakPtrStats::incrementInstances();
    }

    /**
     * @brief Copy assignment operator.
     * @param other The other EnhancedWeakPtr to copy from.
     * @return A reference to this EnhancedWeakPtr.
     */
    auto operator=(const EnhancedWeakPtr& other) noexcept -> EnhancedWeakPtr& {
        if (this != &other) {
            std::unique_lock lockThis(mutex_, std::defer_lock);
            std::shared_lock lockOther(other.mutex_, std::defer_lock);
            std::lock(lockThis, lockOther);
            ptr_ = other.ptr_;
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     * @param other The other EnhancedWeakPtr to move from.
     * @return A reference to this EnhancedWeakPtr.
     */
    auto operator=(EnhancedWeakPtr&& other) noexcept -> EnhancedWeakPtr& {
        if (this != &other) {
            std::unique_lock lockThis(mutex_, std::defer_lock);
            std::unique_lock lockOther(other.mutex_, std::defer_lock);
            std::lock(lockThis, lockOther);
            ptr_ = std::move(other.ptr_);
        }
        return *this;
    }

    /**
     * @brief Locks the weak pointer and returns a shared pointer.
     * @return A shared pointer to the managed object, or nullptr if the object
     * has expired.
     */
    [[nodiscard]] auto lock() const -> std::shared_ptr<T> {
        detail::WeakPtrStats::incrementLockAttempts();
        ++lockAttempts_;

        std::shared_lock lock(mutex_);
        auto result = ptr_.lock();

        if (result) {
            detail::WeakPtrStats::incrementSuccessfulLocks();
        } else {
            detail::WeakPtrStats::incrementFailedLocks();
        }

        return result;
    }

    /**
     * @brief Checks if the managed object has expired.
     * @return True if the object has expired, false otherwise.
     */
    [[nodiscard]] auto expired() const noexcept -> bool {
        std::shared_lock lock(mutex_);
        return ptr_.expired();
    }

    /**
     * @brief Resets the weak pointer.
     */
    void reset() noexcept {
        std::unique_lock lock(mutex_);
        ptr_.reset();
    }

#if HAS_EXPECTED
    /**
     * @brief Locks the weak pointer and returns an expected containing the
     * shared pointer or an error.
     * @return An expected containing a shared pointer to the managed object, or
     * an error if the object has expired.
     */
    [[nodiscard]] auto lockExpected() const
        -> std::expected<std::shared_ptr<T>, WeakPtrError> {
        if (auto shared = lock()) {
            return shared;
        } else {
            return std::unexpected(
                WeakPtrError(WeakPtrErrorType::Expired, "Object has expired"));
        }
    }
#endif

#ifdef ATOM_USE_BOOST
    /**
     * @brief Throws an exception if the managed object has expired.
     * @throws EnhancedWeakPtrException if the object has expired.
     */
    void validate() const {
        std::shared_lock lock(mutex_);
        if (ptr_.expired()) {
            throw EnhancedWeakPtrException(WeakPtrErrorType::Expired);
        }
    }

    /**
     * @brief Locks the weak pointer and throws an exception if the object has
     * expired.
     * @return A shared pointer to the managed object.
     * @throws EnhancedWeakPtrException if the object has expired.
     */
    [[nodiscard]] auto lockOrThrow() const -> std::shared_ptr<T> {
        auto result = lock();
        if (!result) {
            throw EnhancedWeakPtrException(WeakPtrErrorType::Expired);
        }
        return result;
    }
#endif

    /**
     * @brief Executes a function with a locked shared pointer.
     *
     * This method safely executes a function with the managed object if it's
     * still alive.
     *
     * @tparam Func The type of the function to execute.
     * @tparam R The return type of the function.
     * @param func The function to execute.
     * @return An optional containing the result of the function, or
     * std::nullopt if the object has expired. Returns true/false for void
     * functions.
     */
    template <typename Func, typename R = std::invoke_result_t<Func, T&>>
    [[nodiscard]] auto withLock(Func&& func) const
        -> std::conditional_t<std::is_void_v<R>, bool, std::optional<R>> {
        if (auto shared = lock()) {
            if constexpr (std::is_void_v<R>) {
                std::forward<Func>(func)(*shared);
                return true;
            } else {
                return std::forward<Func>(func)(*shared);
            }
        }
        if constexpr (std::is_void_v<R>) {
            return false;
        } else {
            return std::nullopt;
        }
    }

    /**
     * @brief Maps the managed object to a new value using a mapping function.
     *
     * @tparam MapFunc The type of the mapping function.
     * @param mapFunc The function to apply to the managed object.
     * @return An optional containing the mapped value, or std::nullopt if the
     * object has expired.
     */
    template <typename MapFunc,
              typename MapResult = std::invoke_result_t<MapFunc, const T&>>
    [[nodiscard]] auto map(MapFunc&& mapFunc) const
        -> std::optional<MapResult> {
        return withLock([&mapFunc](const T& obj) -> MapResult {
            return std::forward<MapFunc>(mapFunc)(obj);
        });
    }

    /**
     * @brief Waits for the managed object to become available or for a timeout.
     *
     * @tparam Rep The representation of the duration.
     * @tparam Period The period of the duration.
     * @param timeout The timeout duration.
     * @return True if the object became available, false if the timeout was
     * reached.
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto waitFor(
        const std::chrono::duration<Rep, Period>& timeout) const -> bool {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout,
                            [this] { return !this->ptr_.expired(); });
    }

    /**
     * @brief Waits until a specific time point for the managed object to become
     * available.
     *
     * @tparam Clock The clock type.
     * @tparam Duration The duration type.
     * @param timePoint The time point to wait until.
     * @return True if the object became available, false if the timeout was
     * reached.
     */
    template <typename Clock, typename Duration>
    [[nodiscard]] auto waitUntil(
        const std::chrono::time_point<Clock, Duration>& timePoint) const
        -> bool {
        std::unique_lock lock(mutex_);
        return cv_.wait_until(lock, timePoint,
                              [this] { return !this->ptr_.expired(); });
    }

    /**
     * @brief Equality operator.
     * @param other The other EnhancedWeakPtr to compare with.
     * @return True if the weak pointers are equal, false otherwise.
     */
    [[nodiscard]] auto operator==(const EnhancedWeakPtr& other) const noexcept
        -> bool {
        std::shared_lock lockThis(mutex_, std::defer_lock);
        std::shared_lock lockOther(other.mutex_, std::defer_lock);
        std::lock(lockThis, lockOther);
        return !ptr_.owner_before(other.ptr_) && !other.ptr_.owner_before(ptr_);
    }

    /**
     * @brief Inequality operator.
     * @param other The other EnhancedWeakPtr to compare with.
     * @return True if the weak pointers are not equal, false otherwise.
     */
    [[nodiscard]] auto operator!=(const EnhancedWeakPtr& other) const noexcept
        -> bool {
        return !(*this == other);
    }

    /**
     * @brief Gets the use count of the managed object.
     * @return The use count of the managed object.
     */
    [[nodiscard]] auto useCount() const noexcept -> long {
        std::shared_lock lock(mutex_);
        return ptr_.use_count();
    }

    /**
     * @brief Gets the total number of EnhancedWeakPtr instances.
     * @return The total number of EnhancedWeakPtr instances.
     */
    [[nodiscard]] static auto getTotalInstances() noexcept -> size_t {
        return detail::WeakPtrStats::getTotalInstances();
    }

    /**
     * @brief Gets the total number of successful locks across all instances.
     * @return The total number of successful locks.
     */
    [[nodiscard]] static auto getTotalSuccessfulLocks() noexcept -> size_t {
        return detail::WeakPtrStats::getTotalSuccessfulLocks();
    }

    /**
     * @brief Gets the total number of failed locks across all instances.
     * @return The total number of failed locks.
     */
    [[nodiscard]] static auto getTotalFailedLocks() noexcept -> size_t {
        return detail::WeakPtrStats::getTotalFailedLocks();
    }

    /**
     * @brief Resets all statistical counters.
     */
    static void resetStats() noexcept { detail::WeakPtrStats::resetStats(); }

    /**
     * @brief Tries to lock the weak pointer and executes one of two functions
     * based on success or failure.
     *
     * @tparam SuccessFunc The type of the success function.
     * @tparam FailureFunc The type of the failure function.
     * @param success The function to execute on success.
     * @param failure The function to execute on failure.
     * @return The result of either the success or failure function.
     */
    template <typename SuccessFunc, typename FailureFunc,
              typename SuccessResult = std::invoke_result_t<SuccessFunc, T&>,
              typename FailureResult = std::invoke_result_t<FailureFunc>>
    [[nodiscard]] auto tryLockOrElse(SuccessFunc&& success,
                                     FailureFunc&& failure) const
        -> std::common_type_t<SuccessResult, FailureResult> {
        if (auto shared = lock()) {
            return std::forward<SuccessFunc>(success)(*shared);
        }
        return std::forward<FailureFunc>(failure)();
    }

    /**
     * @brief Tries to lock the weak pointer with retry policy.
     *
     * @param policy The retry policy to use.
     * @return A shared pointer to the managed object, or nullptr if all retry
     * attempts failed.
     */
    [[nodiscard]] auto tryLockWithRetry(
        const RetryPolicy& policy = RetryPolicy()) const -> std::shared_ptr<T> {
        using Clock = std::chrono::steady_clock;
        const auto startTime = Clock::now();
        const auto deadline = startTime + policy.maxDuration();

        for (size_t attempt = 0; attempt < policy.maxAttempts(); ++attempt) {
            if (auto shared = lock()) {
                return shared;
            }

            if (Clock::now() >= deadline) {
                break;
            }

            auto sleepTime = policy.interval();
            if (attempt > 0) {
                if (sleepTime == RetryPolicy::Duration::zero())
                    sleepTime = std::chrono::milliseconds(1);
                sleepTime *= static_cast<long long>(
                    1 << std::min(attempt, static_cast<size_t>(10)));
            }

            if (sleepTime > RetryPolicy::Duration::zero()) {
                std::this_thread::sleep_for(sleepTime);
            } else if (attempt == 0 && policy.maxAttempts() > 1) {
                std::this_thread::yield();
            }
        }

        return nullptr;
    }

    /**
     * @brief Gets the underlying weak pointer.
     * @return The underlying weak pointer.
     */
    [[nodiscard]] auto getWeakPtr() const noexcept -> std::weak_ptr<T> {
        std::shared_lock lock(mutex_);
        return ptr_;
    }

    /**
     * @brief Notifies all waiting threads.
     */
    void notifyAll() const noexcept { cv_.notify_all(); }

    /**
     * @brief Gets the number of lock attempts for this instance.
     * @return The number of lock attempts.
     */
    [[nodiscard]] auto getLockAttempts() const noexcept -> size_t {
        return lockAttempts_.load();
    }

    /**
     * @brief Asynchronously locks the weak pointer.
     *
     * @param policy Optional retry policy to use.
     * @return A future that resolves to a shared pointer to the managed object.
     */
    [[nodiscard]] auto asyncLock(
        const std::optional<RetryPolicy>& policy = std::nullopt) const
        -> std::future<std::shared_ptr<T>> {
        if (policy) {
            return std::async(std::launch::async, [this, p = *policy]() {
                return this->tryLockWithRetry(p);
            });
        } else {
            return std::async(std::launch::async,
                              [this]() { return this->lock(); });
        }
    }

    /**
     * @brief Waits until a predicate is satisfied or the managed object
     * expires.
     *
     * @tparam Predicate The type of the predicate.
     * @param pred The predicate to satisfy.
     * @return True if the predicate was satisfied, false if the object expired.
     */
    template <typename Predicate>
    [[nodiscard]] auto waitUntil(Predicate pred) const -> bool {
        std::unique_lock lock(mutex_);
        return cv_.wait(lock, [this, &pred]() {
            return this->ptr_.expired() || pred();
        }) && !this->ptr_.expired();
    }

    /**
     * @brief Safely casts the weak pointer to a different type.
     *
     * @tparam U The type to cast to.
     * @return An EnhancedWeakPtr of the new type.
     */
    template <typename U>
    [[nodiscard]] auto dynamicCast() const -> EnhancedWeakPtr<U> {
        if (auto sharedPtr = lock()) {
            if (auto castedPtr = std::dynamic_pointer_cast<U>(sharedPtr)) {
                return EnhancedWeakPtr<U>(castedPtr);
            }
        }
        return EnhancedWeakPtr<U>();
    }

    /**
     * @brief Safely casts the weak pointer to a different type.
     *
     * @tparam U The type to cast to.
     * @return An EnhancedWeakPtr of the new type.
     */
    template <typename U>
    [[nodiscard]] auto staticCast() const -> EnhancedWeakPtr<U> {
        if (auto sharedPtr = lock()) {
            return EnhancedWeakPtr<U>(std::static_pointer_cast<U>(sharedPtr));
        }
        return EnhancedWeakPtr<U>();
    }

    /**
     * @brief Determines if the managed object is of or derives from a specified
     * type.
     *
     * @tparam U The type to check against.
     * @return True if the object is of the specified type, false otherwise.
     */
    template <typename U>
    [[nodiscard]] auto isType() const -> bool {
        if (auto shared = lock()) {
            return dynamic_cast<U*>(shared.get()) != nullptr;
        }
        return false;
    }

    /**
     * @brief Filters the managed object based on a predicate.
     *
     * @tparam Predicate The type of the predicate function.
     * @param predicate The function to test the managed object.
     * @return This EnhancedWeakPtr if the predicate returns true, otherwise an
     * empty one.
     */
    template <typename Predicate>
    [[nodiscard]] auto filter(Predicate&& predicate) const -> EnhancedWeakPtr {
        auto meets = withLock([&predicate](const T& obj) {
            return std::forward<Predicate>(predicate)(obj);
        });

        if (meets.value_or(false)) {
            return *this;
        }
        return EnhancedWeakPtr<T>();
    }
};

/**
 * @brief Creates a group of EnhancedWeakPtr from a span of shared pointers.
 *
 * @tparam T The type of the managed objects.
 * @param sharedPtrs The span of shared pointers.
 * @return A vector of EnhancedWeakPtr.
 */
template <typename T>
[[nodiscard]] auto createWeakPtrGroup(
    std::span<const std::shared_ptr<T>> sharedPtrs)
    -> std::vector<EnhancedWeakPtr<T>> {
    std::vector<EnhancedWeakPtr<T>> weakPtrs;
    weakPtrs.reserve(sharedPtrs.size());

    std::transform(
        sharedPtrs.begin(), sharedPtrs.end(), std::back_inserter(weakPtrs),
        [](const auto& sharedPtr) { return EnhancedWeakPtr<T>(sharedPtr); });

    return weakPtrs;
}

/**
 * @brief Performs a batch operation on a span of EnhancedWeakPtr.
 *
 * @tparam T The type of the managed objects.
 * @tparam Func The type of the function to execute.
 * @param weakPtrs The span of EnhancedWeakPtr.
 * @param func The function to execute.
 * @param parallelThreshold Optional threshold for parallel execution.
 * @return The number of successfully processed objects.
 */
template <typename T, typename Func>
[[nodiscard]] auto batchOperation(std::span<const EnhancedWeakPtr<T>> weakPtrs,
                                  Func&& func, size_t parallelThreshold = 100)
    -> size_t {
    size_t successCount = 0;

#ifdef __cpp_lib_parallel_algorithm
    if (parallelThreshold > 0 && weakPtrs.size() >= parallelThreshold) {
        std::atomic<size_t> atomicCount{0};

        std::for_each(std::execution::par_unseq, weakPtrs.begin(),
                      weakPtrs.end(),
                      [&func, &atomicCount](const auto& weakPtr) {
                          bool success = weakPtr.withLock(func);
                          if (success) {
                              ++atomicCount;
                          }
                      });

        successCount = atomicCount.load();
    } else
#endif
    {
        for (const auto& weakPtr : weakPtrs) {
            bool success = weakPtr.withLock(std::forward<Func>(func));
            if (success) {
                ++successCount;
            }
        }
    }

    return successCount;
}

/**
 * @brief Filters a collection of EnhancedWeakPtr based on a predicate.
 *
 * @tparam T The type of the managed objects.
 * @tparam Predicate The type of the predicate function.
 * @param weakPtrs The span of EnhancedWeakPtr to filter.
 * @param predicate The predicate function to apply.
 * @return A vector of EnhancedWeakPtr that satisfy the predicate.
 */
template <typename T, typename Predicate>
[[nodiscard]] auto filterWeakPtrs(std::span<const EnhancedWeakPtr<T>> weakPtrs,
                                  Predicate&& predicate)
    -> std::vector<EnhancedWeakPtr<T>> {
    std::vector<EnhancedWeakPtr<T>> result;
    result.reserve(weakPtrs.size());

    for (const auto& weakPtr : weakPtrs) {
        auto meets = weakPtr.withLock([&predicate](const T& obj) {
            return std::forward<Predicate>(predicate)(obj);
        });

        if (meets.value_or(false)) {
            result.push_back(weakPtr);
        }
    }

    return result;
}

}  // namespace atom::type

#endif  // ATOM_TYPE_WEAK_PTR_HPP