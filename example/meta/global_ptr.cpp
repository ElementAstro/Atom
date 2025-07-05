/**
 * Comprehensive examples for GlobalSharedPtrManager
 *
 * This file demonstrates all GlobalSharedPtrManager functionality:
 * 1. Basic shared pointer management
 * 2. Weak pointer handling
 * 3. Custom deleter usage
 * 4. Macro usage
 * 5. Metadata and diagnostics
 * 6. Concurrency aspects
 * 7. Automatic cleanup
 * 8. Error handling
 *
 * @author Example Author
 * @date 2024-03-21
 */

#include "atom/meta/global_ptr.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// 假设这是atom中的异常类
namespace atom {
class Exception : public std::exception {
public:
    Exception(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};
using AtomException = Exception;
}  // namespace atom

// 假设的atom::memory命名空间
namespace atom::memory {
template <typename T, typename... Args>
auto makeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
}  // namespace atom::memory

// Example constants for component IDs
namespace Constants {
inline constexpr const char* LOGGER = "system.logger";
inline constexpr const char* CONFIG = "system.config";
inline constexpr const char* DATABASE = "system.database";
inline constexpr const char* CACHE = "system.cache";
inline constexpr const char* AUTH = "system.auth";
}  // namespace Constants

// Example classes for demonstration
class Logger {
public:
    Logger(const std::string& name = "default") : name_(name) {
        std::cout << "Logger '" << name_ << "' created" << std::endl;
    }

    ~Logger() {
        std::cout << "Logger '" << name_ << "' destroyed" << std::endl;
    }

    void log(const std::string& message) const {
        std::cout << "[" << name_ << "] " << message << std::endl;
    }

private:
    std::string name_;
};

class Config {
public:
    Config() {
        std::cout << "Config created" << std::endl;
        settings_["debug"] = "true";
        settings_["log_level"] = "info";
    }

    ~Config() { std::cout << "Config destroyed" << std::endl; }

    std::string get(const std::string& key) const {
        auto it = settings_.find(key);
        return (it != settings_.end()) ? it->second : "";
    }

    void set(const std::string& key, const std::string& value) {
        settings_[key] = value;
    }

private:
    std::unordered_map<std::string, std::string> settings_;
};

class Database {
public:
    Database(const std::string& connection_string)
        : connection_string_(connection_string) {
        std::cout << "Database connected to: " << connection_string_
                  << std::endl;
    }

    ~Database() { std::cout << "Database disconnected" << std::endl; }

    void query(const std::string& sql) {
        std::cout << "Executing SQL: " << sql << std::endl;
    }

private:
    std::string connection_string_;
};

class Cache {
public:
    Cache(size_t capacity) : capacity_(capacity) {
        std::cout << "Cache created with capacity: " << capacity_ << std::endl;
    }

    ~Cache() { std::cout << "Cache destroyed" << std::endl; }

    void put(const std::string& key, const std::string& value) {
        data_[key] = value;
        std::cout << "Cached: " << key << " = " << value << std::endl;
    }

    std::string get(const std::string& key) const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : "";
    }

private:
    size_t capacity_;
    std::unordered_map<std::string, std::string> data_;
};

// Helper function to print separator lines
void printSeparator(const std::string& title) {
    std::cout << "\n=================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==================================================\n"
              << std::endl;
}

// Helper function to print pointer status
template <typename T>
void printPointerStatus(const std::string& name,
                        const std::shared_ptr<T>& ptr) {
    std::cout << name << ": " << (ptr ? "Valid" : "Invalid")
              << ", Use count: " << (ptr ? ptr.use_count() : 0) << std::endl;
}

// Example 1: Basic shared pointer management
void demonstrateBasicPointerManagement() {
    printSeparator("1. Basic Shared Pointer Management");

    // Create and store a shared pointer
    auto logger = std::make_shared<Logger>("main");
    AddPtr("logger.main", logger);

    // Retrieve the pointer
    auto retrieved_logger = GetPtr<Logger>("logger.main");
    if (retrieved_logger) {
        std::cout << "Retrieved logger successfully" << std::endl;
        retrieved_logger.value()->log("System initialized");
    }

    // Create a pointer using getOrCreateSharedPtr
    auto config = GetPtrOrCreate<Config>(
        "config.main", []() { return std::make_shared<Config>(); });

    config->set("version", "1.0.0");
    std::cout << "Config version: " << config->get("version") << std::endl;

    // Demonstrate pointer access and metadata
    auto logger_info = GetPtrInfo("logger.main");
    if (logger_info) {
        std::cout << "Logger metadata:" << std::endl;
        std::cout << "  Type: " << logger_info->type_name << std::endl;
        std::cout << "  Access count: " << logger_info->access_count
                  << std::endl;
        std::cout << "  Reference count: " << logger_info->ref_count
                  << std::endl;
    }

    // Remove a pointer
    std::cout << "Removing logger.main..." << std::endl;
    RemovePtr("logger.main");
    auto missing_logger = GetPtr<Logger>("logger.main");
    std::cout << "Logger exists: "
              << (missing_logger.has_value() ? "Yes" : "No") << std::endl;
}

// Example 2: Weak pointer handling
void demonstrateWeakPointerHandling() {
    printSeparator("2. Weak Pointer Handling");

    // Create a shared pointer and get a weak reference
    auto db = std::make_shared<Database>("mongodb://localhost:27017");
    AddPtr("db.main", db);

    // Get weak pointer from stored shared pointer
    std::weak_ptr<Database> weak_db = GetWeakPtr<Database>("db.main");

    std::cout << "Weak pointer expired: " << weak_db.expired() << std::endl;

    // Lock the weak pointer to access the object
    if (auto locked_db = weak_db.lock()) {
        locked_db->query("SELECT * FROM users");
    }

    // Store a weak pointer directly
    std::weak_ptr<Database> another_weak = weak_db;
    GlobalSharedPtrManager::getInstance().addWeakPtr("db.weak", another_weak);

    // Get shared pointer from stored weak pointer
    auto shared_from_weak =
        GlobalSharedPtrManager::getInstance().getSharedPtrFromWeakPtr<Database>(
            "db.weak");

    if (shared_from_weak) {
        shared_from_weak->query("SELECT COUNT(*) FROM orders");
    }

    // Demonstrate weak pointer expiration
    std::cout << "Resetting original shared pointer..." << std::endl;
    db.reset();

    std::cout << "Original weak pointer expired: " << weak_db.expired()
              << std::endl;

    // Try to lock expired weak pointer
    auto locked_expired = weak_db.lock();
    std::cout << "Locked expired pointer valid: "
              << (locked_expired ? "Yes" : "No") << std::endl;

    // Clean up expired weak pointers
    auto removed =
        GlobalSharedPtrManager::getInstance().removeExpiredWeakPtrs();
    std::cout << "Removed " << removed << " expired weak pointer(s)"
              << std::endl;
}

// Example 3: Custom deleter usage
void demonstrateCustomDeleterUsage() {
    printSeparator("3. Custom Deleter Usage");

    // Create a logger with custom deleter
    auto custom_logger =
        std::shared_ptr<Logger>(new Logger("custom"), [](Logger* ptr) {
            std::cout << "Custom deleter called for Logger" << std::endl;
            delete ptr;
        });

    // Add to manager
    AddPtr("logger.custom", custom_logger);

    // Create another logger and add a custom deleter afterward
    auto another_logger = std::make_shared<Logger>("another");
    AddPtr("logger.another", another_logger);

    // Add custom deleter
    AddDeleter<Logger>("logger.another", [](Logger* ptr) {
        std::cout << "Added custom deleter called for Logger" << std::endl;
        delete ptr;
    });

    // Using macro with custom deleter
    auto my_logger = GetPtrOrCreate<Logger>("logger.macro", []() {
        return std::shared_ptr<Logger>(new Logger("macro"), [](Logger* ptr) {
            std::cout << "Macro-defined custom deleter called" << std::endl;
            delete ptr;
        });
    });

    my_logger->log("Using logger with custom deleter");

    // Demonstrate custom deletion
    std::cout << "Removing all loggers..." << std::endl;
    RemovePtr("logger.custom");
    RemovePtr("logger.another");
    RemovePtr("logger.macro");
}

// Example 4: Macro usage
void demonstrateMacroUsage() {
    printSeparator("4. Macro Usage for Simplified Access");

    // Basic creation
    std::shared_ptr<Cache> simple_cache;
    GET_OR_CREATE_PTR(simple_cache, Cache, Constants::CACHE, 1000);
    simple_cache->put("key1", "value1");

    // Creation with 'this' pointer
    struct ServiceWithThis {
        void setupCache() {
            // 使用this捕获
            std::shared_ptr<Cache> cache_;
            if (auto ptr = GetPtrOrCreate<Cache>("cache.service", [this]() {
                    return std::make_shared<Cache>(500);
                })) {
                cache_ = ptr;
                cache_->put("service.status", "running");
            }
        }
    };

    ServiceWithThis service;
    service.setupCache();

    // Weak pointer creation
    std::weak_ptr<Config> weak_config;
    GET_OR_CREATE_WEAK_PTR(weak_config, Config, Constants::CONFIG);
    if (auto config = weak_config.lock()) {
        config->set("initialized", "true");
        std::cout << "Config initialized: " << config->get("initialized")
                  << std::endl;
    }

    // GET_WEAK_PTR macro usage (修复版本)
    try {
        std::weak_ptr<Database> dbPtr;
        GET_OR_CREATE_WEAK_PTR(dbPtr, Database, Constants::DATABASE, "memory");
        auto db = dbPtr.lock();
        if (db) {
            db->query("SELECT version()");
        } else {
            throw atom::AtomException("Database pointer is invalid");
        }
    } catch (const atom::AtomException& ex) {
        std::cout << "Expected exception: " << ex.what() << std::endl;

        // Create the database so the next example works
        std::shared_ptr<Database> db;
        GET_OR_CREATE_PTR(db, Database, Constants::DATABASE, "sqlite://memory");
    }

    // Try again after object is created
    try {
        std::weak_ptr<Database> dbPtr;
        GET_OR_CREATE_WEAK_PTR(dbPtr, Database, Constants::DATABASE);
        auto db = dbPtr.lock();
        if (db) {
            db->query("SELECT version()");
            std::cout << "Successfully retrieved database through GET_WEAK_PTR"
                      << std::endl;
        } else {
            throw atom::AtomException("Database pointer is invalid");
        }
    } catch (const atom::AtomException& ex) {
        std::cout << "Unexpected exception: " << ex.what() << std::endl;
    }

    // Advanced usage with capture
    const std::string connection = "postgres://localhost/mydb";
    std::shared_ptr<Database> pg_db;

    if (auto ptr = GetPtrOrCreate<Database>("db.postgres", [&connection]() {
            return atom::memory::makeShared<Database>(connection);
        })) {
        pg_db = ptr;
        pg_db->query("SELECT current_timestamp");
    }
}

// Example 5: Metadata and diagnostics
void demonstrateMetadataAndDiagnostics() {
    printSeparator("5. Metadata and Diagnostics");

    // Create several objects to demonstrate
    for (int i = 0; i < 5; i++) {
        std::string key = "diag.logger." + std::to_string(i);
        std::shared_ptr<Logger> logger;
        if (auto ptr = GetPtrOrCreate<Logger>(key, [i]() {
                return std::make_shared<Logger>("logger-" + std::to_string(i));
            })) {
            logger = ptr;
        }

        // Access some loggers multiple times to affect access counts
        if (i % 2 == 0) {
            for (int j = 0; j < i; j++) {
                auto l = GetPtr<Logger>(key);
                if (l.has_value()) {
                    l.value()->log("Diagnostic message " + std::to_string(j));
                }
            }
        }
    }

    // Print information about all stored pointers
    std::cout << "Current managed pointer count: "
              << GlobalSharedPtrManager::getInstance().size() << std::endl;

    GlobalSharedPtrManager::getInstance().printSharedPtrMap();

    // Get detailed information about a specific pointer
    auto logger0_info = GetPtrInfo("diag.logger.0");
    auto logger4_info = GetPtrInfo("diag.logger.4");

    if (logger0_info && logger4_info) {
        std::cout << "\nDetailed comparison of two loggers:" << std::endl;
        std::cout << "  Logger 0 access count: " << logger0_info->access_count
                  << std::endl;
        std::cout << "  Logger 4 access count: " << logger4_info->access_count
                  << std::endl;

        std::cout << "  Logger 0 creation time: "
                  << std::chrono::system_clock::to_time_t(
                         logger0_info->creation_time)
                  << std::endl;
        std::cout << "  Logger 4 creation time: "
                  << std::chrono::system_clock::to_time_t(
                         logger4_info->creation_time)
                  << std::endl;
    }

    // Clean old pointers based on time
    std::cout << "\nCleaning pointers older than 1 hour..." << std::endl;
    auto cleaned =
        GlobalSharedPtrManager::getInstance().cleanOldPointers(3600s);
    std::cout << "Cleaned " << cleaned << " old pointer(s)" << std::endl;
}

// Example 6: Concurrency aspects
void demonstrateConcurrency() {
    printSeparator("6. Concurrency Aspects");

    // Create a shared resource
    std::shared_ptr<std::atomic<int>> shared_counter;
    GET_OR_CREATE_PTR(shared_counter, std::atomic<int>, "counter", 0);

    // Create and start threads that modify the counter
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back([i]() {
            std::cout << "Thread " << i << " started" << std::endl;

            // Get the shared counter in each thread
            auto counter_ptr = GetPtr<std::atomic<int>>("counter");
            if (!counter_ptr) {
                std::cout << "Thread " << i << " failed to get counter"
                          << std::endl;
                return;
            }

            // Increment the counter multiple times
            auto& counter = *counter_ptr.value();
            for (int j = 0; j < 10; j++) {
                int old_value = counter.fetch_add(1, std::memory_order_relaxed);
                std::cout << "Thread " << i << " incremented counter from "
                          << old_value << " to " << (old_value + 1)
                          << std::endl;
                std::this_thread::sleep_for(10ms);
            }

            std::cout << "Thread " << i << " finished" << std::endl;
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify final counter value
    auto final_counter = GetPtr<std::atomic<int>>("counter");
    if (final_counter) {
        std::cout << "Final counter value: " << final_counter.value()->load()
                  << std::endl;
    }
}

// Example 7: Automatic cleanup
void demonstrateAutomaticCleanup() {
    printSeparator("7. Automatic Cleanup");

    // Create temporary objects
    for (int i = 0; i < 10; i++) {
        std::string key = "temp.object." + std::to_string(i);
        std::shared_ptr<std::string> obj;
        if (auto ptr = GetPtrOrCreate<std::string>(key, [i]() {
                return std::make_shared<std::string>("Temporary object " +
                                                     std::to_string(i));
            })) {
            obj = ptr;
        }

        // Create some weak references
        if (i % 3 == 0) {
            std::weak_ptr<std::string> weak = GetWeakPtr<std::string>(key);
            // 修复: 使用addWeakPtr而不是addSharedPtr
            GlobalSharedPtrManager::getInstance().addWeakPtr(
                "temp.weak." + std::to_string(i), weak);
        }
    }

    std::cout << "Created 10 temporary objects" << std::endl;
    std::cout << "Current number of managed objects: "
              << GlobalSharedPtrManager::getInstance().size() << std::endl;

    // Remove some objects to create expired weak references
    for (int i = 0; i < 10; i += 3) {
        std::string key = "temp.object." + std::to_string(i);
        RemovePtr(key);
    }

    std::cout << "Removed some objects, creating expired weak references"
              << std::endl;

    // Clean up expired weak references
    auto removed =
        GlobalSharedPtrManager::getInstance().removeExpiredWeakPtrs();
    std::cout << "Removed " << removed << " expired weak reference(s)"
              << std::endl;

    // Clear all remaining objects
    std::cout << "Clearing all remaining objects..." << std::endl;
    GlobalSharedPtrManager::getInstance().clearAll();

    std::cout << "Remaining managed objects: "
              << GlobalSharedPtrManager::getInstance().size() << std::endl;
}

// Example 8: Error handling
void demonstrateErrorHandling() {
    printSeparator("8. Error Handling");

    // Attempt to get non-existent pointer
    try {
        std::weak_ptr<Logger> nonexistentPtr;
        GET_OR_CREATE_WEAK_PTR(nonexistentPtr, Logger, "nonexistent.logger");
        auto nonexistent = nonexistentPtr.lock();
        if (!nonexistent) {
            throw atom::AtomException(
                "Component: nonexistent.logger not exist");
        }
        nonexistent->log("This should not execute");
    } catch (const atom::AtomException& ex) {
        std::cout << "Expected exception caught: " << ex.what() << std::endl;
    }

    // Type mismatch example
    std::shared_ptr<int> int_value;
    GET_OR_CREATE_PTR(int_value, int, "value", 42);

    try {
        // This causes a type mismatch
        auto str_value = GetPtr<std::string>("value");
        std::cout << "Type mismatch handling successful: "
                  << (str_value.has_value() ? "unexpected success"
                                            : "properly returned nullopt")
                  << std::endl;
    } catch (const std::exception& ex) {
        std::cout << "Exception during type mismatch: " << ex.what()
                  << std::endl;
    }

    // Handle pointer replacement with different type
    std::shared_ptr<std::string> str_value;
    GET_OR_CREATE_PTR(str_value, std::string, "value", "replaced value");

    auto new_value = GetPtr<std::string>("value");
    if (new_value) {
        std::cout << "Successfully replaced int with string: "
                  << *new_value.value() << std::endl;
    }
}

int main() {
    std::cout << "================================================"
              << std::endl;
    std::cout << "  GlobalSharedPtrManager Comprehensive Examples" << std::endl;
    std::cout << "================================================"
              << std::endl;

    try {
        demonstrateBasicPointerManagement();
        demonstrateWeakPointerHandling();
        demonstrateCustomDeleterUsage();
        demonstrateMacroUsage();
        demonstrateMetadataAndDiagnostics();
        demonstrateConcurrency();
        demonstrateAutomaticCleanup();
        demonstrateErrorHandling();

        std::cout << "\nAll demonstrations completed successfully!"
                  << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Unhandled exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
