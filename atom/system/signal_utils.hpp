#ifndef SIGNAL_UTILS_HPP
#define SIGNAL_UTILS_HPP

#include "signal.hpp"

#include <map>
#include <memory>
#include <string>

/**
 * @brief A scoped signal handler that automatically removes itself when
 * destroyed.
 *
 * This class provides RAII-style management of signal handlers to ensure
 * they're properly cleaned up when the object goes out of scope.
 */
class ScopedSignalHandler {
public:
    /**
     * @brief Construct a new Scoped Signal Handler
     *
     * @param signal The signal to handle
     * @param handler The handler function
     * @param priority Priority of the handler (higher values = higher priority)
     * @param useSafeManager Whether to use the SafeSignalManager (true) or
     * direct registry (false)
     */
    ScopedSignalHandler(SignalID signal, const SignalHandler& handler,
                        int priority = 0, bool useSafeManager = true)
        : signal_(signal), handlerId_(-1), useSafeManager_(useSafeManager) {
        if (useSafeManager_) {
            handlerId_ = SafeSignalManager::getInstance().addSafeSignalHandler(
                signal, handler, priority);
        } else {
            handlerId_ = SignalHandlerRegistry::getInstance().setSignalHandler(
                signal, handler, priority);
        }
    }

    /**
     * @brief Destructor removes the signal handler
     */
    ~ScopedSignalHandler() {
        if (handlerId_ >= 0) {
            if (useSafeManager_) {
                SafeSignalManager::getInstance().removeSafeSignalHandlerById(
                    handlerId_);
            } else {
                SignalHandlerRegistry::getInstance().removeSignalHandlerById(
                    handlerId_);
            }
        }
    }

    // Prevent copying
    ScopedSignalHandler(const ScopedSignalHandler&) = delete;
    ScopedSignalHandler& operator=(const ScopedSignalHandler&) = delete;

    // Allow moving
    ScopedSignalHandler(ScopedSignalHandler&& other) noexcept
        : signal_(other.signal_),
          handlerId_(other.handlerId_),
          useSafeManager_(other.useSafeManager_) {
        other.handlerId_ = -1;  // Prevent the other from removing the handler
    }

    ScopedSignalHandler& operator=(ScopedSignalHandler&& other) noexcept {
        if (this != &other) {
            // Clean up existing handler
            if (handlerId_ >= 0) {
                if (useSafeManager_) {
                    SafeSignalManager::getInstance()
                        .removeSafeSignalHandlerById(handlerId_);
                } else {
                    SignalHandlerRegistry::getInstance()
                        .removeSignalHandlerById(handlerId_);
                }
            }

            // Move from other
            signal_ = other.signal_;
            handlerId_ = other.handlerId_;
            useSafeManager_ = other.useSafeManager_;

            other.handlerId_ =
                -1;  // Prevent the other from removing the handler
        }
        return *this;
    }

private:
    SignalID signal_;
    int handlerId_;
    bool useSafeManager_;
};

/**
 * @brief A signal group that manages multiple related signal handlers
 */
class SignalGroup {
public:
    /**
     * @brief Construct a new Signal Group
     *
     * @param groupName Name of the group (for logging)
     * @param useSafeManager Whether to use the SafeSignalManager (true) or
     * direct registry (false)
     */
    SignalGroup(const std::string& groupName = "", bool useSafeManager = true)
        : groupName_(groupName), useSafeManager_(useSafeManager) {}

    /**
     * @brief Destructor removes all registered handlers
     */
    ~SignalGroup() { removeAll(); }

    /**
     * @brief Add a handler to the group
     *
     * @param signal The signal to handle
     * @param handler The handler function
     * @param priority Priority of the handler (higher values = higher priority)
     * @return int ID of the registered handler
     */
    int addHandler(SignalID signal, const SignalHandler& handler,
                   int priority = 0) {
        int handlerId;
        if (useSafeManager_) {
            handlerId = SafeSignalManager::getInstance().addSafeSignalHandler(
                signal, handler, priority, groupName_);
        } else {
            handlerId = SignalHandlerRegistry::getInstance().setSignalHandler(
                signal, handler, priority, groupName_);
        }

        handlerIds_[signal].push_back(handlerId);
        return handlerId;
    }

    /**
     * @brief Remove a specific handler by ID
     *
     * @param handlerId ID of the handler to remove
     * @return true if handler was successfully removed
     */
    bool removeHandler(int handlerId) {
        for (auto& [signal, ids] : handlerIds_) {
            auto it = std::find(ids.begin(), ids.end(), handlerId);
            if (it != ids.end()) {
                ids.erase(it);

                bool success;
                if (useSafeManager_) {
                    success = SafeSignalManager::getInstance()
                                  .removeSafeSignalHandlerById(handlerId);
                } else {
                    success = SignalHandlerRegistry::getInstance()
                                  .removeSignalHandlerById(handlerId);
                }

                return success;
            }
        }
        return false;
    }

    /**
     * @brief Remove all handlers for a specific signal
     *
     * @param signal Signal to remove handlers for
     * @return int Number of handlers removed
     */
    int removeSignalHandlers(SignalID signal) {
        auto it = handlerIds_.find(signal);
        if (it == handlerIds_.end()) {
            return 0;
        }

        int removed = 0;
        for (int handlerId : it->second) {
            bool success;
            if (useSafeManager_) {
                success = SafeSignalManager::getInstance()
                              .removeSafeSignalHandlerById(handlerId);
            } else {
                success = SignalHandlerRegistry::getInstance()
                              .removeSignalHandlerById(handlerId);
            }

            if (success) {
                removed++;
            }
        }

        handlerIds_.erase(it);
        return removed;
    }

    /**
     * @brief Remove all handlers in this group
     *
     * @return int Number of handlers removed
     */
    int removeAll() {
        int removed = 0;
        for (const auto& [signal, ids] : handlerIds_) {
            for (int handlerId : ids) {
                bool success;
                if (useSafeManager_) {
                    success = SafeSignalManager::getInstance()
                                  .removeSafeSignalHandlerById(handlerId);
                } else {
                    success = SignalHandlerRegistry::getInstance()
                                  .removeSignalHandlerById(handlerId);
                }

                if (success) {
                    removed++;
                }
            }
        }

        handlerIds_.clear();
        return removed;
    }

    /**
     * @brief Get all registered handler IDs
     *
     * @return const std::map<SignalID, std::vector<int>>& Map of signal IDs to
     * handler IDs
     */
    const std::map<SignalID, std::vector<int>>& getHandlerIds() const {
        return handlerIds_;
    }

    /**
     * @brief Get the group name
     *
     * @return const std::string& Group name
     */
    const std::string& getGroupName() const { return groupName_; }

private:
    std::string groupName_;
    bool useSafeManager_;
    std::map<SignalID, std::vector<int>> handlerIds_;
};

/**
 * @brief Create a smart pointer to a SignalGroup
 *
 * @param groupName Name of the group (for logging)
 * @param useSafeManager Whether to use the SafeSignalManager (true) or direct
 * registry (false)
 * @return std::shared_ptr<SignalGroup> Shared pointer to the new group
 */
inline std::shared_ptr<SignalGroup> makeSignalGroup(
    const std::string& groupName = "", bool useSafeManager = true) {
    return std::make_shared<SignalGroup>(groupName, useSafeManager);
}

/**
 * @brief Get the signal name as a string
 *
 * @param signal Signal ID
 * @return std::string Name of the signal
 */
inline std::string getSignalName(int signal) {
    static const std::map<int, std::string> signalNames = {
        {SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},   {SIGILL, "SIGILL"},
        {SIGINT, "SIGINT"},   {SIGSEGV, "SIGSEGV"}, {SIGTERM, "SIGTERM"},
#if !defined(_WIN32) && !defined(_WIN64)
        {SIGALRM, "SIGALRM"}, {SIGBUS, "SIGBUS"},   {SIGCHLD, "SIGCHLD"},
        {SIGCONT, "SIGCONT"}, {SIGHUP, "SIGHUP"},   {SIGKILL, "SIGKILL"},
        {SIGPIPE, "SIGPIPE"}, {SIGQUIT, "SIGQUIT"}, {SIGSTOP, "SIGSTOP"},
        {SIGTSTP, "SIGTSTP"}, {SIGTTIN, "SIGTTIN"}, {SIGTTOU, "SIGTTOU"},
        {SIGUSR1, "SIGUSR1"}, {SIGUSR2, "SIGUSR2"},
#else
        {SIGBREAK, "SIGBREAK"},
#endif
    };

    auto it = signalNames.find(signal);
    if (it != signalNames.end()) {
        return it->second;
    }

    return "SIG" + std::to_string(signal);
}

/**
 * @brief Temporarily block a signal during a critical section
 *
 * @param signal Signal to block
 * @param function Function to execute while signal is blocked
 */
template <typename Func>
inline void withBlockedSignal(int signal, Func function) {
#if !defined(_WIN32) && !defined(_WIN64)
    sigset_t blockSet, oldSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, signal);

    // Block the signal
    sigprocmask(SIG_BLOCK, &blockSet, &oldSet);

    try {
        // Execute the function
        function();
    } catch (...) {
        // Restore the signal mask before re-throwing
        sigprocmask(SIG_SETMASK, &oldSet, nullptr);
        throw;
    }

    // Restore the signal mask
    sigprocmask(SIG_SETMASK, &oldSet, nullptr);
#else
    // Windows doesn't have proper signal blocking, just execute the function
    function();
#endif
}

#endif  // SIGNAL_UTILS_HPP
