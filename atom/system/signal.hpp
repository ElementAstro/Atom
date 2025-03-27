#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <csignal>
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
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
    SignalHandlerWithPriority(const SignalHandler& h, int p, std::string n = "")
        : handler(h), priority(p), name(std::move(n)) {}

    /**
     * @brief Compare two `SignalHandlerWithPriority` objects based on priority.
     *
     * @param other The other `SignalHandlerWithPriority` object to compare
     * against.
     * @return `true` if this object's priority is greater than the other's;
     * `false` otherwise.
     */
    auto operator<(const SignalHandlerWithPriority& other) const -> bool;
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
    static auto getInstance() -> SignalHandlerRegistry&;

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
    int setSignalHandler(SignalID signal, const SignalHandler& handler,
                         int priority = 0, const std::string& handlerName = "");

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
    std::vector<int> setStandardCrashHandlerSignals(
        const SignalHandler& handler, int priority = 0,
        const std::string& handlerName = "");

    /**
     * @brief Process all pending signals synchronously
     *
     * @param timeout Maximum time to spend processing signals (0 means no
     * limit)
     * @return Number of signals processed
     */
    int processAllPendingSignals(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Check if a signal has any registered handlers
     *
     * @param signal The signal ID to check
     * @return true if the signal has registered handlers
     */
    bool hasHandlersForSignal(SignalID signal) const;

    /**
     * @brief Get statistics for a specific signal
     *
     * @param signal The signal to get stats for
     * @return const SignalStats& Reference to the stats for the signal
     */
    const SignalStats& getSignalStats(SignalID signal) const;

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

    static void signalDispatcher(int signal);

    static auto getStandardCrashSignals() -> std::set<SignalID>;

    int nextHandlerId_ = 1;  ///< Counter for generating unique handler IDs
    std::unordered_map<int, std::pair<SignalID, SignalHandler>>
        handlerRegistry_;  ///< Map of handler IDs to signal/handler
    std::map<SignalID, std::set<SignalHandlerWithPriority>>
        handlers_;      ///< Map of signal IDs to handlers with priorities
    std::mutex mutex_;  ///< Mutex for synchronizing access to the handlers
    std::unordered_map<SignalID, SignalStats>
        signalStats_;  ///< Statistics for each signal
    std::chrono::milliseconds handlerTimeout_{
        1000};  ///< Default timeout for handlers (1 second)
};

#ifndef SAFE_SIGNAL_MANAGER_H
#define SAFE_SIGNAL_MANAGER_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <vector>

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
    int addSafeSignalHandler(SignalID signal, const SignalHandler& handler,
                             int priority = 0,
                             const std::string& handlerName = "");

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
    static auto getInstance() -> SafeSignalManager&;

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
    size_t getQueueSize() const;

    /**
     * @brief Get statistics for a specific signal
     *
     * @param signal The signal to get stats for
     * @return const SignalStats& Reference to the stats for the signal
     */
    const SignalStats& getSignalStats(SignalID signal) const;

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
    void processSignals();

    std::atomic<bool> keepRunning_{
        true};  ///< Flag to control the running state
    std::atomic<int> nextHandlerId_{
        1};  ///< Counter for generating unique handler IDs
    std::unordered_map<int, std::pair<SignalID, SignalHandler>>
        handlerRegistry_;  ///< Map of handler IDs to signal/handler
    std::map<SignalID, std::set<SignalHandlerWithPriority>>
        safeHandlers_;             ///< Map of signal IDs to handlers
    std::deque<int> signalQueue_;  ///< Queue of signals to be processed
    size_t maxQueueSize_;          ///< Maximum size of the signal queue
    std::mutex
        queueMutex_;  ///< Mutex for synchronizing access to the signal queue
    std::condition_variable
        queueCondition_;  ///< Condition variable for signaling queue changes
    std::vector<std::jthread>
        workerThreads_;  ///< Threads for processing signals
    std::unordered_map<SignalID, SignalStats>
        signalStats_;  ///< Statistics for each signal
    mutable std::mutex
        statsMutex_;  ///< Mutex for synchronizing access to stats
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

#endif  // SAFE_SIGNAL_MANAGER_H
#endif  // SIGNAL_HANDLER_H
