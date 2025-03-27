#include "signal.hpp"
#include <algorithm>
#include <csignal>
#include <iostream>
#include <sstream>
#include <future>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "atom/log/loguru.hpp"

constexpr int SLEEP_DURATION_MS = 10;
constexpr size_t DEFAULT_QUEUE_SIZE = 1000;

auto SignalHandlerWithPriority::operator<(
    const SignalHandlerWithPriority& other) const -> bool {
    return priority > other.priority;  // Higher priority handlers run first
}

auto SignalHandlerRegistry::getInstance() -> SignalHandlerRegistry& {
    static SignalHandlerRegistry instance;
    return instance;
}

int SignalHandlerRegistry::setSignalHandler(SignalID signal,
                                          const SignalHandler& handler,
                                          int priority,
                                          const std::string& handlerName) {
    std::lock_guard lock(mutex_);
    int handlerId = nextHandlerId_++;
    handlers_[signal].emplace(handler, priority, handlerName);
    handlerRegistry_[handlerId] = {signal, handler};
    
    // Update statistics
    if (signalStats_.find(signal) == signalStats_.end()) {
        signalStats_.try_emplace(signal);
    }
    
    auto previousHandler = std::signal(signal, signalDispatcher);
    if (previousHandler == SIG_ERR) {
        LOG_F(ERROR, "Error setting signal handler for signal {}", signal);
    }
    
    return handlerId;
}

bool SignalHandlerRegistry::removeSignalHandlerById(int handlerId) {
    std::lock_guard lock(mutex_);
    auto it = handlerRegistry_.find(handlerId);
    if (it == handlerRegistry_.end()) {
        return false;
    }
    
    SignalID signal = it->second.first;
    const SignalHandler& handler = it->second.second;
    
    auto handlerIterator = handlers_.find(signal);
    if (handlerIterator != handlers_.end()) {
        auto handlerWithPriority =
            std::find_if(handlerIterator->second.begin(), handlerIterator->second.end(),
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
                    LOG_F(ERROR, "Error resetting signal handler for signal {}", signal);
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
    std::lock_guard lock(mutex_);
    auto handlerIterator = handlers_.find(signal);
    if (handlerIterator != handlers_.end()) {
        auto handlerWithPriority =
            std::find_if(handlerIterator->second.begin(), handlerIterator->second.end(),
                         [&handler](const SignalHandlerWithPriority& handlerPriority) {
                             return handlerPriority.handler.target<void(SignalID)>() ==
                                    handler.target<void(SignalID)>();
                         });
        if (handlerWithPriority != handlerIterator->second.end()) {
            // Also remove from handler registry
            for (auto it = handlerRegistry_.begin(); it != handlerRegistry_.end(); ) {
                if (it->second.first == signal && 
                    it->second.second.target<void(SignalID)>() == handler.target<void(SignalID)>()) {
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
                    LOG_F(ERROR, "Error resetting signal handler for signal {}", signal);
                }
            }
            return true;
        }
    }
    return false;
}

std::vector<int> SignalHandlerRegistry::setStandardCrashHandlerSignals(
    const SignalHandler& handler, int priority, const std::string& handlerName) {
    std::vector<int> handlerIds;
    for (SignalID signal : getStandardCrashSignals()) {
        handlerIds.push_back(setSignalHandler(signal, handler, priority, handlerName));
    }
    return handlerIds;
}

int SignalHandlerRegistry::processAllPendingSignals(std::chrono::milliseconds timeout) {
    const auto startTime = std::chrono::steady_clock::now();
    int processed = 0;
    
    // Process all signals that have registered handlers
    for (const auto& [signal, handlers] : handlers_) {
        if (hasHandlersForSignal(signal)) {
            // Check timeout if specified
            if (timeout.count() > 0) {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed >= timeout) {
                    LOG_F(INFO, "Signal processing timeout reached after processing {} signals", processed);
                    break;
                }
            }
            
            try {
                // Execute all handlers for this signal
                for (const auto& handler : handlers) {
                    if (executeHandlerWithTimeout(handler.handler, signal)) {
                        processed++;
                        // Update statistics
                        if (signalStats_.find(signal) != signalStats_.end()) {
                            signalStats_[signal].processed++;
                            signalStats_[signal].lastProcessed = std::chrono::steady_clock::now();
                        }
                    } else {
                        LOG_F(WARNING, "Handler timed out while processing signal {}", signal);
                        if (signalStats_.find(signal) != signalStats_.end()) {
                            signalStats_[signal].handlerErrors++;
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in signal handler for signal {}: {}", signal, e.what());
                if (signalStats_.find(signal) != signalStats_.end()) {
                    signalStats_[signal].handlerErrors++;
                }
            }
        }
    }
    
    return processed;
}

bool SignalHandlerRegistry::hasHandlersForSignal(SignalID signal) const {
    std::lock_guard lock(mutex_);
    auto it = handlers_.find(signal);
    return it != handlers_.end() && !it->second.empty();
}

const SignalStats& SignalHandlerRegistry::getSignalStats(SignalID signal) const {
    std::lock_guard lock(mutex_);
    static SignalStats emptyStats;
    auto it = signalStats_.find(signal);
    if (it != signalStats_.end()) {
        return it->second;
    }
    return emptyStats;
}

void SignalHandlerRegistry::resetStats(SignalID signal) {
    std::lock_guard lock(mutex_);
    if (signal == -1) {
        // Reset all stats
        for (auto& [sig, stats] : signalStats_) {
            signalStats_.try_emplace(sig);
        }
    } else {
        // Reset specific signal stats
        signalStats_.try_emplace(signal);
    }
}

void SignalHandlerRegistry::setHandlerTimeout(std::chrono::milliseconds timeout) {
    std::lock_guard lock(mutex_);
    handlerTimeout_ = timeout;
}

bool SignalHandlerRegistry::executeHandlerWithTimeout(const SignalHandler& handler, SignalID signal) {
    if (handlerTimeout_.count() <= 0) {
        // No timeout, execute directly
        handler(signal);
        return true;
    }
    
    // Use std::async with timeout
    auto future = std::async(std::launch::async, [&handler, signal]() {
        handler(signal);
    });
    
    // Wait for the future with timeout
    auto status = future.wait_for(handlerTimeout_);
    return status == std::future_status::ready;
}

SignalHandlerRegistry::SignalHandlerRegistry() = default;

SignalHandlerRegistry::~SignalHandlerRegistry() = default;

void SignalHandlerRegistry::signalDispatcher(int signal) {
    SignalHandlerRegistry& registry = getInstance();
    
    // Just record signal received, will be processed by separate thread in SafeSignalManager
    if (registry.signalStats_.find(signal) != registry.signalStats_.end()) {
        registry.signalStats_[signal].received++;
        registry.signalStats_[signal].lastReceived = std::chrono::steady_clock::now();
    } else {
        std::lock_guard lock(registry.mutex_);
        registry.signalStats_.try_emplace(signal);
        auto& stats = registry.signalStats_[signal];
        stats.received = 1;
        stats.lastReceived = std::chrono::steady_clock::now();
    }
    
    // Forward to safe manager if available
    SafeSignalManager::safeSignalDispatcher(signal);
    
    // Immediate handling for critical signals
    std::lock_guard lock(registry.mutex_);
    auto handlerIterator = registry.handlers_.find(signal);
    if (handlerIterator != registry.handlers_.end()) {
        for (const auto& handler : handlerIterator->second) {
            try {
                handler.handler(signal);
                if (registry.signalStats_.find(signal) != registry.signalStats_.end()) {
                    registry.signalStats_[signal].processed++;
                    registry.signalStats_[signal].lastProcessed = std::chrono::steady_clock::now();
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in direct signal handler for signal {}: {}", signal, e.what());
                if (registry.signalStats_.find(signal) != registry.signalStats_.end()) {
                    registry.signalStats_[signal].handlerErrors++;
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
    // Start worker threads
    for (size_t i = 0; i < threadCount; ++i) {
        workerThreads_.emplace_back([this]() { processSignals(); });
    }
    
    LOG_F(INFO, "SafeSignalManager initialized with {} worker threads and queue size {}", 
          threadCount, queueSize);
}

SafeSignalManager::~SafeSignalManager() {
    keepRunning_ = false;
    clearSignalQueue(); // Clear queue to avoid deadlocks
    
    // Wake up any sleeping threads
    queueCondition_.notify_all();
    
    // Worker threads will join automatically (std::jthread)
    LOG_F(INFO, "SafeSignalManager shutting down");
}

int SafeSignalManager::addSafeSignalHandler(SignalID signal,
                                         const SignalHandler& handler,
                                         int priority,
                                         const std::string& handlerName) {
    std::lock_guard lock(queueMutex_);
    int handlerId = nextHandlerId_++;
    safeHandlers_[signal].emplace(handler, priority, handlerName);
    handlerRegistry_[handlerId] = {signal, handler};
    
    // Update statistics
    std::lock_guard statsLock(statsMutex_);
    if (signalStats_.find(signal) == signalStats_.end()) {
        signalStats_.try_emplace(signal, SignalStats{});
    }
    
    LOG_F(INFO, "Added safe signal handler for signal {} with priority {} and ID {}",
          signal, priority, handlerId);
    
    return handlerId;
}

bool SafeSignalManager::removeSafeSignalHandlerById(int handlerId) {
    std::lock_guard lock(queueMutex_);
    auto it = handlerRegistry_.find(handlerId);
    if (it == handlerRegistry_.end()) {
        return false;
    }
    
    SignalID signal = it->second.first;
    const SignalHandler& handler = it->second.second;
    
    auto handlerIterator = safeHandlers_.find(signal);
    if (handlerIterator != safeHandlers_.end()) {
        auto handlerWithPriority =
            std::find_if(handlerIterator->second.begin(), handlerIterator->second.end(),
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
    std::lock_guard lock(queueMutex_);
    auto handlerIterator = safeHandlers_.find(signal);
    if (handlerIterator != safeHandlers_.end()) {
        auto handlerWithPriority =
            std::find_if(handlerIterator->second.begin(), handlerIterator->second.end(),
                         [&handler](const SignalHandlerWithPriority& handlerPriority) {
                             return handlerPriority.handler.target<void(SignalID)>() ==
                                    handler.target<void(SignalID)>();
                         });
        if (handlerWithPriority != handlerIterator->second.end()) {
            // Also remove from handler registry
            for (auto it = handlerRegistry_.begin(); it != handlerRegistry_.end(); ) {
                if (it->second.first == signal && 
                    it->second.second.target<void(SignalID)>() == handler.target<void(SignalID)>()) {
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
    std::lock_guard lock(queueMutex_);
    
    // Update statistics
    {
        std::lock_guard statsLock(statsMutex_);
        if (signalStats_.find(signal) == signalStats_.end()) {
            signalStats_.try_emplace(signal, SignalStats{});
        }
        signalStats_[signal].received++;
        signalStats_[signal].lastReceived = std::chrono::steady_clock::now();
    }
    
    // Check if queue is full
    if (signalQueue_.size() >= maxQueueSize_) {
        // Update dropped count
        std::lock_guard statsLock(statsMutex_);
        signalStats_[signal].dropped++;
        LOG_F(WARNING, "Signal queue full, dropping signal {}", signal);
        return false;
    }
    
    // Add signal to queue
    signalQueue_.push_back(signal);
    queueCondition_.notify_one(); // Wake up a worker thread
    return true;
}

size_t SafeSignalManager::getQueueSize() const {
    std::lock_guard lock(queueMutex_);
    return signalQueue_.size();
}

const SignalStats& SafeSignalManager::getSignalStats(SignalID signal) const {
    std::lock_guard lock(statsMutex_);
    static SignalStats emptyStats;
    auto it = signalStats_.find(signal);
    if (it != signalStats_.end()) {
        return it->second;
    }
    return emptyStats;
}

void SafeSignalManager::resetStats(SignalID signal) {
    std::lock_guard lock(statsMutex_);
    if (signal == -1) {
        // Reset all stats
        for (auto& [sig, stats] : signalStats_) {
            signalStats_.try_emplace(sig);
        }
    } else {
        // Reset specific signal stats
        signalStats_.try_emplace(signal);
    }
}

bool SafeSignalManager::setWorkerThreadCount(size_t threadCount) {
    // Can't change thread count while running
    if (!keepRunning_) {
        return false;
    }
    
    // Stop current threads
    keepRunning_ = false;
    queueCondition_.notify_all();
    workerThreads_.clear(); // Join all threads
    
    // Start new threads
    keepRunning_ = true;
    for (size_t i = 0; i < threadCount; ++i) {
        workerThreads_.emplace_back([this]() { processSignals(); });
    }
    
    LOG_F(INFO, "Changed worker thread count to {}", threadCount);
    return true;
}

void SafeSignalManager::setMaxQueueSize(size_t size) {
    std::lock_guard lock(queueMutex_);
    maxQueueSize_ = size;
    LOG_F(INFO, "Changed maximum queue size to {}", size);
}

int SafeSignalManager::clearSignalQueue() {
    std::lock_guard lock(queueMutex_);
    int cleared = static_cast<int>(signalQueue_.size());
    signalQueue_.clear();
    LOG_F(INFO, "Cleared signal queue, removed {} signals", cleared);
    return cleared;
}

void SafeSignalManager::processSignals() {
    while (keepRunning_) {
        int signal = 0;
        {
            std::unique_lock lock(queueMutex_);
            // Wait for a signal or until shutdown
            queueCondition_.wait(lock, [this] { 
                return !signalQueue_.empty() || !keepRunning_; 
            });
            
            if (!keepRunning_) {
                break;
            }
            
            if (!signalQueue_.empty()) {
                signal = signalQueue_.front();
                signalQueue_.pop_front();
            }
        }

        if (signal != 0) {
            std::lock_guard lock(queueMutex_);
            auto handlerIterator = safeHandlers_.find(signal);
            if (handlerIterator != safeHandlers_.end()) {
                for (const auto& handler : handlerIterator->second) {
                    try {
                        LOG_F(INFO, "Processing signal {} with handler {}", 
                              signal, handler.name.empty() ? "unnamed" : handler.name);
                        handler.handler(signal);
                        
                        // Update statistics
                        std::lock_guard statsLock(statsMutex_);
                        signalStats_[signal].processed++;
                        signalStats_[signal].lastProcessed = std::chrono::steady_clock::now();
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception in safe signal handler for signal {}: {}", 
                               signal, e.what());
                        
                        // Update error statistics
                        std::lock_guard statsLock(statsMutex_);
                        signalStats_[signal].handlerErrors++;
                    }
                }
            }
        } else {
            // No signal to process, sleep a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_DURATION_MS));
        }
    }
}

// Cross-platform signal installation for safe handling
void installPlatformSpecificHandlers() {
#if defined(_WIN32) || defined(_WIN64)
    // Windows-specific signal handling
    SignalHandlerRegistry::getInstance().setStandardCrashHandlerSignals(
        [](int signal) {
            LOG_F(ERROR, "Caught signal {} on Windows", signal);
        }, 100, "PlatformCrashHandler-Windows");
    
    // Add specific Windows exception handling
    SafeSignalManager::getInstance().addSafeSignalHandler(
        SIGBREAK, [](int signal) {
            LOG_F(WARNING, "Caught SIGBREAK on Windows");
        }, 90, "Windows-SIGBREAK-Handler");
        
#else
    // POSIX (Linux, macOS) specific signals
    SignalHandlerRegistry::getInstance().setStandardCrashHandlerSignals(
        [](int signal) {
            LOG_F(ERROR, "Caught signal {} on POSIX system", signal);
        }, 100, "PlatformCrashHandler-POSIX");
    
    // Add POSIX-specific signal handlers
    SafeSignalManager::getInstance().addSafeSignalHandler(
        SIGHUP, [](int signal) {
            LOG_F(INFO, "Caught SIGHUP - reloading configuration");
        }, 80, "POSIX-SIGHUP-Handler");
        
    SafeSignalManager::getInstance().addSafeSignalHandler(
        SIGUSR1, [](int signal) {
            LOG_F(INFO, "Caught SIGUSR1 - custom action");
        }, 80, "POSIX-SIGUSR1-Handler");
#endif

    // Common handlers for both platforms
    SafeSignalManager::getInstance().addSafeSignalHandler(
        SIGTERM, [](int signal) {
            LOG_F(WARNING, "Caught SIGTERM - preparing for shutdown");
        }, 100, "Common-SIGTERM-Handler");
}

void initializeSignalSystem(size_t workerThreadCount, size_t queueSize) {
    // Initialize the safe signal manager with specified parameters
    auto& manager = SafeSignalManager::getInstance();
    
    // Configure worker threads and queue size
    manager.setWorkerThreadCount(workerThreadCount);
    manager.setMaxQueueSize(queueSize);
    
    // Set handler timeout in registry
    SignalHandlerRegistry::getInstance().setHandlerTimeout(std::chrono::milliseconds(2000));
    
    // Install platform-specific handlers
    installPlatformSpecificHandlers();
    
    LOG_F(INFO, "Signal system initialized with {} worker threads and queue size {}", 
          workerThreadCount, queueSize);
}
