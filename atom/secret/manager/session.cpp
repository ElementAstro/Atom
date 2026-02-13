#include "session.hpp"

namespace atom::secret {

SessionManager::SessionManager(const SessionConfig& config)
    : config_(config),
      state_(SessionState::Locked),
      lastActivity_(std::chrono::system_clock::now()),
      sessionStart_(),
      nextCallbackId_(1) {}

SessionManager::~SessionManager() = default;

void SessionManager::unlock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Unlocked) {
        state_ = SessionState::Unlocked;
        sessionStart_ = std::chrono::system_clock::now();
        lastActivity_ = sessionStart_;
        notifyEvent(SessionEvent::Unlocked);
    }
}

void SessionManager::lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != SessionState::Locked) {
        state_ = SessionState::Locked;
        notifyEvent(SessionEvent::Locked);
    }
}

SessionState SessionManager::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool SessionManager::isUnlocked() const {
    return getState() == SessionState::Unlocked;
}

bool SessionManager::isLocked() const {
    return getState() != SessionState::Unlocked;
}

void SessionManager::recordActivity() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == SessionState::Unlocked) {
        lastActivity_ = std::chrono::system_clock::now();
        notifyEvent(SessionEvent::ActivityReset);
    }
}

int SessionManager::getSecondsUntilLock() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.autoLockTimeoutSeconds <= 0) {
        return -1;  // Disabled
    }

    if (state_ != SessionState::Unlocked) {
        return 0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity_)
            .count();

    int remaining = config_.autoLockTimeoutSeconds - static_cast<int>(elapsed);
    return remaining > 0 ? remaining : 0;
}

std::chrono::system_clock::time_point SessionManager::getLastActivity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastActivity_;
}

std::chrono::system_clock::time_point SessionManager::getSessionStart() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessionStart_;
}

void SessionManager::updateConfig(const SessionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const SessionConfig& SessionManager::getConfig() const { return config_; }

int SessionManager::registerCallback(SessionEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextCallbackId_++;
    callbacks_.push_back({id, std::move(callback)});
    return id;
}

void SessionManager::unregisterCallback(int callbackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
                                    [callbackId](const CallbackEntry& entry) {
                                        return entry.id == callbackId;
                                    }),
                     callbacks_.end());
}

void SessionManager::checkTimeout() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != SessionState::Unlocked) {
        return;
    }

    if (config_.autoLockTimeoutSeconds <= 0) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity_)
            .count();

    if (elapsed >= config_.autoLockTimeoutSeconds) {
        state_ = SessionState::Expired;
        notifyEvent(SessionEvent::Expired);
        state_ = SessionState::Locked;
        notifyEvent(SessionEvent::Locked);
    }
}

void SessionManager::notifyEvent(SessionEvent event) {
    // Note: mutex is already held by caller
    for (const auto& entry : callbacks_) {
        try {
            entry.callback(event);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

}  // namespace atom::secret
