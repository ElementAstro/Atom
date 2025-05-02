#include "signal.hpp"

#include <algorithm>
#include <condition_variable>
#include <csignal>
#include <future>
#include <mutex>
#include <optional>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
constexpr size_t DEFAULT_QUEUE_SIZE = 1000;
#endif

#include "atom/log/loguru.hpp"

constexpr int SLEEP_DURATION_MS = 10;

auto SignalHandlerWithPriority::operator<(
    const SignalHandlerWithPriority& other) const noexcept -> bool {
    return priority > other.priority;  // Higher priority handlers run first
}

auto SignalHandlerRegistry::getInstance() -> SignalHandlerRegistry& {
    static SignalHandlerRegistry instance;
    return instance;
}

int SignalHandlerRegistry::setSignalHandler(SignalID signal,
                                            const SignalHandler& handler,
                                            int priority,
                                            std::string_view handlerName) {
    std::unique_lock lock(mutex_);
    int handlerId = nextHandlerId_++;
    handlers_[signal].emplace(handler, priority, handlerName);
    handlerRegistry_[handlerId] = {signal, handler};

    // Update statistics - 使用插入或赋值确保始终存在，避免竞争条件
    signalStats_.try_emplace(signal);

    auto previousHandler = std::signal(signal, signalDispatcher);
    if (previousHandler == SIG_ERR) {
        LOG_F(ERROR, "Error setting signal handler for signal {}", signal);
    }

    return handlerId;
}

bool SignalHandlerRegistry::removeSignalHandlerById(int handlerId) {
    std::unique_lock lock(mutex_);
    auto it = handlerRegistry_.find(handlerId);
    if (it == handlerRegistry_.end()) {
        return false;
    }

    SignalID signal = it->second.first;
    const SignalHandler& handler = it->second.second;

    auto handlerIterator = handlers_.find(signal);
    if (handlerIterator != handlers_.end()) {
        auto handlerWithPriority = std::find_if(
            handlerIterator->second.begin(), handlerIterator->second.end(),
            [&handler](const SignalHandlerWithPriority& handlerPriority) {
                return handlerPriority.handler.target<void(SignalID)>() ==
                       handler.target<void(SignalID)>();
            });
        if (handlerWithPriority != handlerIterator->second.end()) {
            handlerIterator->second.erase(handlerWithPriority);
            handlerRegistry_.erase(handlerId);

            if (handlerIterator->second.empty()) {
                handlers_.erase(handlerIterator);
                auto previousHandler = std::signal(signal, SIG_DFL);
                if (previousHandler == SIG_ERR) {
                    LOG_F(ERROR, "Error resetting signal handler for signal {}",
                          signal);
                }
            }
            return true;
        }
    }

    // Handler not found in set, still remove from registry
    handlerRegistry_.erase(handlerId);
    return false;
}

bool SignalHandlerRegistry::removeSignalHandler(SignalID signal,
                                                const SignalHandler& handler) {
    std::unique_lock lock(mutex_);
    auto handlerIterator = handlers_.find(signal);
    if (handlerIterator != handlers_.end()) {
        auto handlerWithPriority = std::find_if(
            handlerIterator->second.begin(), handlerIterator->second.end(),
            [&handler](const SignalHandlerWithPriority& handlerPriority) {
                return handlerPriority.handler.target<void(SignalID)>() ==
                       handler.target<void(SignalID)>();
            });
        if (handlerWithPriority != handlerIterator->second.end()) {
            // Also remove from handler registry
            for (auto it = handlerRegistry_.begin();
                 it != handlerRegistry_.end();) {
                if (it->second.first == signal &&
                    it->second.second.target<void(SignalID)>() ==
                        handler.target<void(SignalID)>()) {
                    it = handlerRegistry_.erase(it);
                } else {
                    ++it;
                }
            }

            handlerIterator->second.erase(handlerWithPriority);
            if (handlerIterator->second.empty()) {
                handlers_.erase(handlerIterator);
                auto previousHandler = std::signal(signal, SIG_DFL);
                if (previousHandler == SIG_ERR) {
                    LOG_F(ERROR, "Error resetting signal handler for signal {}",
                          signal);
                }
            }
            return true;
        }
    }
    return false;
}

std::vector<int> SignalHandlerRegistry::setStandardCrashHandlerSignals(
    const SignalHandler& handler, int priority, std::string_view handlerName) {
    std::vector<int> handlerIds;
    handlerIds.reserve(getStandardCrashSignals().size());

    for (SignalID signal : getStandardCrashSignals()) {
        handlerIds.push_back(
            setSignalHandler(signal, handler, priority, handlerName));
    }
    return handlerIds;
}

int SignalHandlerRegistry::processAllPendingSignals(
    std::chrono::milliseconds timeout) {
    const auto startTime = std::chrono::steady_clock::now();
    int processed = 0;

    // Process all signals that have registered handlers
    for (const auto& [signal, handlers] : handlers_) {
        if (hasHandlersForSignal(signal)) {
            // Check timeout if specified
            if (timeout.count() > 0) {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed >= timeout) {
                    LOG_F(INFO,
                          "Signal processing timeout reached after processing "
                          "{} signals",
                          processed);
                    break;
                }
            }

            try {
                // Execute all handlers for this signal
                for (const auto& handler : handlers) {
                    if (executeHandlerWithTimeout(handler.handler, signal)) {
                        processed++;
                        // Update statistics
                        auto it = signalStats_.find(signal);
                        if (it != signalStats_.end()) {
                            it->second.processed.fetch_add(
                                1, std::memory_order_relaxed);
                            it->second.lastProcessed =
                                std::chrono::steady_clock::now();
                        }
                    } else {
                        LOG_F(WARNING,
                              "Handler timed out while processing signal {}",
                              signal);
                        auto it = signalStats_.find(signal);
                        if (it != signalStats_.end()) {
                            it->second.handlerErrors.fetch_add(
                                1, std::memory_order_relaxed);
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in signal handler for signal {}: {}",
                      signal, e.what());
                auto it = signalStats_.find(signal);
                if (it != signalStats_.end()) {
                    it->second.handlerErrors.fetch_add(
                        1, std::memory_order_relaxed);
                }
            }
        }
    }

    return processed;
}

bool SignalHandlerRegistry::hasHandlersForSignal(SignalID signal) {
    std::shared_lock lock(mutex_);
    auto it = handlers_.find(signal);
    return it != handlers_.end() && !it->second.empty();
}

const SignalStats& SignalHandlerRegistry::getSignalStats(
    SignalID signal) const {
    std::shared_lock lock(mutex_);
    static const SignalStats emptyStats;
    auto it = signalStats_.find(signal);
    if (it != signalStats_.end()) {
        return it->second;
    }
    return emptyStats;
}

void SignalHandlerRegistry::resetStats(SignalID signal) {
    std::unique_lock lock(mutex_);
    if (signal == -1) {
        for (auto& [sig, stats] : signalStats_) {
            signalStats_.try_emplace(sig);
        }
    } else {
        signalStats_.try_emplace(signal);
    }
}

void SignalHandlerRegistry::setHandlerTimeout(
    std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    handlerTimeout_ = timeout;
}

bool SignalHandlerRegistry::executeHandlerWithTimeout(
    const SignalHandler& handler, SignalID signal) {
    if (handlerTimeout_.count() <= 0) {
        handler(signal);
        return true;
    }

    auto futureTask = std::async(std::launch::async,
                                 [&handler, signal]() { handler(signal); });

    auto status = futureTask.wait_for(handlerTimeout_);
    return status == std::future_status::ready;
}

SignalHandlerRegistry::SignalHandlerRegistry() = default;

SignalHandlerRegistry::~SignalHandlerRegistry() = default;

void SignalHandlerRegistry::signalDispatcher(int signal) {
    SignalHandlerRegistry& registry = getInstance();
    {
        std::shared_lock lock(registry.mutex_);
        auto it = registry.signalStats_.find(signal);
        if (it != registry.signalStats_.end()) {
            it->second.received.fetch_add(1, std::memory_order_relaxed);
            it->second.lastReceived = std::chrono::steady_clock::now();
        } else {
            // 需要升级锁才能修改集合
            lock.unlock();
            std::unique_lock writeLock(registry.mutex_);
            auto& stats = registry.signalStats_[signal];
            stats.received.store(1, std::memory_order_relaxed);
            stats.lastReceived = std::chrono::steady_clock::now();
        }
    }

    // Forward to safe manager if available
    SafeSignalManager::safeSignalDispatcher(signal);

    // Immediate handling for critical signals
    std::shared_lock lock(registry.mutex_);
    auto handlerIterator = registry.handlers_.find(signal);
    if (handlerIterator != registry.handlers_.end()) {
        for (const auto& handler : handlerIterator->second) {
            try {
                handler.handler(signal);

                auto statsIt = registry.signalStats_.find(signal);
                if (statsIt != registry.signalStats_.end()) {
                    statsIt->second.processed.fetch_add(
                        1, std::memory_order_relaxed);
                    statsIt->second.lastProcessed =
                        std::chrono::steady_clock::now();
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR,
                      "Exception in direct signal handler for signal {}: {}",
                      signal, e.what());

                auto statsIt = registry.signalStats_.find(signal);
                if (statsIt != registry.signalStats_.end()) {
                    statsIt->second.handlerErrors.fetch_add(
                        1, std::memory_order_relaxed);
                }
            }
        }
    }
}

auto SignalHandlerRegistry::getStandardCrashSignals() -> std::set<SignalID> {
#if defined(_WIN32) || defined(_WIN64)
    return {SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGTERM};
#else
    return {SIGABRT, SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGQUIT};
#endif
}

SafeSignalManager::SafeSignalManager(size_t threadCount, size_t queueSize)
    : maxQueueSize_(queueSize) {
    // 启动工作线程前先配置队列大小
#ifdef ATOM_USE_BOOST
    // 使用Boost的无锁队列不需要额外操作
#else
    // signalQueue_.reserve(queueSize);
#endif

    workerThreads_.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        workerThreads_.emplace_back([this](std::stop_token stopToken) {
            this->processSignals(stopToken);
        });
    }

    LOG_F(INFO,
          "SafeSignalManager initialized with {} worker threads and queue size "
          "{}",
          threadCount, queueSize);
}

SafeSignalManager::~SafeSignalManager() {
    keepRunning_ = false;
    clearSignalQueue();  // Clear queue to avoid deadlocks

    // Wake up any sleeping threads - C++20 jthread自动处理停止和join
#ifndef ATOM_USE_BOOST
    queueCondition_.notify_all();
#endif

    LOG_F(INFO, "SafeSignalManager shutting down");
}

int SafeSignalManager::addSafeSignalHandler(SignalID signal,
                                            const SignalHandler& handler,
                                            int priority,
                                            std::string_view handlerName) {
    std::unique_lock lock(queueMutex_);
    int handlerId = nextHandlerId_++;
    safeHandlers_[signal].emplace(handler, priority, handlerName);
    handlerRegistry_[handlerId] = {signal, handler};

    // Update statistics
    std::unique_lock statsLock(statsMutex_);
    signalStats_.try_emplace(signal, SignalStats{});

    LOG_F(INFO,
          "Added safe signal handler for signal {} with priority {} and ID {}",
          signal, priority, handlerId);

    return handlerId;
}

bool SafeSignalManager::removeSafeSignalHandlerById(int handlerId) {
    std::unique_lock lock(queueMutex_);
    auto it = handlerRegistry_.find(handlerId);
    if (it == handlerRegistry_.end()) {
        return false;
    }

    SignalID signal = it->second.first;
    const SignalHandler& handler = it->second.second;

    auto handlerIterator = safeHandlers_.find(signal);
    if (handlerIterator != safeHandlers_.end()) {
        auto handlerWithPriority = std::find_if(
            handlerIterator->second.begin(), handlerIterator->second.end(),
            [&handler](const SignalHandlerWithPriority& handlerPriority) {
                return handlerPriority.handler.target<void(SignalID)>() ==
                       handler.target<void(SignalID)>();
            });
        if (handlerWithPriority != handlerIterator->second.end()) {
            handlerIterator->second.erase(handlerWithPriority);
            handlerRegistry_.erase(handlerId);

            if (handlerIterator->second.empty()) {
                safeHandlers_.erase(handlerIterator);
            }

            LOG_F(INFO, "Removed safe signal handler with ID {}", handlerId);
            return true;
        }
    }

    // Handler not found in set, still remove from registry
    handlerRegistry_.erase(handlerId);
    return false;
}

bool SafeSignalManager::removeSafeSignalHandler(SignalID signal,
                                                const SignalHandler& handler) {
    std::unique_lock lock(queueMutex_);
    auto handlerIterator = safeHandlers_.find(signal);
    if (handlerIterator != safeHandlers_.end()) {
        auto handlerWithPriority = std::find_if(
            handlerIterator->second.begin(), handlerIterator->second.end(),
            [&handler](const SignalHandlerWithPriority& handlerPriority) {
                return handlerPriority.handler.target<void(SignalID)>() ==
                       handler.target<void(SignalID)>();
            });
        if (handlerWithPriority != handlerIterator->second.end()) {
            // Also remove from handler registry
            for (auto it = handlerRegistry_.begin();
                 it != handlerRegistry_.end();) {
                if (it->second.first == signal &&
                    it->second.second.target<void(SignalID)>() ==
                        handler.target<void(SignalID)>()) {
                    it = handlerRegistry_.erase(it);
                } else {
                    ++it;
                }
            }

            handlerIterator->second.erase(handlerWithPriority);
            if (handlerIterator->second.empty()) {
                safeHandlers_.erase(handlerIterator);
            }

            LOG_F(INFO, "Removed safe signal handler for signal {}", signal);
            return true;
        }
    }
    return false;
}

void SafeSignalManager::safeSignalDispatcher(int signal) {
    auto& manager = getInstance();
    manager.queueSignal(signal);
}

auto SafeSignalManager::getInstance() -> SafeSignalManager& {
    static SafeSignalManager instance;
    return instance;
}

bool SafeSignalManager::queueSignal(SignalID signal) {
#ifdef ATOM_USE_BOOST
    {
        std::shared_lock statsLock(statsMutex_);
        auto it = signalStats_.find(signal);
        if (it != signalStats_.end()) {
            it->second.received.fetch_add(1, std::memory_order_relaxed);
            it->second.lastReceived = std::chrono::steady_clock::now();
        } else {
            // 需要升级锁才能修改集合
            statsLock.unlock();
            std::unique_lock writeLock(statsMutex_);
            auto& stats = signalStats_[signal];
            stats.received.store(1, std::memory_order_relaxed);
            stats.lastReceived = std::chrono::steady_clock::now();
        }
    }

    bool pushed = signalQueue_.push(signal);
    if (!pushed) {
        std::unique_lock statsLock(statsMutex_);
        signalStats_[signal].dropped.fetch_add(1, std::memory_order_relaxed);
        LOG_F(WARNING, "Signal queue full, dropping signal {}", signal);
    }
    return pushed;
#else
    std::unique_lock lock(queueMutex_);

    {
        std::shared_lock statsLock(statsMutex_);
        auto it = signalStats_.find(signal);
        if (it != signalStats_.end()) {
            it->second.received.fetch_add(1, std::memory_order_relaxed);
            it->second.lastReceived = std::chrono::steady_clock::now();
        } else {
            statsLock.unlock();
            std::unique_lock writeLock(statsMutex_);
            signalStats_.try_emplace(signal, SignalStats{});
            auto& stats = signalStats_[signal];
            stats.received.store(1, std::memory_order_relaxed);
            stats.lastReceived = std::chrono::steady_clock::now();
        }
    }

    if (signalQueue_.size() >= maxQueueSize_) {
        std::unique_lock statsLock(statsMutex_);
        signalStats_[signal].dropped.fetch_add(1, std::memory_order_relaxed);
        LOG_F(WARNING, "Signal queue full, dropping signal {}", signal);
        return false;
    }

    signalQueue_.push_back(signal);
    queueCondition_.notify_one();
    return true;
#endif
}

size_t SafeSignalManager::getQueueSize() const {
#ifdef ATOM_USE_BOOST
    return signalQueue_.read_available();
#else
    std::shared_lock lock(queueMutex_);
    return signalQueue_.size();
#endif
}

const SignalStats& SafeSignalManager::getSignalStats(SignalID signal) const {
    std::shared_lock lock(statsMutex_);
    static const SignalStats emptyStats;
    auto it = signalStats_.find(signal);
    if (it != signalStats_.end()) {
        return it->second;
    }
    return emptyStats;
}

void SafeSignalManager::resetStats(SignalID signal) {
    std::unique_lock lock(statsMutex_);
    if (signal == -1) {
        for (auto& [sig, stats] : signalStats_) {
            signalStats_.try_emplace(sig);
        }
    } else {
        signalStats_.try_emplace(signal);
    }
}

bool SafeSignalManager::setWorkerThreadCount(size_t threadCount) {
    if (!keepRunning_) {
        return false;
    }

    keepRunning_ = false;
#ifndef ATOM_USE_BOOST
    queueCondition_.notify_all();
#endif
    workerThreads_.clear();

    keepRunning_ = true;
    workerThreads_.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        workerThreads_.emplace_back([this](std::stop_token stopToken) {
            this->processSignals(stopToken);
        });
    }

    LOG_F(INFO, "Changed worker thread count to {}", threadCount);
    return true;
}

void SafeSignalManager::setMaxQueueSize(size_t size) {
#ifdef ATOM_USE_BOOST
    LOG_F(WARNING, "Cannot change queue size for Boost lockfree queue");
#else
    std::unique_lock lock(queueMutex_);
    maxQueueSize_ = size;
    LOG_F(INFO, "Changed maximum queue size to {}", size);
#endif
}

int SafeSignalManager::clearSignalQueue() {
#ifdef ATOM_USE_BOOST
    int cleared = 0;
    int signal;
    while (signalQueue_.pop(signal)) {
        cleared++;
    }
    LOG_F(INFO, "Cleared signal queue, removed approximately {} signals",
          cleared);
    return cleared;
#else
    std::unique_lock lock(queueMutex_);
    int cleared = static_cast<int>(signalQueue_.size());
    signalQueue_.clear();
    LOG_F(INFO, "Cleared signal queue, removed {} signals", cleared);
    return cleared;
#endif
}

void SafeSignalManager::processSignals(std::stop_token stopToken) {
    while (!stopToken.stop_requested() && keepRunning_) {
        std::optional<int> signal;

#ifdef ATOM_USE_BOOST
        int signalValue;
        if (signalQueue_.pop(signalValue)) {
            signal = signalValue;
        } else {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SLEEP_DURATION_MS));
            continue;
        }
#else
        {
            std::unique_lock lock(queueMutex_);
            queueCondition_.wait(lock, [this, &stopToken] {
                return !signalQueue_.empty() || stopToken.stop_requested() ||
                       !keepRunning_;
            });

            if (stopToken.stop_requested() || !keepRunning_) {
                break;
            }

            if (!signalQueue_.empty()) {
                signal = signalQueue_.front();
                signalQueue_.pop_front();
            }
        }
#endif

        if (signal) {
            std::shared_lock lock(queueMutex_);
            auto handlerIterator = safeHandlers_.find(signal.value());
            if (handlerIterator != safeHandlers_.end()) {
                for (const auto& handler : handlerIterator->second) {
                    try {
                        LOG_F(INFO, "Processing signal {} with handler {}",
                              signal.value(),
                              handler.name.empty() ? "unnamed" : handler.name);
                        handler.handler(signal.value());

                        std::shared_lock statsLock(statsMutex_);
                        auto statsIt = signalStats_.find(signal.value());
                        if (statsIt != signalStats_.end()) {
                            statsIt->second.processed.fetch_add(
                                1, std::memory_order_relaxed);
                            statsIt->second.lastProcessed =
                                std::chrono::steady_clock::now();
                        }
                    } catch (const std::exception& e) {
                        LOG_F(ERROR,
                              "Exception in safe signal handler for signal {}: "
                              "{}",
                              signal.value(), e.what());

                        std::shared_lock statsLock(statsMutex_);
                        auto statsIt = signalStats_.find(signal.value());
                        if (statsIt != signalStats_.end()) {
                            statsIt->second.handlerErrors.fetch_add(
                                1, std::memory_order_relaxed);
                        }
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(SLEEP_DURATION_MS));
        }
    }
}

void installPlatformSpecificHandlers() {
#if defined(_WIN32) || defined(_WIN64)
    [[maybe_unused]] auto windowsHandlerIds =
        SignalHandlerRegistry::getInstance().setStandardCrashHandlerSignals(
            [](int signal) {
                LOG_F(ERROR, "Caught signal {} on Windows", signal);
            },
            100, "PlatformCrashHandler-Windows");

    [[maybe_unused]] auto sigbreakHandlerId =
        SafeSignalManager::getInstance().addSafeSignalHandler(
            SIGBREAK,
            []([[maybe_unused]] int signal) {
                LOG_F(WARNING, "Caught SIGBREAK on Windows");
            },
            90, "Windows-SIGBREAK-Handler");

#else
    [[maybe_unused]] auto posixHandlerIds =
        SignalHandlerRegistry::getInstance().setStandardCrashHandlerSignals(
            [](int signal) {
                LOG_F(ERROR, "Caught signal {} on POSIX system", signal);
            },
            100, "PlatformCrashHandler-POSIX");

    [[maybe_unused]] auto sighupHandlerId =
        SafeSignalManager::getInstance().addSafeSignalHandler(
            SIGHUP,
            [](int signal) {
                LOG_F(INFO, "Caught SIGHUP - reloading configuration");
            },
            80, "POSIX-SIGHUP-Handler");

    [[maybe_unused]] auto sigusr1HandlerId =
        SafeSignalManager::getInstance().addSafeSignalHandler(
            SIGUSR1,
            [](int signal) { LOG_F(INFO, "Caught SIGUSR1 - custom action"); },
            80, "POSIX-SIGUSR1-Handler");
#endif

    // 两个平台共用的处理器
    [[maybe_unused]] auto sigtermHandlerId =
        SafeSignalManager::getInstance().addSafeSignalHandler(
            SIGTERM,
            []([[maybe_unused]] int signal) {
                LOG_F(WARNING, "Caught SIGTERM - preparing for shutdown");
            },
            100, "Common-SIGTERM-Handler");
}

void initializeSignalSystem(size_t workerThreadCount, size_t queueSize) {
    // 使用指定参数初始化安全信号管理器
    auto& manager = SafeSignalManager::getInstance();

    // 配置工作线程和队列大小
    manager.setWorkerThreadCount(workerThreadCount);
    manager.setMaxQueueSize(queueSize);

    // 在注册表中设置处理器超时
    SignalHandlerRegistry::getInstance().setHandlerTimeout(
        std::chrono::milliseconds(2000));

    // 安装平台特定处理器
    installPlatformSpecificHandlers();

    LOG_F(INFO,
          "Signal system initialized with {} worker threads and queue size {}",
          workerThreadCount, queueSize);
}
