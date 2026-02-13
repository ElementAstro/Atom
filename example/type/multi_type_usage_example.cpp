#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Include multiple type utilities for integration
#include "atom/type/args.hpp"
#include "atom/type/concurrent_map.hpp"
#include "atom/type/expected.hpp"
#include "atom/type/optional.hpp"
#include "atom/type/rjson.hpp"
#include "atom/type/weak_ptr.hpp"

// Helper function to print section headersvoid print_header(const std::string&
// title) {
std::cout << "\n=== " << title << " ===" << std::endl;
std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Example 1: Configuration System using Args + Expected + JSONclass
// ConfigurationManager {
private:
atom::Args config_;
std::string config_file_;

public:
explicit ConfigurationManager(const std::string& config_file)
    : config_file_(config_file) {}

atom::type::expected<void, std::string> loadFromJson(
    const std::string& json_str) {
    try {
        auto json_data = atom::type::JsonParser::parse(json_str);

        if (json_data.type() != atom::type::JsonValue::Type::Object) {
            return atom::type::Error<std::string>("Root must be an object");
        }

        const auto& obj = json_data.asObject();
        for (const auto& [key, value] : obj) {
            switch (value.type()) {
                case atom::type::JsonValue::Type::String:
                    config_.set(key, value.asString());
                    break;
                case atom::type::JsonValue::Type::Number:
                    config_.set(key, value.asNumber());
                    break;
                case atom::type::JsonValue::Type::Bool:
                    config_.set(key, value.asBool());
                    break;
                default:
                    // Skip complex types for this example
                    break;
            }
        }
        return {};
    } catch (const std::exception& e) {
        return atom::type::Error<std::string>("JSON parse error: " +
                                              std::string(e.what()));
    }
}

template <typename T>
atom::type::expected<T, std::string> get(const std::string& key) const {
    try {
        return config_.get<T>(key);
    } catch (const std::exception& e) {
        return atom::type::Error<std::string>("Config key '" + key +
                                              "' not found or wrong type");
    }
}

template <typename T>
T getOr(const std::string& key, T&& default_value) const {
    return config_.getOr<T>(key, std::forward<T>(default_value));
}

void set(const std::string& key, const auto& value) { config_.set(key, value); }

size_t size() const { return config_.size(); }
}
;

// Example 2: Cache System using Concurrent Map + Weak Ptr + Expectedtemplate
// <typename Key, typename Value>
class SmartCache {
private:
    atom::type::concurrent_map<Key, std::shared_ptr<Value>> cache_;
    atom::type::concurrent_map<Key, atom::type::EnhancedWeakPtr<Value>>
        weak_cache_;

public:
    SmartCache() : cache_(4, 100) {}  // 4 threads, cache size 100

    atom::type::expected<std::shared_ptr<Value>, std::string> get(
        const Key& key) {
        // First try the strong cache
        auto strong_result = cache_.find(key);
        if (strong_result) {
            return *strong_result;
        }

        // Try the weak cache
        auto weak_result = weak_cache_.find(key);
        if (weak_result) {
            auto locked = weak_result->lock();
            if (locked) {
                // Promote back to strong cache
                cache_.insert(key, locked);
                return locked;
            } else {
                // Weak reference expired, remove it
                weak_cache_.erase(key);
            }
        }

        return atom::type::Error<std::string>("Key not found in cache");
    }

    void put(const Key& key, std::shared_ptr<Value> value) {
        cache_.insert(key, value);
        weak_cache_.insert(key, atom::type::EnhancedWeakPtr<Value>(value));
    }

    void evictToWeak(const Key& key) {
        auto result = cache_.find(key);
        if (result) {
            weak_cache_.insert(key,
                               atom::type::EnhancedWeakPtr<Value>(*result));
            cache_.erase(key);
        }
    }

    size_t strongCacheSize() const { return cache_.size(); }
    size_t weakCacheSize() const { return weak_cache_.size(); }
};

// Example 3: Data Processing Pipeline using Expected + Optional + Argsclass
// DataProcessor {
private:
atom::Args settings_;

public:
DataProcessor() {
    // Set default processing settings
    settings_.set("max_retries", 3);
    settings_.set("timeout_ms", 5000);
    settings_.set("enable_logging", true);
}

atom::type::expected<std::vector<int>, std::string> processNumbers(
    const std::vector<std::string>& input) {
    std::vector<int> results;
    int max_retries = settings_.getOr<int>("max_retries", 3);
    bool logging = settings_.getOr<bool>("enable_logging", false);

    for (const auto& str : input) {
        auto parsed = parseWithRetry(str, max_retries);
        if (!parsed) {
            if (logging) {
                std::cout << "Failed to parse: " << str << std::endl;
            }
            return atom::type::Error<std::string>("Parse failed for: " + str);
        }
        results.push_back(*parsed);
    }

    return results;
}

private:
atom::type::optional<int> parseWithRetry(const std::string& str,
                                         int max_retries) {
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        try {
            return std::stoi(str);
        } catch (...) {
            if (attempt < max_retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    return atom::type::nullopt;
}
}
;

int main() {
    std::cout << "Advanced Type Integration Examples" << std::endl;
    std::cout << "==================================" << std::endl;

    // Example 1: Configuration Management
    print_header("Configuration Management System");

    ConfigurationManager config("app.json");

    std::string json_config = R"({
        "app_name": "MyApplication",
        "version": "1.0.0",
        "port": 8080,
        "debug": true,
        "max_connections": 100,
        "timeout": 30.5
    })";

    auto load_result = config.loadFromJson(json_config);
    if (load_result) {
        std::cout << "✓ Configuration loaded successfully" << std::endl;
        std::cout << "Configuration entries: " << config.size() << std::endl;

        // Access configuration values with error handling
        auto app_name = config.get<std::string>("app_name");
        auto port = config.get<int>("port");
        auto debug = config.get<bool>("debug");

        if (app_name && port && debug) {
            std::cout << "App: " << *app_name << std::endl;
            std::cout << "Port: " << *port << std::endl;
            std::cout << "Debug: " << (*debug ? "enabled" : "disabled")
                      << std::endl;
        }

        // Safe access with defaults
        int max_conn = config.getOr<int>("max_connections", 50);
        double timeout = config.getOr<double>("timeout", 10.0);
        std::string log_level = config.getOr<std::string>("log_level", "info");

        std::cout << "Max connections: " << max_conn << std::endl;
        std::cout << "Timeout: " << timeout << "s" << std::endl;
        std::cout << "Log level: " << log_level << " (default)" << std::endl;

    } else {
        std::cout << "✗ Configuration load failed: "
                  << load_result.error().error() << std::endl;
    }

    // Example 2: Smart Cache System
    print_header("Smart Cache System");

    SmartCache<std::string, std::string> cache;

    // Add some data to cache
    cache.put("user:1", std::make_shared<std::string>("Alice"));
    cache.put("user:2", std::make_shared<std::string>("Bob"));
    cache.put("user:3", std::make_shared<std::string>("Charlie"));

    std::cout << "Added 3 users to cache" << std::endl;
    std::cout << "Strong cache size: " << cache.strongCacheSize() << std::endl;
    std::cout << "Weak cache size: " << cache.weakCacheSize() << std::endl;

    // Access cached data
    auto user1 = cache.get("user:1");
    if (user1) {
        std::cout << "Retrieved user:1 = " << **user1 << std::endl;
    }

    // Evict to weak cache
    cache.evictToWeak("user:2");
    std::cout << "\nAfter evicting user:2 to weak cache:" << std::endl;
    std::cout << "Strong cache size: " << cache.strongCacheSize() << std::endl;
    std::cout << "Weak cache size: " << cache.weakCacheSize() << std::endl;

    // Try to access evicted user (should still work via weak cache)
    auto user2 = cache.get("user:2");
    if (user2) {
        std::cout << "Retrieved user:2 from weak cache = " << **user2
                  << std::endl;
        std::cout << "Promoted back to strong cache" << std::endl;
    }

    // Example 3: Data Processing Pipeline
    print_header("Data Processing Pipeline");

    DataProcessor processor;

    std::vector<std::string> input_data = {"123", "456", "789", "invalid",
                                           "999"};

    std::cout << "Processing input: ";
    for (const auto& item : input_data) {
        std::cout << item << " ";
    }
    std::cout << std::endl;

    auto result = processor.processNumbers(input_data);
    if (result) {
        std::cout << "✓ Processing successful. Results: ";
        for (int num : *result) {
            std::cout << num << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "✗ Processing failed: " << result.error().error()
                  << std::endl;
    }

    // Try with valid data only
    std::vector<std::string> valid_data = {"100", "200", "300"};
    auto valid_result = processor.processNumbers(valid_data);
    if (valid_result) {
        std::cout << "✓ Valid data processed successfully. Results: ";
        for (int num : *valid_result) {
            std::cout << num << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "\nAll integration examples completed successfully!"
              << std::endl;
    return 0;
}
