#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include "shortcut.h"
#include "advanced_shortcut.h"
#include "status.h"

namespace shortcut_detector {

/**
 * @brief Types of monitoring events
 */
enum class MonitoringEventType {
    ShortcutRegistered,     // New shortcut was registered
    ShortcutUnregistered,   // Shortcut was unregistered
    ShortcutPressed,        // Shortcut was pressed
    ShortcutReleased,       // Shortcut was released
    ConflictDetected,       // Conflict between shortcuts detected
    ConflictResolved,       // Conflict was resolved
    SystemStateChanged,     // System state changed (hooks, processes)
    PermissionChanged,      // Permission state changed
    ErrorOccurred          // Error occurred during monitoring
};

/**
 * @brief Monitoring event data
 */
struct MonitoringEvent {
    MonitoringEventType type;
    std::chrono::system_clock::time_point timestamp;
    AdvancedShortcut shortcut;
    std::string source;           // Source of the event (process, system, etc.)
    std::string description;      // Human-readable description
    std::unordered_map<std::string, std::string> metadata; // Additional data
    
    MonitoringEvent(MonitoringEventType t, const AdvancedShortcut& s = AdvancedShortcut(),
                   const std::string& src = "", const std::string& desc = "")
        : type(t), timestamp(std::chrono::system_clock::now()), 
          shortcut(s), source(src), description(desc) {}
    
    void addMetadata(const std::string& key, const std::string& value) {
        metadata[key] = value;
    }
    
    std::string toString() const;
};

/**
 * @brief Event filter for selective monitoring
 */
struct EventFilter {
    std::vector<MonitoringEventType> allowedTypes;
    std::vector<std::string> allowedSources;
    std::vector<std::string> blockedSources;
    std::function<bool(const MonitoringEvent&)> customFilter;
    
    bool shouldProcess(const MonitoringEvent& event) const;
};

/**
 * @brief Event callback interface
 */
using EventCallback = std::function<void(const MonitoringEvent&)>;

/**
 * @brief Monitoring configuration
 */
struct MonitoringConfig {
    bool enableRealTimeMonitoring{true};
    bool enableConflictDetection{true};
    bool enableSystemStateMonitoring{true};
    bool enableKeyboardHooks{false};        // Requires elevated privileges
    std::chrono::milliseconds pollingInterval{100};
    std::chrono::milliseconds eventQueueTimeout{5000};
    size_t maxEventQueueSize{1000};
    bool logEvents{true};
    std::string logLevel{"info"};
};

/**
 * @brief Real-time shortcut monitoring system
 */
class ShortcutMonitor {
public:
    explicit ShortcutMonitor(const MonitoringConfig& config = MonitoringConfig{});
    ~ShortcutMonitor();
    
    // Prevent copy and move
    ShortcutMonitor(const ShortcutMonitor&) = delete;
    ShortcutMonitor& operator=(const ShortcutMonitor&) = delete;
    ShortcutMonitor(ShortcutMonitor&&) = delete;
    ShortcutMonitor& operator=(ShortcutMonitor&&) = delete;
    
    /**
     * @brief Start monitoring
     */
    bool start();
    
    /**
     * @brief Stop monitoring
     */
    void stop();
    
    /**
     * @brief Check if monitoring is active
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Register event callback
     */
    void registerCallback(EventCallback callback, const EventFilter& filter = EventFilter{});
    
    /**
     * @brief Unregister all callbacks
     */
    void clearCallbacks();
    
    /**
     * @brief Add shortcut to monitor
     */
    void addShortcut(const AdvancedShortcut& shortcut, const std::string& owner = "");
    
    /**
     * @brief Remove shortcut from monitoring
     */
    void removeShortcut(const AdvancedShortcut& shortcut);
    
    /**
     * @brief Get all monitored shortcuts
     */
    std::vector<AdvancedShortcut> getMonitoredShortcuts() const;
    
    /**
     * @brief Force conflict detection check
     */
    void checkConflicts();
    
    /**
     * @brief Get recent events
     */
    std::vector<MonitoringEvent> getRecentEvents(size_t maxCount = 100) const;
    
    /**
     * @brief Get events by type
     */
    std::vector<MonitoringEvent> getEventsByType(MonitoringEventType type, 
                                                 size_t maxCount = 100) const;
    
    /**
     * @brief Clear event history
     */
    void clearEventHistory();
    
    /**
     * @brief Update monitoring configuration
     */
    void updateConfig(const MonitoringConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const MonitoringConfig& getConfig() const { return config_; }
    
    /**
     * @brief Get monitoring statistics
     */
    struct MonitoringStats {
        size_t totalEvents{0};
        size_t conflictsDetected{0};
        size_t shortcutsMonitored{0};
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds uptime{0};
        double eventsPerSecond{0.0};
    };
    
    MonitoringStats getStats() const;
    
    /**
     * @brief Reset statistics
     */
    void resetStats();

private:
    MonitoringConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    
    // Threading
    std::unique_ptr<std::thread> monitoringThread_;
    std::unique_ptr<std::thread> eventProcessingThread_;
    
    // Event system
    mutable std::mutex eventMutex_;
    std::queue<MonitoringEvent> eventQueue_;
    std::vector<MonitoringEvent> eventHistory_;
    
    // Callbacks
    mutable std::mutex callbackMutex_;
    std::vector<std::pair<EventCallback, EventFilter>> callbacks_;
    
    // Monitored shortcuts
    mutable std::mutex shortcutMutex_;
    std::unordered_map<AdvancedShortcut, std::string> monitoredShortcuts_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    MonitoringStats stats_;
    
    // Internal methods
    void monitoringLoop();
    void eventProcessingLoop();
    void processEvent(const MonitoringEvent& event);
    void emitEvent(const MonitoringEvent& event);
    void checkSystemState();
    void detectConflicts();
    void updateStats();
    
    // Platform-specific monitoring
    void initializePlatformMonitoring();
    void cleanupPlatformMonitoring();
    bool installKeyboardHook();
    void uninstallKeyboardHook();
    
#ifdef _WIN32
    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static ShortcutMonitor* instance_;
    HHOOK keyboardHook_;
#endif
};

/**
 * @brief Automatic conflict resolver
 */
class ConflictResolver {
public:
    enum class ResolutionStrategy {
        FirstWins,          // First registered shortcut wins
        LastWins,           // Last registered shortcut wins
        HighestPriority,    // Shortcut with highest priority wins
        UserChoice,         // Ask user to resolve
        Automatic          // Use heuristics to resolve
    };
    
    ConflictResolver(ResolutionStrategy strategy = ResolutionStrategy::Automatic);
    
    /**
     * @brief Resolve conflict between shortcuts
     */
    AdvancedShortcut resolveConflict(const std::vector<AdvancedShortcut>& conflictingShortcuts);
    
    /**
     * @brief Set resolution strategy
     */
    void setStrategy(ResolutionStrategy strategy) { strategy_ = strategy; }
    
    /**
     * @brief Get current strategy
     */
    ResolutionStrategy getStrategy() const { return strategy_; }
    
    /**
     * @brief Set user choice callback for UserChoice strategy
     */
    void setUserChoiceCallback(std::function<int(const std::vector<AdvancedShortcut>&)> callback);

private:
    ResolutionStrategy strategy_;
    std::function<int(const std::vector<AdvancedShortcut>&)> userChoiceCallback_;
    
    int calculatePriority(const AdvancedShortcut& shortcut) const;
    int automaticResolution(const std::vector<AdvancedShortcut>& shortcuts) const;
};

/**
 * @brief Change notification system
 */
class ChangeNotifier {
public:
    enum class ChangeType {
        ShortcutAdded,
        ShortcutRemoved,
        ShortcutModified,
        ConflictDetected,
        ConflictResolved,
        SystemStateChanged
    };
    
    struct ChangeNotification {
        ChangeType type;
        std::chrono::system_clock::time_point timestamp;
        std::string description;
        std::unordered_map<std::string, std::string> details;
        
        ChangeNotification(ChangeType t, const std::string& desc = "")
            : type(t), timestamp(std::chrono::system_clock::now()), description(desc) {}
    };
    
    using ChangeCallback = std::function<void(const ChangeNotification&)>;
    
    /**
     * @brief Register change callback
     */
    void registerCallback(ChangeCallback callback);
    
    /**
     * @brief Notify about change
     */
    void notifyChange(const ChangeNotification& notification);
    
    /**
     * @brief Get recent notifications
     */
    std::vector<ChangeNotification> getRecentNotifications(size_t maxCount = 50) const;
    
    /**
     * @brief Clear notification history
     */
    void clearHistory();

private:
    mutable std::mutex mutex_;
    std::vector<ChangeCallback> callbacks_;
    std::vector<ChangeNotification> history_;
};

/**
 * @brief Global monitoring instance
 */
extern std::unique_ptr<ShortcutMonitor> g_globalMonitor;

/**
 * @brief Get or create global monitor instance
 */
ShortcutMonitor& getGlobalMonitor();

/**
 * @brief Initialize global monitoring with configuration
 */
bool initializeGlobalMonitoring(const MonitoringConfig& config = MonitoringConfig{});

/**
 * @brief Shutdown global monitoring
 */
void shutdownGlobalMonitoring();

}  // namespace shortcut_detector
