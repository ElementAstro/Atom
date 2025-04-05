#ifndef ATOM_SYSTEM_SIGNAL_MONITOR_HPP
#define ATOM_SYSTEM_SIGNAL_MONITOR_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "signal.hpp"

/**
 * @brief Callback for signal monitoring events
 */
using SignalMonitorCallback = std::function<void(SignalID, const SignalStats&)>;

/**
 * @brief Class to monitor signal activity and collect statistics
 */
class SignalMonitor {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return SignalMonitor& Reference to the singleton instance
     */
    static SignalMonitor& getInstance() {
        static SignalMonitor instance;
        return instance;
    }

    /**
     * @brief Start monitoring signals
     *
     * @param monitorInterval How often to check signal statistics (in
     * milliseconds)
     * @param signalsToMonitor List of signals to monitor (empty = all signals)
     */
    void start(std::chrono::milliseconds monitorInterval =
                   std::chrono::milliseconds(1000),
               const std::vector<SignalID>& signalsToMonitor = {}) {
        std::lock_guard lock(mutex_);

        if (isRunning_) {
            return;
        }

        monitorInterval_ = monitorInterval;
        signalsToMonitor_ = signalsToMonitor;
        isRunning_ = true;

        // Start the monitoring thread
        monitorThread_ = std::jthread([this](std::stop_token stopToken) {
            this->monitorLoop(stopToken);
        });
    }

    /**
     * @brief Stop monitoring signals
     */
    void stop() {
        std::lock_guard lock(mutex_);

        if (!isRunning_) {
            return;
        }

        isRunning_ = false;
        if (monitorThread_.joinable()) {
            monitorThread_.request_stop();
            monitorThread_.join();
        }
    }

    /**
     * @brief Add a callback for when a signal exceeds a threshold
     *
     * @param signal The signal to monitor
     * @param receivedThreshold Callback triggered when received count exceeds
     * this value
     * @param errorThreshold Callback triggered when error count exceeds this
     * value
     * @param callback The callback function to execute
     * @return int ID of the registered callback
     */
    int addThresholdCallback(SignalID signal, uint64_t receivedThreshold,
                             uint64_t errorThreshold,
                             const SignalMonitorCallback& callback) {
        std::lock_guard lock(mutex_);

        int callbackId = nextCallbackId_++;
        thresholdCallbacks_[callbackId] = {
            signal, receivedThreshold, errorThreshold, callback, {}, {}};

        // Add this signal to monitored signals if not already there
        if (!signalsToMonitor_.empty() &&
            std::find(signalsToMonitor_.begin(), signalsToMonitor_.end(),
                      signal) == signalsToMonitor_.end()) {
            signalsToMonitor_.push_back(signal);
        }

        return callbackId;
    }

    /**
     * @brief Add a callback for when a signal has been inactive for a period
     *
     * @param signal The signal to monitor
     * @param inactivityPeriod Time without activity to trigger callback (in
     * milliseconds)
     * @param callback The callback function to execute
     * @return int ID of the registered callback
     */
    int addInactivityCallback(SignalID signal,
                              std::chrono::milliseconds inactivityPeriod,
                              const SignalMonitorCallback& callback) {
        std::lock_guard lock(mutex_);

        int callbackId = nextCallbackId_++;
        inactivityCallbacks_[callbackId] = {signal, inactivityPeriod, callback,
                                            std::chrono::steady_clock::now()};

        // Add this signal to monitored signals if not already there
        if (!signalsToMonitor_.empty() &&
            std::find(signalsToMonitor_.begin(), signalsToMonitor_.end(),
                      signal) == signalsToMonitor_.end()) {
            signalsToMonitor_.push_back(signal);
        }

        return callbackId;
    }

    /**
     * @brief Remove a callback by ID
     *
     * @param callbackId ID of the callback to remove
     * @return true if callback was successfully removed
     */
    bool removeCallback(int callbackId) {
        std::lock_guard lock(mutex_);

        if (thresholdCallbacks_.erase(callbackId) > 0) {
            return true;
        }

        if (inactivityCallbacks_.erase(callbackId) > 0) {
            return true;
        }

        return false;
    }

    /**
     * @brief Get a snapshot of signal statistics
     *
     * @return std::map<SignalID, SignalStats> Map of signal IDs to their
     * statistics
     */
    std::map<SignalID, SignalStats> getStatSnapshot() const {
        std::map<SignalID, SignalStats> stats;

        // Get stats from registry
        for (const auto& signal : getMonitoredSignals()) {
            // Use insertion instead of assignment since SignalStats contains
            // atomic members
            stats.insert_or_assign(
                signal,
                SignalHandlerRegistry::getInstance().getSignalStats(signal));
        }

        // Merge with stats from safe manager
        for (const auto& signal : getMonitoredSignals()) {
            const SignalStats& safeStats =
                SafeSignalManager::getInstance().getSignalStats(signal);

            // Merge the stats - using load() to get values from atomics
            stats[signal].received += safeStats.received.load();
            stats[signal].processed += safeStats.processed.load();
            stats[signal].dropped += safeStats.dropped.load();
            stats[signal].handlerErrors += safeStats.handlerErrors.load();

            // Take the most recent timestamps
            if (safeStats.lastReceived > stats[signal].lastReceived) {
                stats[signal].lastReceived = safeStats.lastReceived;
            }

            if (safeStats.lastProcessed > stats[signal].lastProcessed) {
                stats[signal].lastProcessed = safeStats.lastProcessed;
            }
        }

        return stats;
    }

    /**
     * @brief Get a list of all monitored signals
     *
     * @return std::vector<SignalID> List of monitored signal IDs
     */
    std::vector<SignalID> getMonitoredSignals() const {
        std::lock_guard lock(mutex_);

        if (signalsToMonitor_.empty()) {
            // If no specific signals are specified, get all registered signals
            std::vector<SignalID> allSignals;

            // Get signals from registry
            const auto& registryStats = SignalHandlerRegistry::getInstance();
            for (int i = 1; i < 32; ++i) {  // Common signal range
                if (registryStats.hasHandlersForSignal(i)) {
                    allSignals.push_back(i);
                }
            }

            return allSignals;
        }

        return signalsToMonitor_;
    }

    /**
     * @brief Reset all monitoring statistics
     */
    void resetAllStats() {
        SignalHandlerRegistry::getInstance().resetStats();
        SafeSignalManager::getInstance().resetStats();

        // Reset callback state
        std::lock_guard lock(mutex_);
        for (auto& [id, callback] : thresholdCallbacks_) {
            callback.lastReceivedCount = 0;
            callback.lastErrorCount = 0;
        }

        for (auto& [id, callback] : inactivityCallbacks_) {
            callback.lastActivity = std::chrono::steady_clock::now();
        }
    }

private:
    // Constructor
    SignalMonitor() : isRunning_(false), nextCallbackId_(1) {}

    // Destructor
    ~SignalMonitor() { stop(); }

    // Prevent copying
    SignalMonitor(const SignalMonitor&) = delete;
    SignalMonitor& operator=(const SignalMonitor&) = delete;

    // Threshold callback information
    struct ThresholdCallback {
        SignalID signal;
        uint64_t receivedThreshold;
        uint64_t errorThreshold;
        SignalMonitorCallback callback;
        uint64_t lastReceivedCount;
        uint64_t lastErrorCount;
    };

    // Inactivity callback information
    struct InactivityCallback {
        SignalID signal;
        std::chrono::milliseconds inactivityPeriod;
        SignalMonitorCallback callback;
        std::chrono::steady_clock::time_point lastActivity;
    };

    // Monitor loop
    void monitorLoop(std::stop_token stopToken) {
        while (!stopToken.stop_requested() && isRunning_) {
            // Check threshold callbacks
            checkThresholds();

            // Check inactivity callbacks
            checkInactivity();

            // Sleep until next check
            std::this_thread::sleep_for(monitorInterval_);
        }
    }

    // Check threshold callbacks
    void checkThresholds() {
        auto stats = getStatSnapshot();

        std::lock_guard lock(mutex_);
        for (auto& [id, callback] : thresholdCallbacks_) {
            const auto& signalStats = stats[callback.signal];

            // Check received threshold
            if (callback.receivedThreshold > 0 &&
                signalStats.received >
                    callback.lastReceivedCount + callback.receivedThreshold) {
                callback.callback(callback.signal, signalStats);
                callback.lastReceivedCount = signalStats.received;
            }

            // Check error threshold
            if (callback.errorThreshold > 0 &&
                signalStats.handlerErrors >
                    callback.lastErrorCount + callback.errorThreshold) {
                callback.callback(callback.signal, signalStats);
                callback.lastErrorCount = signalStats.handlerErrors;
            }
        }
    }

    // Check inactivity callbacks
    void checkInactivity() {
        auto stats = getStatSnapshot();
        auto now = std::chrono::steady_clock::now();

        std::lock_guard lock(mutex_);
        for (auto& [id, callback] : inactivityCallbacks_) {
            const auto& signalStats = stats[callback.signal];

            // Update last activity time if we have new activity
            if (signalStats.lastReceived > callback.lastActivity) {
                callback.lastActivity = signalStats.lastReceived;
                continue;
            }

            // Check if inactive for too long
            auto elapsed = now - callback.lastActivity;
            if (elapsed > callback.inactivityPeriod) {
                callback.callback(callback.signal, signalStats);
                callback.lastActivity =
                    now;  // Reset to avoid repeated callbacks
            }
        }
    }

    std::atomic<bool> isRunning_;
    std::chrono::milliseconds monitorInterval_;
    std::vector<SignalID> signalsToMonitor_;
    std::jthread monitorThread_;
    mutable std::mutex mutex_;
    int nextCallbackId_;
    std::map<int, ThresholdCallback> thresholdCallbacks_;
    std::map<int, InactivityCallback> inactivityCallbacks_;
};

#endif  // ATOM_SYSTEM_SIGNAL_MONITOR_HPP
