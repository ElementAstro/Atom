#pragma once

#include "thread_safe_xml.hpp"
#include "parallel_processor.hpp"
#include "../performance/metrics_collector.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <concepts>
#include <type_traits>
#include <mutex>
#include <shared_mutex>
#include <source_location>

namespace atom::extra::pugixml::concurrent {

/**
 * @brief Concept for thread-safe builder configurators
 */
template<typename F>
concept ThreadSafeBuilderConfigurator = requires(F f) {
    requires std::is_invocable_v<F, class ThreadSafeNodeBuilder&>;
};

/**
 * @brief Thread-safe XML node builder with concurrent construction support
 */
class ThreadSafeNodeBuilder {
private:
    ThreadSafeNode node_;
    mutable std::shared_mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    performance::MetricsCollector* metrics_;
    std::atomic<uint32_t> operation_count_{0};
    
    void log_operation(std::string_view operation,
                      const std::source_location& loc = std::source_location::current()) const {
        if (logger_) {
            logger_->trace("ThreadSafeNodeBuilder::{} called from {}:{}", 
                          operation, loc.file_name(), loc.line());
        }
        operation_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void record_timing(std::string_view operation, double microseconds) const {
        if (metrics_) {
            metrics_->record_timing(std::string(operation), microseconds);
        }
    }
    
public:
    explicit ThreadSafeNodeBuilder(ThreadSafeNode node,
                                  std::shared_ptr<spdlog::logger> logger = nullptr,
                                  performance::MetricsCollector* metrics = nullptr)
        : node_(std::move(node)), logger_(logger), metrics_(metrics) {
        log_operation("constructor");
    }
    
    ThreadSafeNodeBuilder(const ThreadSafeNodeBuilder& other)
        : node_(other.node_), logger_(other.logger_), metrics_(other.metrics_) {
        log_operation("copy_constructor");
    }
    
    ThreadSafeNodeBuilder& operator=(const ThreadSafeNodeBuilder& other) {
        if (this != &other) {
            std::unique_lock lock(mutex_);
            std::shared_lock other_lock(other.mutex_);
            node_ = other.node_;
            logger_ = other.logger_;
            metrics_ = other.metrics_;
            log_operation("copy_assignment");
        }
        return *this;
    }
    
    ThreadSafeNodeBuilder(ThreadSafeNodeBuilder&& other) noexcept
        : node_(std::move(other.node_)), logger_(other.logger_), metrics_(other.metrics_) {
        log_operation("move_constructor");
    }
    
    ThreadSafeNodeBuilder& operator=(ThreadSafeNodeBuilder&& other) noexcept {
        if (this != &other) {
            std::unique_lock lock(mutex_);
            node_ = std::move(other.node_);
            logger_ = other.logger_;
            metrics_ = other.metrics_;
            log_operation("move_assignment");
        }
        return *this;
    }
    
    /**
     * @brief Thread-safe attribute setting with fluent interface
     */
    template<typename NameType, typename ValueType>
    ThreadSafeNodeBuilder& attribute(NameType&& name, ValueType&& value) {
        auto timer = performance::HighResolutionTimer{};
        std::unique_lock lock(mutex_);
        
        log_operation("attribute");
        
        if constexpr (std::is_convertible_v<ValueType, std::string_view>) {
            node_.set_attribute(std::string_view(name), std::string_view(value));
        } else {
            node_.set_attribute(std::string_view(name), std::to_string(value));
        }
        
        record_timing("attribute_set", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Thread-safe multiple attributes setting
     */
    template<typename... Pairs>
    ThreadSafeNodeBuilder& attributes(Pairs&&... pairs) {
        auto timer = performance::HighResolutionTimer{};
        std::unique_lock lock(mutex_);
        
        log_operation("attributes");
        
        auto set_attribute = [this](const auto& pair) {
            if constexpr (std::is_convertible_v<decltype(pair.value), std::string_view>) {
                node_.set_attribute(pair.name, std::string_view(pair.value));
            } else {
                node_.set_attribute(pair.name, std::to_string(pair.value));
            }
        };
        
        (set_attribute(pairs), ...);
        
        record_timing("attributes_set", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Thread-safe text content setting
     */
    template<typename T>
    ThreadSafeNodeBuilder& text(T&& value) {
        auto timer = performance::HighResolutionTimer{};
        std::unique_lock lock(mutex_);
        
        log_operation("text");
        
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            node_.set_text(std::string_view(value));
        } else {
            node_.set_text(std::to_string(value));
        }
        
        record_timing("text_set", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Thread-safe child element creation with configurator
     */
    template<typename F>
        requires ThreadSafeBuilderConfigurator<F>
    ThreadSafeNodeBuilder& child(std::string_view name, F&& configurator) {
        auto timer = performance::HighResolutionTimer{};
        std::unique_lock lock(mutex_);
        
        log_operation("child_with_configurator");
        
        auto child_node = node_.append_child(name);
        ThreadSafeNodeBuilder child_builder(child_node, logger_, metrics_);
        
        // Release lock before calling configurator to avoid deadlock
        lock.unlock();
        
        std::invoke(std::forward<F>(configurator), child_builder);
        
        record_timing("child_configured", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Thread-safe simple child with text content
     */
    template<typename T>
        requires(!ThreadSafeBuilderConfigurator<T>)
    ThreadSafeNodeBuilder& child(std::string_view name, T&& text_value) {
        auto timer = performance::HighResolutionTimer{};
        std::unique_lock lock(mutex_);
        
        log_operation("child_with_text");
        
        auto child_node = node_.append_child(name);
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            child_node.set_text(std::string_view(text_value));
        } else {
            child_node.set_text(std::to_string(text_value));
        }
        
        record_timing("child_text_set", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Thread-safe parallel children creation from container
     */
    template<typename Container, typename Transformer>
    ThreadSafeNodeBuilder& children_parallel(std::string_view element_name,
                                            const Container& container,
                                            Transformer&& transform) {
        auto timer = performance::HighResolutionTimer{};
        log_operation("children_parallel");
        
        // Create futures for parallel child creation
        std::vector<std::future<void>> futures;
        futures.reserve(container.size());
        
        for (const auto& item : container) {
            futures.push_back(std::async(std::launch::async, [this, element_name, &item, &transform]() {
                std::unique_lock lock(mutex_);
                auto child_node = node_.append_child(element_name);
                ThreadSafeNodeBuilder child_builder(child_node, logger_, metrics_);
                lock.unlock();
                
                std::invoke(std::forward<Transformer>(transform), child_builder, item);
            }));
        }
        
        // Wait for all children to be created
        for (auto& future : futures) {
            future.wait();
        }
        
        record_timing("children_parallel_created", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Thread-safe conditional building
     */
    template<typename F>
        requires ThreadSafeBuilderConfigurator<F>
    ThreadSafeNodeBuilder& if_condition(bool condition, F&& configurator) {
        if (condition) {
            auto timer = performance::HighResolutionTimer{};
            log_operation("if_condition_true");
            
            std::invoke(std::forward<F>(configurator), *this);
            
            record_timing("conditional_build", timer.elapsed_microseconds());
        } else {
            log_operation("if_condition_false");
        }
        return *this;
    }
    
    /**
     * @brief Thread-safe batch operations
     */
    template<typename... Operations>
    ThreadSafeNodeBuilder& batch(Operations&&... operations) {
        auto timer = performance::HighResolutionTimer{};
        std::unique_lock lock(mutex_);
        
        log_operation("batch_operations");
        
        // Execute all operations while holding the lock
        (std::invoke(std::forward<Operations>(operations), *this), ...);
        
        record_timing("batch_executed", timer.elapsed_microseconds());
        return *this;
    }
    
    /**
     * @brief Get the built node (thread-safe)
     */
    [[nodiscard]] ThreadSafeNode build() const {
        std::shared_lock lock(mutex_);
        log_operation("build");
        return node_;
    }
    
    /**
     * @brief Get the built node (thread-safe)
     */
    [[nodiscard]] ThreadSafeNode get() const {
        return build();
    }
    
    /**
     * @brief Implicit conversion to ThreadSafeNode
     */
    operator ThreadSafeNode() const {
        return build();
    }
    
    /**
     * @brief Get operation count for debugging
     */
    [[nodiscard]] uint32_t operation_count() const noexcept {
        return operation_count_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if node is valid
     */
    [[nodiscard]] bool valid() const {
        std::shared_lock lock(mutex_);
        return !node_.empty();
    }
};

/**
 * @brief Thread-safe document builder with concurrent assembly
 */
class ThreadSafeDocumentBuilder {
private:
    ThreadSafeDocument doc_;
    mutable std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    performance::MetricsCollector* metrics_;
    
    void log_operation(std::string_view operation,
                      const std::source_location& loc = std::source_location::current()) const {
        if (logger_) {
            logger_->trace("ThreadSafeDocumentBuilder::{} called from {}:{}", 
                          operation, loc.file_name(), loc.line());
        }
    }
    
public:
    explicit ThreadSafeDocumentBuilder(std::shared_ptr<spdlog::logger> logger = nullptr,
                                      performance::MetricsCollector* metrics = nullptr)
        : doc_(logger), logger_(logger), metrics_(metrics) {
        log_operation("constructor");
    }
    
    /**
     * @brief Thread-safe XML declaration setting
     */
    ThreadSafeDocumentBuilder& declaration(std::string_view version = "1.0",
                                          std::string_view encoding = "UTF-8",
                                          std::string_view standalone = "") {
        std::lock_guard lock(mutex_);
        log_operation("declaration");
        
        // Implementation would add XML declaration
        // This is a simplified version
        return *this;
    }
    
    /**
     * @brief Thread-safe root element creation with configurator
     */
    template<typename F>
        requires ThreadSafeBuilderConfigurator<F>
    ThreadSafeDocumentBuilder& root(std::string_view name, F&& configurator) {
        auto timer = performance::HighResolutionTimer{};
        std::lock_guard lock(mutex_);
        
        log_operation("root_with_configurator");
        
        auto root_node = doc_.create_root(name);
        ThreadSafeNodeBuilder builder(root_node, logger_, metrics_);
        
        std::invoke(std::forward<F>(configurator), builder);
        
        if (metrics_) {
            metrics_->record_timing("root_configured", timer.elapsed_microseconds());
        }
        
        return *this;
    }
    
    /**
     * @brief Thread-safe simple root with text
     */
    template<typename T>
        requires(!ThreadSafeBuilderConfigurator<T>)
    ThreadSafeDocumentBuilder& root(std::string_view name, T&& text_value) {
        auto timer = performance::HighResolutionTimer{};
        std::lock_guard lock(mutex_);
        
        log_operation("root_with_text");
        
        auto root_node = doc_.create_root(name);
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            root_node.set_text(std::string_view(text_value));
        } else {
            root_node.set_text(std::to_string(text_value));
        }
        
        if (metrics_) {
            metrics_->record_timing("root_text_set", timer.elapsed_microseconds());
        }
        
        return *this;
    }
    
    /**
     * @brief Build the document (thread-safe)
     */
    [[nodiscard]] ThreadSafeDocument build() {
        std::lock_guard lock(mutex_);
        log_operation("build");
        return std::move(doc_);
    }
    
    /**
     * @brief Get the document (thread-safe)
     */
    [[nodiscard]] ThreadSafeDocument get() {
        return build();
    }
};

/**
 * @brief Factory functions for thread-safe builders
 */
[[nodiscard]] inline ThreadSafeDocumentBuilder document(
    std::shared_ptr<spdlog::logger> logger = nullptr,
    performance::MetricsCollector* metrics = nullptr) {
    return ThreadSafeDocumentBuilder{logger, metrics};
}

[[nodiscard]] inline ThreadSafeNodeBuilder element(
    ThreadSafeNode node,
    std::shared_ptr<spdlog::logger> logger = nullptr,
    performance::MetricsCollector* metrics = nullptr) {
    return ThreadSafeNodeBuilder{node, logger, metrics};
}

}  // namespace atom::extra::pugixml::concurrent
