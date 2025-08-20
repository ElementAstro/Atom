#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <csignal>
#endif

#ifdef ATOM_USE_BOOST
#include <boost/container/small_vector.hpp>
#include <boost/lockfree/queue.hpp>
#endif

/**
 * @brief Type alias for signal identifiers.
 */
using SignalID = int;

/**
 * @brief Type alias for signal handler functions.
 */
using SignalHandler = std::function<void(SignalID)>;

/**
 * @brief Structure to associate a signal handler with a priority.
 *
 * Handlers with higher priority values will be executed first.
 */
struct SignalHandlerWithPriority {
    SignalHandler handler;  ///< The signal handler function.
    int priority;           ///< The priority of the handler.
    std::string name;       ///< Optional name for the handler to aid debugging

    /**
     * @brief Construct a new Signal Handler With Priority object
     *
     * @param h The signal handler function
     * @param p The priority value
     * @param n Optional name for the handler
     */
    SignalHandlerWithPriority(const SignalHandler& h, int p,
                              std::string_view n = "")
        : handler(h), priority(p), name(n) {}

    /**
     * @brief Move constructor for efficient resource management
     */
    SignalHandlerWithPriority(SignalHandlerWithPriority&& other) noexcept
        : handler(std::move(other.handler)),
          priority(other.priority),
          name(std::move(other.name)) {}

    /**
     * @brief Compare two `SignalHandlerWithPriority` objects based on priority.
     *
     * @param other The other `SignalHandlerWithPriority` object to compare
     * against.
     * @return `true` if this object's priority is greater than the other's;
     * `false` otherwise.
     */
    [[nodiscard]] auto operator<(
        const SignalHandlerWithPriority& other) const noexcept -> bool;
};

/**
 * @brief Structure to store signal statistics
 */
struct SignalStats {
    std::atomic<uint64_t> received{0};   ///< Total number of signals received
    std::atomic<uint64_t> processed{0};  ///< Total number of signals processed
    std::atomic<uint64_t> dropped{0};    ///< Total number of signals dropped
    std::atomic<uint64_t> handlerErrors{0};  ///< Total number of handler errors
    std::chrono::steady_clock::time_point
        lastReceived;  ///< Timestamp of last received signal
    std::chrono::steady_clock::time_point
        lastProcessed;  ///< Timestamp of last processed signal

    SignalStats() = default;

    SignalStats(const SignalStats& other)
        : received(other.received.load(std::memory_order_relaxed)),
          processed(other.processed.load(std::memory_order_relaxed)),
          dropped(other.dropped.load(std::memory_order_relaxed)),
          handlerErrors(other.handlerErrors.load(std::memory_order_relaxed)),
          lastReceived(other.lastReceived),
          lastProcessed(other.lastProcessed) {}

    SignalStats& operator=(const SignalStats& other) {
        received.store(other.received.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);
        processed.store(other.processed.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
        dropped.store(other.dropped.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
        handlerErrors.store(other.handlerErrors.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
        lastReceived = other.lastReceived;
        lastProcessed = other.lastProcessed;
        return *this;
    }
};

/**
 * @brief Singleton class to manage signal handlers and dispatch signals.
 *
 * This class handles registering and dispatching signal handlers with
 * priorities. It also provides a mechanism to set up default crash signal
 * handlers.
 */
class SignalHandlerRegistry {
public:
    /**
     * @brief Get the singleton instance of the `SignalHandlerRegistry`.
     *
     * @return A reference to the singleton `SignalHandlerRegistry` instance.
     */
    [[nodiscard]] static auto getInstance() -> SignalHandlerRegistry&;

    /**
     * @brief Set a signal handler for a specific signal with an optional
     * priority.
     *
     * @param signal The signal ID to handle.
     * @param handler The handler function to execute.
     * @param priority The priority of the handler. Default is 0.
     * @param handlerName Optional name for the handler for debugging purposes.
     * @return A unique identifier for this handler registration.
     */
    [[nodiscard]] int setSignalHandler(SignalID signal,
                                       const SignalHandler& handler,
                                       int priority = 0,
                                       std::string_view handlerName = "");

    /**
     * @brief Remove a specific signal handler by its identifier.
     *
     * @param handlerId The identifier returned by setSignalHandler
     * @return true if handler was successfully removed, false otherwise
     */
    bool removeSignalHandlerById(int handlerId);

    /**
     * @brief Remove a specific signal handler for a signal.
     *
     * @param signal The signal ID to stop handling.
     * @param handler The handler function to remove.
     * @return true if handler was successfully removed, false otherwise
     */
    bool removeSignalHandler(SignalID signal, const SignalHandler& handler);

    /**
     * @brief Set handlers for standard crash signals.
     *
     * @param handler The handler function to execute for crash signals.
     * @param priority The priority of the handler. Default is 0.
     * @param handlerName Optional name for the handler for debugging purposes.
     * @return Vector of handler IDs created for each signal
     */
    [[nodiscard]] std::vector<int> setStandardCrashHandlerSignals(
        const SignalHandler& handler, int priority = 0,
        std::string_view handlerName = "");

    /**
     * @brief Process all pending signals synchronously
     *
     * @param timeout Maximum time to spend processing signals (0 means no
     * limit)
     * @return Number of signals processed
     */
    [[nodiscard]] int processAllPendingSignals(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Check if a signal has any registered handlers
     *
     * @param signal The signal ID to check
     * @return true if the signal has registered handlers
     */
    [[nodiscard]] bool hasHandlersForSignal(SignalID signal);

    /**
     * @brief Get statistics for a specific signal
     *
     * @param signal The signal to get stats for
     * @return const SignalStats& Reference to the stats for the signal
     */
    [[nodiscard]] const SignalStats& getSignalStats(SignalID signal) const;

    /**
     * @brief Reset statistics for all or a specific signal
     *
     * @param signal The signal to reset (default -1 means all signals)
     */
    void resetStats(SignalID signal = -1);

    /**
     * @brief Set the timeout for signal handlers
     *
     * @param timeout Maximum time a handler can run before being considered
     * hanging
     */
    void setHandlerTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Execute a handler with timeout protection
     *
     * @param handler The handler to execute
     * @param signal The signal to pass to the handler
     * @return true if handler completed successfully, false if it timed out
     */
    bool executeHandlerWithTimeout(const SignalHandler& handler,
                                   SignalID signal);

private:
    SignalHandlerRegistry();
    ~SignalHandlerRegistry();

    SignalHandlerRegistry(const SignalHandlerRegistry&) = delete;
    SignalHandlerRegistry& operator=(const SignalHandlerRegistry&) = delete;
    SignalHandlerRegistry(SignalHandlerRegistry&&) = delete;
    SignalHandlerRegistry& operator=(SignalHandlerRegistry&&) = delete;

    static void signalDispatcher(int signal);
    [[nodiscard]] static auto getStandardCrashSignals() -> std::set<SignalID>;

    std::atomic<int> nextHandlerId_{1};

#ifdef ATOM_USE_PMR
    std::pmr::monotonic_buffer_resource memResource_{1024 * 1024};
    std::pmr::unordered_map<int, std::pair<SignalID, SignalHandler>>
        handlerRegistry_{&memResource_};
#else
    std::unordered_map<int, std::pair<SignalID, SignalHandler>>
        handlerRegistry_;
#endif

    mutable std::shared_mutex mutex_;
    std::map<SignalID, std::set<SignalHandlerWithPriority>> handlers_;
    std::unordered_map<SignalID, SignalStats> signalStats_;
    std::chrono::milliseconds handlerTimeout_{1000};
};

/**
 * @brief Class to safely manage and dispatch signals with separate thread
 * handling.
 *
 * This class allows adding and removing signal handlers and dispatching signals
 * in a separate thread to ensure thread safety and avoid blocking signal
 * handling.
 */
class SafeSignalManager {
public:
    /**
     * @brief Constructs a `SafeSignalManager` and starts the signal processing
     * thread.
     *
     * @param threadCount Number of worker threads to handle signals (default:
     * 1)
     * @param queueSize Maximum size of the signal queue (default: 1000)
     */
    explicit SafeSignalManager(size_t threadCount = 1, size_t queueSize = 1000);

    /**
     * @brief Destructs the `SafeSignalManager` and stops the signal processing
     * thread.
     */
    ~SafeSignalManager();

    SafeSignalManager(const SafeSignalManager&) = delete;
    SafeSignalManager& operator=(const SafeSignalManager&) = delete;
    SafeSignalManager(SafeSignalManager&&) = delete;
    SafeSignalManager& operator=(SafeSignalManager&&) = delete;

    /**
     * @brief Add a signal handler for a specific signal with an optional
     * priority.
     *
     * @param signal The signal ID to handle.
     * @param handler The handler function to execute.
     * @param priority The priority of the handler. Default is 0.
     * @param handlerName Optional name for the handler for debugging purposes.
     * @return A unique identifier for this handler registration.
     */
    [[nodiscard]] int addSafeSignalHandler(SignalID signal,
                                           const SignalHandler& handler,
                                           int priority = 0,
                                           std::string_view handlerName = "");

    /**
     * @brief Remove a specific signal handler by its identifier.
     *
     * @param handlerId The identifier returned by addSafeSignalHandler
     * @return true if handler was successfully removed, false otherwise
     */
    bool removeSafeSignalHandlerById(int handlerId);

    /**
     * @brief Remove a specific signal handler for a signal.
     *
     * @param signal The signal ID to stop handling.
     * @param handler The handler function to remove.
     * @return true if handler was successfully removed, false otherwise
     */
    bool removeSafeSignalHandler(SignalID signal, const SignalHandler& handler);

    /**
     * @brief Clear the signal queue
     *
     * @return Number of signals cleared from the queue
     */
    int clearSignalQueue();

    /**
     * @brief Static method to safely dispatch signals to the manager.
     *
     * This method is called by the system signal handler to queue signals for
     * processing.
     *
     * @param signal The signal ID to queue.
     */
    static void safeSignalDispatcher(int signal);

    /**
     * @brief Get the singleton instance of `SafeSignalManager`.
     *
     * @return A reference to the singleton `SafeSignalManager` instance.
     */
    [[nodiscard]] static auto getInstance() -> SafeSignalManager&;

    /**
     * @brief Manually queue a signal for processing
     *
     * @param signal The signal to queue
     * @return true if signal was queued, false if queue is full
     */
    bool queueSignal(SignalID signal);

    /**
     * @brief Get current queue size
     *
     * @return Current number of signals in the queue
     */
    [[nodiscard]] size_t getQueueSize() const;

    /**
     * @brief Get statistics for a specific signal
     *
     * @param signal The signal to get stats for
     * @return const SignalStats& Reference to the stats for the signal
     */
    [[nodiscard]] const SignalStats& getSignalStats(SignalID signal) const;

    /**
     * @brief Reset statistics for all or a specific signal
     *
     * @param signal The signal to reset (default -1 means all signals)
     */
    void resetStats(SignalID signal = -1);

    /**
     * @brief Configure the number of worker threads
     *
     * @param threadCount New number of worker threads
     * @return true if change was successful, false otherwise
     */
    bool setWorkerThreadCount(size_t threadCount);

    /**
     * @brief Set the maximum queue size
     *
     * @param size New maximum queue size
     */
    void setMaxQueueSize(size_t size);

private:
    /**
     * @brief Process signals from the queue in a separate thread.
     */
    void processSignals(std::stop_token stopToken);

    std::atomic<bool> keepRunning_{true};
    std::atomic<int> nextHandlerId_{1};

#ifdef ATOM_USE_PMR
    std::pmr::monotonic_buffer_resource handlerMemResource_{1024 * 1024};
    std::pmr::unordered_map<int, std::pair<SignalID, SignalHandler>>
        handlerRegistry_{&handlerMemResource_};
#else
    std::unordered_map<int, std::pair<SignalID, SignalHandler>>
        handlerRegistry_;
#endif

    std::map<SignalID, std::set<SignalHandlerWithPriority>> safeHandlers_;

#ifdef ATOM_USE_BOOST
    boost::lockfree::queue<int> signalQueue_{1024};
    size_t maxQueueSize_{1024};
#else
    std::deque<int> signalQueue_;
    size_t maxQueueSize_;
    mutable std::shared_mutex queueMutex_;
    std::condition_variable_any queueCondition_;
#endif

    std::vector<std::jthread> workerThreads_;
    std::unordered_map<SignalID, SignalStats> signalStats_;
    mutable std::shared_mutex statsMutex_;
};

/**
 * @brief Register signal handlers for platform-specific signals
 */
void installPlatformSpecificHandlers();

/**
 * @brief Initialize the signal handling system with reasonable defaults
 *
 * @param workerThreadCount Number of worker threads for SafeSignalManager
 * @param queueSize Size of the signal queue
 */
void initializeSignalSystem(size_t workerThreadCount = 1,
                            size_t queueSize = 1000);

#endif  // SIGNAL_HANDLER_H
