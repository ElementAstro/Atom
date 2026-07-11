#include "monitoring.h"
#include "detector.hpp"
#include "error_handling.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace shortcut_detector {

// Global monitor instance
std::unique_ptr<ShortcutMonitor> g_globalMonitor;

#ifdef _WIN32
ShortcutMonitor* ShortcutMonitor::instance_ = nullptr;
#endif

// MonitoringEvent implementation
std::string MonitoringEvent::toString() const {
    std::stringstream ss;
    ss << "Event: ";

    switch (type) {
        case MonitoringEventType::ShortcutRegistered: ss << "ShortcutRegistered"; break;
        case MonitoringEventType::ShortcutUnregistered: ss << "ShortcutUnregistered"; break;
        case MonitoringEventType::ShortcutPressed: ss << "ShortcutPressed"; break;
        case MonitoringEventType::ShortcutReleased: ss << "ShortcutReleased"; break;
        case MonitoringEventType::ConflictDetected: ss << "ConflictDetected"; break;
        case MonitoringEventType::ConflictResolved: ss << "ConflictResolved"; break;
        case MonitoringEventType::SystemStateChanged: ss << "SystemStateChanged"; break;
        case MonitoringEventType::PermissionChanged: ss << "PermissionChanged"; break;
        case MonitoringEventType::ErrorOccurred: ss << "ErrorOccurred"; break;
        default: ss << "Unknown"; break;
    }

    ss << ", Shortcut: " << shortcut.toString();
    if (!source.empty()) ss << ", Source: " << source;
    if (!description.empty()) ss << ", Description: " << description;

    return ss.str();
}

// EventFilter implementation
bool EventFilter::shouldProcess(const MonitoringEvent& event) const {
    // Check allowed types
    if (!allowedTypes.empty()) {
        if (std::find(allowedTypes.begin(), allowedTypes.end(), event.type) == allowedTypes.end()) {
            return false;
        }
    }

    // Check blocked sources
    if (!blockedSources.empty()) {
        if (std::find(blockedSources.begin(), blockedSources.end(), event.source) != blockedSources.end()) {
            return false;
        }
    }

    // Check allowed sources
    if (!allowedSources.empty()) {
        if (std::find(allowedSources.begin(), allowedSources.end(), event.source) == allowedSources.end()) {
            return false;
        }
    }

    // Apply custom filter
    if (customFilter && !customFilter(event)) {
        return false;
    }

    return true;
}

// ShortcutMonitor implementation
ShortcutMonitor::ShortcutMonitor(const MonitoringConfig& config)
    : config_(config) {
#ifdef _WIN32
    instance_ = this;
    keyboardHook_ = nullptr;
#endif
    stats_.startTime = std::chrono::system_clock::now();
    spdlog::debug("ShortcutMonitor created");
}

ShortcutMonitor::~ShortcutMonitor() {
    stop();
#ifdef _WIN32
    instance_ = nullptr;
#endif
    spdlog::debug("ShortcutMonitor destroyed");
}

bool ShortcutMonitor::start() {
    SHORTCUT_ERROR_CONTEXT();

    if (running_.load()) {
        spdlog::warn("Monitor is already running");
        return true;
    }

    try {
        initializePlatformMonitoring();

        stopRequested_.store(false);
        running_.store(true);

        // Start monitoring threads
        monitoringThread_ = std::make_unique<std::thread>(&ShortcutMonitor::monitoringLoop, this);
        eventProcessingThread_ = std::make_unique<std::thread>(&ShortcutMonitor::eventProcessingLoop, this);

        stats_.startTime = std::chrono::system_clock::now();

        spdlog::info("ShortcutMonitor started successfully");

        // Emit start event
        MonitoringEvent startEvent(MonitoringEventType::SystemStateChanged,
                                  ShortcutBinding(), "ShortcutMonitor", "Monitoring started");
        emitEvent(startEvent);

        return true;

    } catch (const std::exception& e) {
        running_.store(false);
        spdlog::error("Failed to start ShortcutMonitor: {}", e.what());
        return false;
    }
}

void ShortcutMonitor::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("Stopping ShortcutMonitor...");

    stopRequested_.store(true);
    running_.store(false);

    // Wait for threads to finish
    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }
    if (eventProcessingThread_ && eventProcessingThread_->joinable()) {
        eventProcessingThread_->join();
    }

    cleanupPlatformMonitoring();

    // Emit stop event
    MonitoringEvent stopEvent(MonitoringEventType::SystemStateChanged,
                             ShortcutBinding(), "ShortcutMonitor", "Monitoring stopped");
    emitEvent(stopEvent);

    spdlog::info("ShortcutMonitor stopped");
}

void ShortcutMonitor::registerCallback(EventCallback callback, const EventFilter& filter) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.emplace_back(callback, filter);
    spdlog::debug("Registered event callback");
}

void ShortcutMonitor::clearCallbacks() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.clear();
    spdlog::debug("Cleared all event callbacks");
}

void ShortcutMonitor::addShortcut(const ShortcutBinding& shortcut, const std::string& owner) {
    {
        std::lock_guard<std::mutex> lock(shortcutMutex_);
        monitoredShortcuts_[shortcut] = owner;
    }

    MonitoringEvent event(MonitoringEventType::ShortcutRegistered, shortcut, owner,
                         "Shortcut added to monitoring");
    emitEvent(event);

    spdlog::debug("Added shortcut to monitoring: {}", shortcut.toString());
}

void ShortcutMonitor::removeShortcut(const ShortcutBinding& shortcut) {
    std::string owner;
    {
        std::lock_guard<std::mutex> lock(shortcutMutex_);
        auto it = monitoredShortcuts_.find(shortcut);
        if (it != monitoredShortcuts_.end()) {
            owner = it->second;
            monitoredShortcuts_.erase(it);
        }
    }

    MonitoringEvent event(MonitoringEventType::ShortcutUnregistered, shortcut, owner,
                         "Shortcut removed from monitoring");
    emitEvent(event);

    spdlog::debug("Removed shortcut from monitoring: {}", shortcut.toString());
}

std::vector<ShortcutBinding> ShortcutMonitor::getMonitoredShortcuts() const {
    std::lock_guard<std::mutex> lock(shortcutMutex_);
    std::vector<ShortcutBinding> result;
    result.reserve(monitoredShortcuts_.size());

    for (const auto& [shortcut, owner] : monitoredShortcuts_) {
        result.push_back(shortcut);
    }

    return result;
}

void ShortcutMonitor::checkConflicts() {
    if (!config_.enableConflictDetection) {
        return;
    }

    detectConflicts();
}

std::vector<MonitoringEvent> ShortcutMonitor::getRecentEvents(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(eventMutex_);

    std::vector<MonitoringEvent> result;
    size_t startIndex = eventHistory_.size() > maxCount ? eventHistory_.size() - maxCount : 0;

    for (size_t i = startIndex; i < eventHistory_.size(); ++i) {
        result.push_back(eventHistory_[i]);
    }

    return result;
}

std::vector<MonitoringEvent> ShortcutMonitor::getEventsByType(MonitoringEventType type, size_t maxCount) const {
    std::lock_guard<std::mutex> lock(eventMutex_);

    std::vector<MonitoringEvent> result;

    for (auto it = eventHistory_.rbegin(); it != eventHistory_.rend() && result.size() < maxCount; ++it) {
        if (it->type == type) {
            result.push_back(*it);
        }
    }

    std::reverse(result.begin(), result.end());
    return result;
}

void ShortcutMonitor::clearEventHistory() {
    std::lock_guard<std::mutex> lock(eventMutex_);
    eventHistory_.clear();
    spdlog::debug("Cleared event history");
}

void ShortcutMonitor::updateConfig(const MonitoringConfig& config) {
    config_ = config;
    spdlog::debug("Updated monitoring configuration");
}

ShortcutMonitor::MonitoringStats ShortcutMonitor::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    MonitoringStats stats = stats_;

    if (running_.load()) {
        auto now = std::chrono::system_clock::now();
        stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_.startTime);

        if (stats.uptime.count() > 0) {
            stats.eventsPerSecond = static_cast<double>(stats.totalEvents) /
                                   (static_cast<double>(stats.uptime.count()) / 1000.0);
        }
    }

    return stats;
}

void ShortcutMonitor::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = MonitoringStats{};
    stats_.startTime = std::chrono::system_clock::now();
    spdlog::debug("Reset monitoring statistics");
}

void ShortcutMonitor::monitoringLoop() {
    spdlog::debug("Monitoring loop started");

    while (!stopRequested_.load()) {
        try {
            if (config_.enableSystemStateMonitoring) {
                checkSystemState();
            }

            if (config_.enableConflictDetection) {
                detectConflicts();
            }

            updateStats();

            std::this_thread::sleep_for(config_.pollingInterval);

        } catch (const std::exception& e) {
            spdlog::error("Error in monitoring loop: {}", e.what());

            MonitoringEvent errorEvent(MonitoringEventType::ErrorOccurred,
                                     ShortcutBinding(), "MonitoringLoop", e.what());
            emitEvent(errorEvent);
        }
    }

    spdlog::debug("Monitoring loop stopped");
}

void ShortcutMonitor::eventProcessingLoop() {
    spdlog::debug("Event processing loop started");

    while (!stopRequested_.load()) {
        try {
            std::unique_lock<std::mutex> lock(eventMutex_);

            if (!eventQueue_.empty()) {
                MonitoringEvent event = eventQueue_.front();
                eventQueue_.pop();
                lock.unlock();

                processEvent(event);
            } else {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

        } catch (const std::exception& e) {
            spdlog::error("Error in event processing loop: {}", e.what());
        }
    }

    spdlog::debug("Event processing loop stopped");
}

void ShortcutMonitor::processEvent(const MonitoringEvent& event) {
    // Add to history
    {
        std::lock_guard<std::mutex> lock(eventMutex_);
        eventHistory_.push_back(event);

        // Limit history size
        if (eventHistory_.size() > config_.maxEventQueueSize) {
            eventHistory_.erase(eventHistory_.begin());
        }
    }

    // Log event if enabled
    if (config_.logEvents) {
        spdlog::info("Monitoring event: {}", event.toString());
    }

    // Call registered callbacks
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (const auto& [callback, filter] : callbacks_) {
        if (filter.shouldProcess(event)) {
            try {
                callback(event);
            } catch (const std::exception& e) {
                spdlog::error("Error in event callback: {}", e.what());
            }
        }
    }
}

void ShortcutMonitor::emitEvent(const MonitoringEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex_);

    if (eventQueue_.size() >= config_.maxEventQueueSize) {
        spdlog::warn("Event queue is full, dropping oldest event");
        eventQueue_.pop();
    }

    eventQueue_.push(event);

    // Update stats
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        stats_.totalEvents++;
    }
}

void ShortcutMonitor::checkSystemState() {
    // Check for changes in system state (processes, hooks, etc.)
    static auto lastCheck = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= 5) {
        // Perform system state check every 5 seconds
        try {
            ShortcutDetector detector;
            bool hasHooks = detector.hasKeyboardHookInstalled();

            static bool lastHookState = false;
            if (hasHooks != lastHookState) {
                MonitoringEvent event(MonitoringEventType::SystemStateChanged,
                                    ShortcutBinding(), "System",
                                    hasHooks ? "Keyboard hooks detected" : "Keyboard hooks removed");
                emitEvent(event);
                lastHookState = hasHooks;
            }

        } catch (const std::exception& e) {
            spdlog::warn("Error checking system state: {}", e.what());
        }

        lastCheck = now;
    }
}

void ShortcutMonitor::detectConflicts() {
    std::lock_guard<std::mutex> lock(shortcutMutex_);

    // Simple conflict detection - check for duplicate shortcuts
    std::unordered_map<std::string, std::vector<ShortcutBinding>> shortcutGroups;

    for (const auto& [shortcut, owner] : monitoredShortcuts_) {
        std::string key = shortcut.toString();
        shortcutGroups[key].push_back(shortcut);
    }

    for (const auto& [key, shortcuts] : shortcutGroups) {
        if (shortcuts.size() > 1) {
            MonitoringEvent event(MonitoringEventType::ConflictDetected,
                                shortcuts[0], "ConflictDetector",
                                "Multiple shortcuts with same key combination");
            event.addMetadata("conflict_count", std::to_string(shortcuts.size()));
            emitEvent(event);

            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.conflictsDetected++;
        }
    }
}

void ShortcutMonitor::updateStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    std::lock_guard<std::mutex> shortcutLock(shortcutMutex_);
    stats_.shortcutsMonitored = monitoredShortcuts_.size();
}

void ShortcutMonitor::initializePlatformMonitoring() {
#ifdef _WIN32
    if (config_.enableKeyboardHooks) {
        if (!installKeyboardHook()) {
            spdlog::warn("Failed to install keyboard hook - elevated privileges may be required");
        }
    }
#endif
}

void ShortcutMonitor::cleanupPlatformMonitoring() {
#ifdef _WIN32
    uninstallKeyboardHook();
#endif
}

bool ShortcutMonitor::installKeyboardHook() {
#ifdef _WIN32
    if (keyboardHook_) {
        return true; // Already installed
    }

    keyboardHook_ = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHookProc,
                                    GetModuleHandle(NULL), 0);

    if (keyboardHook_) {
        spdlog::debug("Keyboard hook installed successfully");
        return true;
    } else {
        spdlog::error("Failed to install keyboard hook: {}", GetLastError());
        return false;
    }
#else
    return false;
#endif
}

void ShortcutMonitor::uninstallKeyboardHook() {
#ifdef _WIN32
    if (keyboardHook_) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
        spdlog::debug("Keyboard hook uninstalled");
    }
#endif
}

#ifdef _WIN32
LRESULT CALLBACK ShortcutMonitor::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && instance_) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // Create shortcut from current key state
            bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool win = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
                      (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

            Shortcut shortcut(kbStruct->vkCode, ctrl, alt, shift, win);
            ShortcutBinding advShortcut = ShortcutBinding::createKeyboard(shortcut);

            MonitoringEvent event(MonitoringEventType::ShortcutPressed, advShortcut,
                                "KeyboardHook", "Key combination pressed");
            instance_->emitEvent(event);
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
#endif

// ConflictResolver implementation
ConflictResolver::ConflictResolver(ResolutionStrategy strategy) : strategy_(strategy) {}

ShortcutBinding ConflictResolver::resolveConflict(const std::vector<ShortcutBinding>& conflictingShortcuts) {
    if (conflictingShortcuts.empty()) {
        throw ValidationException("No shortcuts provided for conflict resolution");
    }

    if (conflictingShortcuts.size() == 1) {
        return conflictingShortcuts[0];
    }

    switch (strategy_) {
        case ResolutionStrategy::FirstWins:
            return conflictingShortcuts[0];

        case ResolutionStrategy::LastWins:
            return conflictingShortcuts.back();

        case ResolutionStrategy::HighestPriority: {
            auto maxIt = std::max_element(conflictingShortcuts.begin(), conflictingShortcuts.end(),
                [this](const ShortcutBinding& a, const ShortcutBinding& b) {
                    return calculatePriority(a) < calculatePriority(b);
                });
            return *maxIt;
        }

        case ResolutionStrategy::UserChoice:
            if (userChoiceCallback_) {
                int choice = userChoiceCallback_(conflictingShortcuts);
                if (choice >= 0 && choice < static_cast<int>(conflictingShortcuts.size())) {
                    return conflictingShortcuts[choice];
                }
            }
            // Fall through to automatic if user choice fails

        case ResolutionStrategy::Automatic:
        default: {
            int choice = automaticResolution(conflictingShortcuts);
            return conflictingShortcuts[choice];
        }
    }
}

void ConflictResolver::setUserChoiceCallback(std::function<int(const std::vector<ShortcutBinding>&)> callback) {
    userChoiceCallback_ = callback;
}

int ConflictResolver::calculatePriority(const ShortcutBinding& shortcut) const {
    int priority = 0;

    // System shortcuts have highest priority
    if (shortcut.category == "System") priority += 1000;
    else if (shortcut.category == "Application") priority += 500;
    else if (shortcut.category == "User") priority += 100;

    // Shortcuts with more modifiers have higher priority
    if (shortcut.type == ShortcutType::Keyboard && !shortcut.keySequence.empty()) {
        const auto& key = shortcut.keySequence[0];
        if (key.ctrl) priority += 10;
        if (key.alt) priority += 10;
        if (key.shift) priority += 5;
        if (key.win) priority += 15;
    }

    return priority;
}

int ConflictResolver::automaticResolution(const std::vector<ShortcutBinding>& shortcuts) const {
    // Use priority-based resolution as default automatic strategy
    int maxPriority = -1;
    int bestChoice = 0;

    for (size_t i = 0; i < shortcuts.size(); ++i) {
        int priority = calculatePriority(shortcuts[i]);
        if (priority > maxPriority) {
            maxPriority = priority;
            bestChoice = static_cast<int>(i);
        }
    }

    return bestChoice;
}

// ChangeNotifier implementation
void ChangeNotifier::registerCallback(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(callback);
}

void ChangeNotifier::notifyChange(const ChangeNotification& notification) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.push_back(notification);

        // Limit history size
        if (history_.size() > 1000) {
            history_.erase(history_.begin());
        }

        // Notify callbacks
        for (const auto& callback : callbacks_) {
            try {
                callback(notification);
            } catch (const std::exception& e) {
                spdlog::error("Error in change notification callback: {}", e.what());
            }
        }
    }
}

std::vector<ChangeNotifier::ChangeNotification> ChangeNotifier::getRecentNotifications(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<ChangeNotification> result;
    size_t startIndex = history_.size() > maxCount ? history_.size() - maxCount : 0;

    for (size_t i = startIndex; i < history_.size(); ++i) {
        result.push_back(history_[i]);
    }

    return result;
}

void ChangeNotifier::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

// Global functions
ShortcutMonitor& getGlobalMonitor() {
    if (!g_globalMonitor) {
        g_globalMonitor = std::make_unique<ShortcutMonitor>();
    }
    return *g_globalMonitor;
}

bool initializeGlobalMonitoring(const MonitoringConfig& config) {
    try {
        g_globalMonitor = std::make_unique<ShortcutMonitor>(config);
        return g_globalMonitor->start();
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize global monitoring: {}", e.what());
        return false;
    }
}

void shutdownGlobalMonitoring() {
    if (g_globalMonitor) {
        g_globalMonitor->stop();
        g_globalMonitor.reset();
    }
}

}  // namespace shortcut_detector
