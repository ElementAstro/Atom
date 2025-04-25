#ifndef ATOM_SYSTEM_SIGNAL_MONITOR_HPP
#define ATOM_SYSTEM_SIGNAL_MONITOR_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "signal.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/container/flat_map.hpp>
#endif

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
    [[nodiscard]] static SignalMonitor& getInstance() noexcept {
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
               std::span<const SignalID> signalsToMonitor = {}) {
        std::unique_lock lock(mutex_);

        if (isRunning_) {
            return;
        }

        monitorInterval_ = monitorInterval;
        signalsToMonitor_.assign(signalsToMonitor.begin(),
                                 signalsToMonitor.end());
        isRunning_ = true;

        // C++20 jthread会在析构时自动join
        monitorThread_ = std::jthread([this](std::stop_token stopToken) {
            this->monitorLoop(stopToken);
        });
    }

    // 兼容C++17使用向量形式的参数
    void start(std::chrono::milliseconds monitorInterval,
               const std::vector<SignalID>& signalsToMonitor) {
        start(monitorInterval, std::span<const SignalID>(signalsToMonitor));
    }

    /**
     * @brief Stop monitoring signals
     */
    void stop() noexcept {
        std::unique_lock lock(mutex_);

        if (!isRunning_) {
            return;
        }

        isRunning_ = false;
        if (monitorThread_.joinable()) {
            monitorThread_.request_stop();
        }
        // jthread会在析构时自动join，不需要显式调用
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
    [[nodiscard]] int addThresholdCallback(
        SignalID signal, uint64_t receivedThreshold, uint64_t errorThreshold,
        const SignalMonitorCallback& callback) {
        std::unique_lock lock(mutex_);

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
    [[nodiscard]] int addInactivityCallback(
        SignalID signal, std::chrono::milliseconds inactivityPeriod,
        const SignalMonitorCallback& callback) {
        std::unique_lock lock(mutex_);

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
    bool removeCallback(int callbackId) noexcept {
        std::unique_lock lock(mutex_);

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
    [[nodiscard]] std::map<SignalID, SignalStats> getStatSnapshot() const {
#ifdef ATOM_USE_BOOST
        boost::container::flat_map<SignalID, SignalStats> stats;
#else
        std::map<SignalID, SignalStats> stats;
#endif

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
            stats[signal].received +=
                safeStats.received.load(std::memory_order_acquire);
            stats[signal].processed +=
                safeStats.processed.load(std::memory_order_acquire);
            stats[signal].dropped +=
                safeStats.dropped.load(std::memory_order_acquire);
            stats[signal].handlerErrors +=
                safeStats.handlerErrors.load(std::memory_order_acquire);

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
    [[nodiscard]] std::vector<SignalID> getMonitoredSignals() const {
        std::shared_lock lock(mutex_);

        if (signalsToMonitor_.empty()) {
            // If no specific signals are specified, get all registered signals
            std::vector<SignalID> allSignals;

            // Get signals from registry
            auto& registryStats = SignalHandlerRegistry::getInstance();
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
        std::unique_lock lock(mutex_);
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

    // Prevent copying and moving
    SignalMonitor(const SignalMonitor&) = delete;
    SignalMonitor& operator=(const SignalMonitor&) = delete;
    SignalMonitor(SignalMonitor&&) = delete;
    SignalMonitor& operator=(SignalMonitor&&) = delete;

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

        std::shared_lock lock(mutex_);  // 使用共享锁来增加并发性能
        for (auto& [id, callback] : thresholdCallbacks_) {
            const auto& signalStats = stats[callback.signal];

            // Check received threshold
            if (callback.receivedThreshold > 0 &&
                signalStats.received >
                    callback.lastReceivedCount + callback.receivedThreshold) {
                // 释放锁在回调期间以避免死锁 - 仅当回调需要长时间运行时
                lock.unlock();
                callback.callback(callback.signal, signalStats);

                // 重新获取锁以更新状态
                lock.lock();
                callback.lastReceivedCount = signalStats.received;
            }

            // Check error threshold
            if (callback.errorThreshold > 0 &&
                signalStats.handlerErrors >
                    callback.lastErrorCount + callback.errorThreshold) {
                // 释放锁在回调期间以避免死锁
                lock.unlock();
                callback.callback(callback.signal, signalStats);

                // 重新获取锁以更新状态
                lock.lock();
                callback.lastErrorCount = signalStats.handlerErrors;
            }
        }
    }

    // Check inactivity callbacks
    void checkInactivity() {
        auto stats = getStatSnapshot();
        auto now = std::chrono::steady_clock::now();

        std::shared_lock lock(mutex_);  // 使用共享锁增加并发性能
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
                // 释放锁在回调期间以避免死锁
                lock.unlock();
                callback.callback(callback.signal, signalStats);

                // 重新获取锁以更新状态
                lock.lock();
                callback.lastActivity =
                    now;  // Reset to avoid repeated callbacks
            }
        }
    }

    std::atomic<bool> isRunning_;
    std::chrono::milliseconds monitorInterval_;
    std::vector<SignalID> signalsToMonitor_;
    std::jthread monitorThread_;
    mutable std::shared_mutex mutex_;     // 使用读写锁提高并发性
    std::atomic<int> nextCallbackId_{1};  // 使用原子变量避免互斥锁

#ifdef ATOM_USE_PMR
    std::pmr::synchronized_pool_resource callbackPoolResource_{};  // PMR池资源
    std::pmr::map<int, ThresholdCallback> thresholdCallbacks_{
        &callbackPoolResource_};
    std::pmr::map<int, InactivityCallback> inactivityCallbacks_{
        &callbackPoolResource_};
#else
    std::unordered_map<int, ThresholdCallback> thresholdCallbacks_;
    std::unordered_map<int, InactivityCallback> inactivityCallbacks_;
#endif
};

#endif  // ATOM_SYSTEM_SIGNAL_MONITOR_HPP
