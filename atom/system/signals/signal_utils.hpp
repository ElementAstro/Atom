#ifndef SIGNAL_UTILS_HPP
#define SIGNAL_UTILS_HPP

#include "signal.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>

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
    explicit ScopedSignalHandler(SignalID signal, const SignalHandler& handler,
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
     * @brief 支持移动构造的函数参数，避免复制
     */
    template <typename F, typename = std::enable_if_t<
                              std::is_invocable_r_v<void, F, SignalID>>>
    explicit ScopedSignalHandler(SignalID signal, F&& handler, int priority = 0,
                                 bool useSafeManager = true)
        : signal_(signal), handlerId_(-1), useSafeManager_(useSafeManager) {
        if (useSafeManager_) {
            handlerId_ = SafeSignalManager::getInstance().addSafeSignalHandler(
                signal, std::forward<F>(handler), priority);
        } else {
            handlerId_ = SignalHandlerRegistry::getInstance().setSignalHandler(
                signal, std::forward<F>(handler), priority);
        }
    }

    /**
     * @brief Destructor removes the signal handler
     */
    ~ScopedSignalHandler() noexcept { removeHandler(); }

    /**
     * @brief 显式移除处理器
     * @return true 移除成功，false 移除失败或已移除
     */
    bool removeHandler() noexcept {
        if (handlerId_ >= 0) {
            bool success = false;
            if (useSafeManager_) {
                success = SafeSignalManager::getInstance()
                              .removeSafeSignalHandlerById(handlerId_);
            } else {
                success = SignalHandlerRegistry::getInstance()
                              .removeSignalHandlerById(handlerId_);
            }
            handlerId_ = -1;  // Mark as removed even if removal failed
            return success;
        }
        return false;
    }

    // Prevent copying
    ScopedSignalHandler(const ScopedSignalHandler&) = delete;
    ScopedSignalHandler& operator=(const ScopedSignalHandler&) = delete;

    // Allow moving with noexcept保证
    ScopedSignalHandler(ScopedSignalHandler&& other) noexcept
        : signal_(other.signal_),
          handlerId_(other.handlerId_),
          useSafeManager_(other.useSafeManager_) {
        other.handlerId_ = -1;  // Prevent the other from removing the handler
    }

    ScopedSignalHandler& operator=(ScopedSignalHandler&& other) noexcept {
        if (this != &other) {
            // Clean up existing handler
            removeHandler();

            // Move from other
            signal_ = other.signal_;
            handlerId_ = other.handlerId_;
            useSafeManager_ = other.useSafeManager_;

            other.handlerId_ =
                -1;  // Prevent the other from removing the handler
        }
        return *this;
    }

    /**
     * @brief 获取处理器ID
     * @return 处理器ID，-1表示无效或已移除
     */
    [[nodiscard]] int getHandlerId() const noexcept { return handlerId_; }

    /**
     * @brief 检查处理器是否有效
     * @return true 有效，false 无效或已移除
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return handlerId_ >= 0;
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
    explicit SignalGroup(std::string_view groupName = "",
                         bool useSafeManager = true)
        : groupName_(groupName), useSafeManager_(useSafeManager) {}

    /**
     * @brief Destructor removes all registered handlers
     */
    ~SignalGroup() noexcept { [[maybe_unused]] int removedCount = removeAll(); }

    SignalGroup(const SignalGroup&) = delete;
    SignalGroup& operator=(const SignalGroup&) = delete;

    SignalGroup(SignalGroup&& other) noexcept
        : groupName_(std::move(other.groupName_)),
          useSafeManager_(other.useSafeManager_),
          handlerIds_(std::move(other.handlerIds_)) {
        other.handlerIds_.clear();
    }

    SignalGroup& operator=(SignalGroup&& other) noexcept {
        if (this != &other) {
            [[maybe_unused]] int removedCount = removeAll();

            groupName_ = std::move(other.groupName_);
            useSafeManager_ = other.useSafeManager_;
            handlerIds_ = std::move(other.handlerIds_);

            other.handlerIds_.clear();
        }
        return *this;
    }

    /**
     * @brief Add a handler to the group
     *
     * @param signal The signal to handle
     * @param handler The handler function
     * @param priority Priority of the handler (higher values = higher priority)
     * @return int ID of the registered handler
     */
    template <typename F, typename = std::enable_if_t<
                              std::is_invocable_r_v<void, F, SignalID>>>
    [[nodiscard]] int addHandler(SignalID signal, F&& handler,
                                 int priority = 0) {
        int handlerId;
        if (useSafeManager_) {
            handlerId = SafeSignalManager::getInstance().addSafeSignalHandler(
                signal, std::forward<F>(handler), priority, groupName_);
        } else {
            handlerId = SignalHandlerRegistry::getInstance().setSignalHandler(
                signal, std::forward<F>(handler), priority, groupName_);
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
    [[nodiscard]] bool removeHandler(int handlerId) noexcept {
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
    [[nodiscard]] int removeSignalHandlers(SignalID signal) noexcept {
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
    [[nodiscard]] int removeAll() noexcept {
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
    [[nodiscard]] const std::map<SignalID, std::vector<int>>& getHandlerIds()
        const noexcept {
        return handlerIds_;
    }

    /**
     * @brief Get the group name
     *
     * @return const std::string& Group name
     */
    [[nodiscard]] const std::string& getGroupName() const noexcept {
        return groupName_;
    }

    /**
     * @brief 检查组是否为空
     * @return true 组为空，false 组不为空
     */
    [[nodiscard]] bool empty() const noexcept { return handlerIds_.empty(); }

    /**
     * @brief 获取组内处理器数量
     * @return 组内处理器数量
     */
    [[nodiscard]] size_t size() const noexcept {
        size_t count = 0;
        for (const auto& [signal, ids] : handlerIds_) {
            count += ids.size();
        }
        return count;
    }

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
[[nodiscard]] inline std::shared_ptr<SignalGroup> makeSignalGroup(
    std::string_view groupName = "", bool useSafeManager = true) {
    return std::make_shared<SignalGroup>(groupName, useSafeManager);
}

/**
 * @brief Get the signal name as a string
 *
 * @param signal Signal ID
 * @return std::string Name of the signal
 */
[[nodiscard]] inline std::string getSignalName(int signal) {
    // 使用静态变量避免重复创建，提高性能
    static const std::map<int, std::string_view> signalNames = {
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
        return std::string(it->second);
    }

    return "SIG" + std::to_string(signal);
}

/**
 * @brief C++17 的模板函数自动推导参数类型，无需显式指定
 * Temporarily block a signal during a critical section
 *
 * @param signal Signal to block
 * @param function Function to execute while signal is blocked
 */
template <typename Func>
inline void withBlockedSignal([[maybe_unused]] int signal, Func&& function) {
#if !defined(_WIN32) && !defined(_WIN64)
    sigset_t blockSet, oldSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, signal);

    // Block the signal
    if (sigprocmask(SIG_BLOCK, &blockSet, &oldSet) != 0) {
        throw std::runtime_error("Failed to block signal");
    }

    try {
        // Execute the function
        std::forward<Func>(function)();
    } catch (...) {
        // Restore the signal mask before re-throwing
        sigprocmask(SIG_SETMASK, &oldSet, nullptr);
        throw;
    }

    // Restore the signal mask
    if (sigprocmask(SIG_SETMASK, &oldSet, nullptr) != 0) {
        throw std::runtime_error("Failed to restore signal mask");
    }
#else
    // Windows doesn't have proper signal blocking, just execute the function
    std::forward<Func>(function)();
#endif
}

/**
 * @brief RAII类型的信号阻塞，无需手动恢复
 */
class ScopedSignalBlocker {
public:
    /**
     * @brief 构造时阻塞信号
     *
     * @param signal 要阻塞的信号
     */
    explicit ScopedSignalBlocker(int signal)
        : signal_(signal), blocked_(false) {
#if !defined(_WIN32) && !defined(_WIN64)
        sigemptyset(&blockSet_);
        sigaddset(&blockSet_, signal);
        blocked_ = (sigprocmask(SIG_BLOCK, &blockSet_, &oldSet_) == 0);
#endif
    }

    /**
     * @brief 析构时恢复信号
     */
    ~ScopedSignalBlocker() noexcept {
#if !defined(_WIN32) && !defined(_WIN64)
        if (blocked_) {
            sigprocmask(SIG_SETMASK, &oldSet_, nullptr);
        }
#endif
    }

    // 不允许复制
    ScopedSignalBlocker(const ScopedSignalBlocker&) = delete;
    ScopedSignalBlocker& operator=(const ScopedSignalBlocker&) = delete;

    // 允许移动
    ScopedSignalBlocker(ScopedSignalBlocker&& other) noexcept
        : signal_(other.signal_),
          blocked_(other.blocked_)
#if !defined(_WIN32) && !defined(_WIN64)
          ,
          blockSet_(other.blockSet_),
          oldSet_(other.oldSet_)
#endif
    {
        other.blocked_ = false;
    }

    ScopedSignalBlocker& operator=(ScopedSignalBlocker&& other) noexcept {
        if (this != &other) {
            // 恢复当前对象的信号
#if !defined(_WIN32) && !defined(_WIN64)
            if (blocked_) {
                sigprocmask(SIG_SETMASK, &oldSet_, nullptr);
            }
#endif

            // 移动数据
            signal_ = other.signal_;
            blocked_ = other.blocked_;
#if !defined(_WIN32) && !defined(_WIN64)
            blockSet_ = other.blockSet_;
            oldSet_ = other.oldSet_;
#endif

            // 防止源对象恢复信号
            other.blocked_ = false;
        }
        return *this;
    }

    /**
     * @brief 检查信号是否成功被阻塞
     * @return true 阻塞成功，false 阻塞失败
     */
    [[nodiscard]] bool isBlocked() const noexcept { return blocked_; }

private:
    int signal_;
    bool blocked_;
#if !defined(_WIN32) && !defined(_WIN64)
    sigset_t blockSet_;
    sigset_t oldSet_;
#endif
};

/**
 * @brief 创建一个阻塞多个信号的RAII对象
 */
class ScopedMultiSignalBlocker {
public:
    /**
     * @brief 构造时阻塞多个信号
     *
     * @param signals 要阻塞的信号列表
     */
    explicit ScopedMultiSignalBlocker(std::initializer_list<int> signals)
        : blocked_(false) {
#if !defined(_WIN32) && !defined(_WIN64)
        sigemptyset(&blockSet_);
        for (int signal : signals) {
            sigaddset(&blockSet_, signal);
        }
        blocked_ = (sigprocmask(SIG_BLOCK, &blockSet_, &oldSet_) == 0);
#else
        (void)signals;  // 避免未使用变量警告
#endif
    }

    /**
     * @brief 析构时恢复信号
     */
    ~ScopedMultiSignalBlocker() noexcept {
#if !defined(_WIN32) && !defined(_WIN64)
        if (blocked_) {
            sigprocmask(SIG_SETMASK, &oldSet_, nullptr);
        }
#endif
    }

    // 不允许复制
    ScopedMultiSignalBlocker(const ScopedMultiSignalBlocker&) = delete;
    ScopedMultiSignalBlocker& operator=(const ScopedMultiSignalBlocker&) =
        delete;

    // 允许移动
    ScopedMultiSignalBlocker(ScopedMultiSignalBlocker&& other) noexcept
        : blocked_(other.blocked_)
#if !defined(_WIN32) && !defined(_WIN64)
          ,
          blockSet_(other.blockSet_),
          oldSet_(other.oldSet_)
#endif
    {
        other.blocked_ = false;
    }

    ScopedMultiSignalBlocker& operator=(
        ScopedMultiSignalBlocker&& other) noexcept {
        if (this != &other) {
            // 恢复当前对象的信号
#if !defined(_WIN32) && !defined(_WIN64)
            if (blocked_) {
                sigprocmask(SIG_SETMASK, &oldSet_, nullptr);
            }
#endif

            // 移动数据
            blocked_ = other.blocked_;
#if !defined(_WIN32) && !defined(_WIN64)
            blockSet_ = other.blockSet_;
            oldSet_ = other.oldSet_;
#endif

            // 防止源对象恢复信号
            other.blocked_ = false;
        }
        return *this;
    }

    /**
     * @brief 检查信号是否成功被阻塞
     * @return true 阻塞成功，false 阻塞失败
     */
    [[nodiscard]] bool isBlocked() const noexcept { return blocked_; }

private:
    bool blocked_;
#if !defined(_WIN32) && !defined(_WIN64)
    sigset_t blockSet_;
    sigset_t oldSet_;
#endif
};

#endif  // SIGNAL_UTILS_HPP
