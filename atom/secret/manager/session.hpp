#ifndef ATOM_SECRET_MANAGER_SESSION_HPP
#define ATOM_SECRET_MANAGER_SESSION_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace atom::secret {

/**
 * @brief Session state.
 */
enum class SessionState {
    Locked,    ///< Session is locked
    Unlocked,  ///< Session is unlocked
    Expired    ///< Session has expired
};

/**
 * @brief Session event types.
 */
enum class SessionEvent {
    Unlocked,        ///< Session was unlocked
    Locked,          ///< Session was locked
    Expired,         ///< Session expired due to timeout
    ActivityReset,   ///< Activity timer was reset
    PasswordChanged  ///< Master password was changed
};

/**
 * @brief Session configuration.
 */
struct SessionConfig {
    int autoLockTimeoutSeconds = 300;  ///< Auto-lock timeout (0 = disabled)
    bool lockOnSleep = true;           ///< Lock when system sleeps
    bool lockOnScreenLock = true;      ///< Lock when screen locks
    bool clearClipboardOnLock = true;  ///< Clear clipboard when locking
    int clipboardClearSeconds = 30;    ///< Clear clipboard after N seconds

    static SessionConfig defaults() { return SessionConfig{}; }
};

/**
 * @brief Session event callback type.
 */
using SessionEventCallback = std::function<void(SessionEvent event)>;

/**
 * @brief Manages password manager session state and auto-lock.
 */
class SessionManager {
public:
    /**
     * @brief Constructs a SessionManager.
     * @param config Session configuration.
     */
    explicit SessionManager(
        const SessionConfig& config = SessionConfig::defaults());

    ~SessionManager();

    // Disable copy
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /**
     * @brief Unlocks the session.
     */
    void unlock();

    /**
     * @brief Locks the session.
     */
    void lock();

    /**
     * @brief Gets the current session state.
     * @return Current state.
     */
    SessionState getState() const;

    /**
     * @brief Checks if the session is unlocked.
     * @return True if unlocked.
     */
    bool isUnlocked() const;

    /**
     * @brief Checks if the session is locked.
     * @return True if locked.
     */
    bool isLocked() const;

    /**
     * @brief Records activity to reset auto-lock timer.
     */
    void recordActivity();

    /**
     * @brief Gets seconds until auto-lock.
     * @return Seconds remaining, or -1 if disabled.
     */
    int getSecondsUntilLock() const;

    /**
     * @brief Gets the last activity time.
     * @return Last activity timestamp.
     */
    std::chrono::system_clock::time_point getLastActivity() const;

    /**
     * @brief Gets the session start time.
     * @return Session start timestamp.
     */
    std::chrono::system_clock::time_point getSessionStart() const;

    /**
     * @brief Updates the session configuration.
     * @param config New configuration.
     */
    void updateConfig(const SessionConfig& config);

    /**
     * @brief Gets the current configuration.
     * @return Current configuration.
     */
    const SessionConfig& getConfig() const;

    /**
     * @brief Registers an event callback.
     * @param callback Callback function.
     * @return Callback ID for unregistering.
     */
    int registerCallback(SessionEventCallback callback);

    /**
     * @brief Unregisters an event callback.
     * @param callbackId Callback ID from registerCallback.
     */
    void unregisterCallback(int callbackId);

    /**
     * @brief Checks for timeout and locks if needed.
     * Should be called periodically.
     */
    void checkTimeout();

private:
    void notifyEvent(SessionEvent event);

    SessionConfig config_;
    SessionState state_;
    std::chrono::system_clock::time_point lastActivity_;
    std::chrono::system_clock::time_point sessionStart_;
    mutable std::mutex mutex_;

    struct CallbackEntry {
        int id;
        SessionEventCallback callback;
    };
    std::vector<CallbackEntry> callbacks_;
    int nextCallbackId_;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_MANAGER_SESSION_HPP
