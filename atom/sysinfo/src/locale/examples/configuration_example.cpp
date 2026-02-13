/**
 * @file configuration_example.cpp
 * @brief Configuration and caching demonstration for the locale module
 *
 * This example demonstrates configuration management, caching strategies,
 * performance monitoring, and advanced cache operations.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "../config.hpp"
#include "../locale.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace atom::system::locale;

void demonstrateConfigurationManagement() {
    std::cout << "=== Configuration Management ===" << std::endl;

    auto& configManager = LocaleConfigManager::getInstance();

    // Show default configuration
    auto config = configManager.getConfig();
    std::cout << "Default Configuration:" << std::endl;
    std::cout << "  Default Locale: " << config.defaultLocale << std::endl;
    std::cout << "  Enable Validation: " << (config.enableValidation ? "Yes" : "No") << std::endl;
    std::cout << "  Enable Compatibility Checks: " << (config.enableCompatibilityChecks ? "Yes" : "No") << std::endl;
    std::cout << "  Log Level: " << static_cast<int>(config.logLevel) << std::endl;
    std::cout << "  Cache Strategy: " << static_cast<int>(config.cache.strategy) << std::endl;
    std::cout << "  Cache Timeout: " << config.cache.timeout.count() << " seconds" << std::endl;
    std::cout << "  Max Memory Entries: " << config.cache.maxMemoryEntries << std::endl;
    std::cout << "  Enable Lazy Loading: " << (config.performance.enableLazyLoading ? "Yes" : "No") << std::endl;
    std::cout << "  Thread Pool Size: " << config.performance.threadPoolSize << std::endl;
    std::cout << std::endl;

    // Modify configuration
    std::cout << "Modifying configuration..." << std::endl;
    config.defaultLocale = "de_DE.UTF-8";
    config.cache.strategy = CacheStrategy::Hybrid;
    config.cache.timeout = std::chrono::seconds(600);
    config.cache.maxMemoryEntries = 500;
    config.performance.enableMetrics = true;
    config.performance.threadPoolSize = 8;

    configManager.setConfig(config);
    std::cout << "Configuration updated!" << std::endl;

    // Show updated configuration
    auto updatedConfig = configManager.getConfig();
    std::cout << "Updated Configuration:" << std::endl;
    std::cout << "  Default Locale: " << updatedConfig.defaultLocale << std::endl;
    std::cout << "  Cache Strategy: " << static_cast<int>(updatedConfig.cache.strategy) << std::endl;
    std::cout << "  Cache Timeout: " << updatedConfig.cache.timeout.count() << " seconds" << std::endl;
    std::cout << "  Max Memory Entries: " << updatedConfig.cache.maxMemoryEntries << std::endl;
    std::cout << "  Enable Metrics: " << (updatedConfig.performance.enableMetrics ? "Yes" : "No") << std::endl;
    std::cout << "  Thread Pool Size: " << updatedConfig.performance.threadPoolSize << std::endl;
    std::cout << std::endl;

    // Test configuration validation
    bool isValid = configManager.validateConfig();
    std::cout << "Configuration is " << (isValid ? "valid" : "invalid") << std::endl;
    std::cout << std::endl;
}

void demonstrateEnvironmentConfiguration() {
    std::cout << "=== Environment Configuration ===" << std::endl;

    auto& configManager = LocaleConfigManager::getInstance();

    std::cout << "Loading configuration from environment variables..." << std::endl;
    std::cout << "Set these environment variables to test:" << std::endl;
    std::cout << "  export LOCALE_DEFAULT=fr_FR.UTF-8" << std::endl;
    std::cout << "  export LOCALE_CACHE_STRATEGY=2" << std::endl;
    std::cout << "  export LOCALE_CACHE_TIMEOUT=900" << std::endl;
    std::cout << "  export LOCALE_METRICS=true" << std::endl;
    std::cout << std::endl;

    configManager.loadFromEnvironment();

    auto config = configManager.getConfig();
    std::cout << "Configuration after loading from environment:" << std::endl;
    std::cout << "  Default Locale: " << config.defaultLocale << std::endl;
    std::cout << "  Cache Strategy: " << static_cast<int>(config.cache.strategy) << std::endl;
    std::cout << "  Cache Timeout: " << config.cache.timeout.count() << " seconds" << std::endl;
    std::cout << "  Enable Metrics: " << (config.performance.enableMetrics ? "Yes" : "No") << std::endl;
    std::cout << std::endl;
}

void demonstrateConfigurationSerialization() {
    std::cout << "=== Configuration Serialization ===" << std::endl;

    auto& configManager = LocaleConfigManager::getInstance();

    // Export to JSON
    std::cout << "Configuration as JSON:" << std::endl;
    std::string json = configManager.toJson();
    std::cout << json << std::endl;
    std::cout << std::endl;

    // Save to file
    std::string configFile = "/tmp/locale_config.conf";
    bool saved = configManager.saveToFile(configFile);
    std::cout << "Configuration " << (saved ? "saved to" : "failed to save to")
              << " " << configFile << std::endl;

    // Reset and load from file
    configManager.resetToDefaults();
    std::cout << "Configuration reset to defaults" << std::endl;

    bool loaded = configManager.loadFromFile(configFile);
    std::cout << "Configuration " << (loaded ? "loaded from" : "failed to load from")
              << " " << configFile << std::endl;
    std::cout << std::endl;
}

void demonstrateCaching() {
    std::cout << "=== Caching Demonstration ===" << std::endl;

    // Create cache with different strategies
    std::vector<std::pair<std::string, CacheStrategy>> strategies = {
        {"Memory", CacheStrategy::Memory},
        {"Hybrid", CacheStrategy::Hybrid}
    };

    for (const auto& [name, strategy] : strategies) {
        std::cout << "Testing " << name << " caching strategy:" << std::endl;

        CacheConfig cacheConfig;
        cacheConfig.strategy = strategy;
        cacheConfig.timeout = std::chrono::seconds(5);
        cacheConfig.maxMemoryEntries = 10;

        LocaleCache cache(cacheConfig);

        // Store some locale information
        atom::system::LocaleManager manager;
        auto localeInfo = manager.getCurrentLocale();

        std::cout << "  Storing locale info for key 'current'..." << std::endl;
        cache.store("current", localeInfo);

        // Check if it exists
        bool exists = cache.exists("current");
        std::cout << "  Key 'current' exists: " << (exists ? "Yes" : "No") << std::endl;

        // Retrieve from cache
        auto cached = cache.retrieve("current");
        if (cached) {
            std::cout << "  Retrieved from cache: " << cached->localeName << std::endl;
        } else {
            std::cout << "  Failed to retrieve from cache" << std::endl;
        }

        // Store multiple entries
        std::vector<std::string> testLocales = {
            "en_US.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "es_ES.UTF-8", "it_IT.UTF-8"
        };

        for (const auto& locale : testLocales) {
            LocaleInfo info = localeInfo; // Copy base info
            info.localeName = locale;
            cache.store(locale, info);
        }

        // Show cache statistics
        auto stats = cache.getStatistics();
        std::cout << "  Cache Statistics:" << std::endl;
        for (const auto& [key, value] : stats) {
            std::cout << "    " << key << ": " << value << std::endl;
        }

        // Test cache expiration
        std::cout << "  Waiting for cache expiration..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(6));

        auto expiredCached = cache.retrieve("current");
        std::cout << "  After expiration, cached entry "
                  << (expiredCached ? "still exists" : "has expired") << std::endl;

        // Perform maintenance
        cache.maintenance();
        auto statsAfterMaintenance = cache.getStatistics();
        std::cout << "  Entries after maintenance: " << statsAfterMaintenance["entries"] << std::endl;

        std::cout << std::endl;
    }
}

void demonstratePerformanceMonitoring() {
    std::cout << "=== Performance Monitoring ===" << std::endl;

    auto& monitor = PerformanceMonitor::getInstance();
    monitor.setEnabled(true);

    std::cout << "Performance monitoring enabled" << std::endl;

    // Simulate some operations
    std::vector<std::string> operations = {
        "locale_detection", "locale_validation", "locale_formatting", "cache_lookup"
    };

    std::cout << "Simulating operations..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        for (const auto& operation : operations) {
            // Simulate operation timing
            auto start = std::chrono::high_resolution_clock::now();

            // Simulate work (random delay)
            std::this_thread::sleep_for(std::chrono::microseconds(rand() % 1000 + 100));

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double timeMs = duration.count() / 1000.0;

            bool success = (rand() % 10) != 0; // 90% success rate
            monitor.recordOperation(operation, timeMs, success);

            // Simulate cache hits/misses
            if (operation == "cache_lookup") {
                if (rand() % 3 == 0) {
                    monitor.recordCacheMiss(operation);
                } else {
                    monitor.recordCacheHit(operation);
                }
            }
        }
    }

    // Show performance metrics
    std::cout << "Performance Metrics:" << std::endl;
    auto allMetrics = monitor.getAllMetrics();

    std::cout << std::setw(20) << "Operation"
              << std::setw(10) << "Total"
              << std::setw(10) << "Success"
              << std::setw(10) << "Failed"
              << std::setw(12) << "Avg Time"
              << std::setw(10) << "Cache Hit"
              << std::setw(10) << "Cache Miss" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& [operation, metrics] : allMetrics) {
        std::cout << std::setw(20) << operation
                  << std::setw(10) << metrics.totalOperations
                  << std::setw(10) << metrics.successfulOperations
                  << std::setw(10) << metrics.failedOperations
                  << std::setw(12) << std::fixed << std::setprecision(3) << metrics.averageExecutionTime
                  << std::setw(10) << metrics.cacheHits
                  << std::setw(10) << metrics.cacheMisses << std::endl;
    }

    std::cout << std::endl;
}

void demonstratePerformanceTimer() {
    std::cout << "=== Performance Timer ===" << std::endl;

    auto& monitor = PerformanceMonitor::getInstance();
    monitor.setEnabled(true);

    // Demonstrate automatic timing with RAII
    {
        LOCALE_PERFORMANCE_TIMER("automatic_timing_test");
        std::cout << "Performing timed operation..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } // Timer automatically records when going out of scope

    // Demonstrate manual timer with failure marking
    {
        PerformanceTimer timer("manual_timing_test");
        std::cout << "Performing operation that might fail..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // Simulate failure condition
        if (rand() % 2 == 0) {
            timer.markFailed();
            std::cout << "Operation failed!" << std::endl;
        } else {
            std::cout << "Operation succeeded!" << std::endl;
        }
    }

    // Show the recorded metrics
    auto timerMetrics = monitor.getAllMetrics();
    std::cout << "Timer Metrics:" << std::endl;
    for (const auto& [operation, metrics] : timerMetrics) {
        if (operation.find("timing_test") != std::string::npos) {
            std::cout << "  " << operation << ": "
                      << metrics.averageExecutionTime << " ms average" << std::endl;
        }
    }

    std::cout << std::endl;
}

int main() {
    std::cout << "Locale Module - Configuration and Caching Example" << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << std::endl;

    try {
        demonstrateConfigurationManagement();
        demonstrateEnvironmentConfiguration();
        demonstrateConfigurationSerialization();
        demonstrateCaching();
        demonstratePerformanceMonitoring();
        demonstratePerformanceTimer();

        std::cout << "Configuration and caching example completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
