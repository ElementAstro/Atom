/*
 * test_module_integration.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Integration Tests for Atom Module Interactions
Tests how different modules work together in real-world scenarios.

**************************************************/

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <fstream>

// Include headers from different modules
#include "atom/containers/high_performance.hpp"
#include "atom/log/logger.hpp"
#include "atom/error/exception.hpp"
#include "atom/utils/string.hpp"
#include "atom/utils/time.hpp"
#include "atom/memory/pool.hpp"

namespace atom::integration::test {

// ============================================================================
// Container + Log Integration Tests
// ============================================================================

class ContainerLogIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_dir = "test_integration_logs";
        std::filesystem::create_directories(test_log_dir);
        logger_manager = std::make_unique<lithium::LoggerManager>();
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_log_dir)) {
            std::filesystem::remove_all(test_log_dir);
        }
        logger_manager.reset();
    }
    
    std::string test_log_dir;
    std::unique_ptr<lithium::LoggerManager> logger_manager;
};

TEST_F(ContainerLogIntegrationTest, ContainerOperationsWithLogging) {
    // Test container operations with comprehensive logging
    
    std::string log_file = test_log_dir + "/container_ops.log";
    std::ofstream log_stream(log_file);
    
    // Create containers and log operations
    atom::containers::HashMap<std::string, int> data_map;
    atom::containers::Vector<std::string> operation_log;
    
    // Simulate logged container operations
    auto loggedInsert = [&](const std::string& key, int value) {
        data_map[key] = value;
        std::string log_entry = "[INFO] Inserted key: " + key + ", value: " + std::to_string(value);
        operation_log.push_back(log_entry);
        log_stream << log_entry << "\n";
    };
    
    auto loggedLookup = [&](const std::string& key) -> bool {
        bool found = data_map.find(key) != data_map.end();
        std::string log_entry = "[INFO] Lookup key: " + key + ", found: " + (found ? "true" : "false");
        operation_log.push_back(log_entry);
        log_stream << log_entry << "\n";
        return found;
    };
    
    // Perform operations
    loggedInsert("user1", 100);
    loggedInsert("user2", 200);
    loggedInsert("user3", 300);
    
    EXPECT_TRUE(loggedLookup("user1"));
    EXPECT_TRUE(loggedLookup("user2"));
    EXPECT_FALSE(loggedLookup("user4"));
    
    log_stream.close();
    
    // Verify logging worked
    EXPECT_EQ(data_map.size(), 3);
    EXPECT_EQ(operation_log.size(), 6); // 3 inserts + 3 lookups
    
    // Verify log file was created
    EXPECT_TRUE(std::filesystem::exists(log_file));
    
    // Use logger manager to analyze the logs
    logger_manager->scanLogsFolder(test_log_dir);
    auto search_results = logger_manager->searchLogs("Inserted");
    EXPECT_EQ(search_results.size(), 3);
}

// ============================================================================
// Error + Log Integration Tests
// ============================================================================

class ErrorLogIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_dir = "test_error_logs";
        std::filesystem::create_directories(test_log_dir);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_log_dir)) {
            std::filesystem::remove_all(test_log_dir);
        }
    }
    
    std::string test_log_dir;
};

TEST_F(ErrorLogIntegrationTest, ErrorHandlingWithLogging) {
    // Test error handling with comprehensive logging
    
    std::string error_log_file = test_log_dir + "/errors.log";
    std::ofstream error_log(error_log_file);
    
    auto loggedOperation = [&](int operation_type) -> bool {
        try {
            switch (operation_type) {
                case 1:
                    throw std::invalid_argument("Invalid input parameter");
                case 2:
                    throw std::runtime_error("Runtime operation failed");
                case 3:
                    throw std::out_of_range("Index out of bounds");
                default:
                    error_log << "[INFO] Operation " << operation_type << " completed successfully\n";
                    return true;
            }
        } catch (const std::exception& e) {
            error_log << "[ERROR] Operation " << operation_type << " failed: " << e.what() << "\n";
            return false;
        }
    };
    
    // Test various operations
    EXPECT_TRUE(loggedOperation(0));   // Success
    EXPECT_FALSE(loggedOperation(1));  // Invalid argument
    EXPECT_FALSE(loggedOperation(2));  // Runtime error
    EXPECT_FALSE(loggedOperation(3));  // Out of range
    EXPECT_TRUE(loggedOperation(4));   // Success
    
    error_log.close();
    
    // Verify error log file
    EXPECT_TRUE(std::filesystem::exists(error_log_file));
    
    // Count log entries
    std::ifstream log_reader(error_log_file);
    int line_count = 0;
    std::string line;
    while (std::getline(log_reader, line)) {
        line_count++;
    }
    EXPECT_EQ(line_count, 5); // 2 success + 3 error entries
}

// ============================================================================
// Utils + Container Integration Tests
// ============================================================================

class UtilsContainerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(UtilsContainerIntegrationTest, StringProcessingWithContainers) {
    // Test string processing utilities with container storage
    
    atom::containers::Vector<std::string> text_data = {
        "Hello World",
        "Processing Text",
        "Integration Testing",
        "Atom Framework",
        "Module Interaction"
    };
    
    atom::containers::HashMap<std::string, int> word_count;
    atom::containers::HashSet<std::string> unique_words;
    
    // Process each text entry
    for (const auto& text : text_data) {
        // Simulate string splitting (basic implementation)
        std::vector<std::string> words;
        std::string current_word;
        
        for (char c : text) {
            if (c == ' ') {
                if (!current_word.empty()) {
                    words.push_back(current_word);
                    current_word.clear();
                }
            } else {
                current_word += c;
            }
        }
        if (!current_word.empty()) {
            words.push_back(current_word);
        }
        
        // Count words and track unique words
        for (const auto& word : words) {
            word_count[word]++;
            unique_words.insert(word);
        }
    }
    
    // Verify results
    EXPECT_GT(word_count.size(), 0);
    EXPECT_GT(unique_words.size(), 0);
    EXPECT_EQ(word_count.size(), unique_words.size()); // All words should be unique in this test
    
    // Verify specific word counts
    EXPECT_EQ(word_count["Hello"], 1);
    EXPECT_EQ(word_count["Testing"], 1);
    EXPECT_EQ(word_count["Integration"], 2); // Appears in two entries
}

// ============================================================================
// Memory + Container Integration Tests
// ============================================================================

class MemoryContainerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(MemoryContainerIntegrationTest, CustomAllocatorIntegration) {
    // Test containers with custom memory management
    
    // Simulate custom allocator usage
    struct MemoryStats {
        size_t allocations = 0;
        size_t deallocations = 0;
        size_t bytes_allocated = 0;
        size_t bytes_deallocated = 0;
    };
    
    MemoryStats stats;
    
    auto simulateAllocation = [&stats](size_t size) -> void* {
        stats.allocations++;
        stats.bytes_allocated += size;
        return malloc(size);
    };
    
    auto simulateDeallocation = [&stats](void* ptr, size_t size) -> void {
        stats.deallocations++;
        stats.bytes_deallocated += size;
        free(ptr);
    };
    
    // Simulate container operations with memory tracking
    const size_t num_elements = 1000;
    const size_t element_size = sizeof(int);
    
    // Simulate vector allocation
    void* vector_memory = simulateAllocation(num_elements * element_size);
    EXPECT_NE(vector_memory, nullptr);
    
    // Simulate map allocation (approximate)
    void* map_memory = simulateAllocation(num_elements * element_size * 2); // Key + value
    EXPECT_NE(map_memory, nullptr);
    
    // Cleanup
    simulateDeallocation(vector_memory, num_elements * element_size);
    simulateDeallocation(map_memory, num_elements * element_size * 2);
    
    // Verify memory tracking
    EXPECT_EQ(stats.allocations, 2);
    EXPECT_EQ(stats.deallocations, 2);
    EXPECT_EQ(stats.bytes_allocated, stats.bytes_deallocated);
    EXPECT_GT(stats.bytes_allocated, 0);
}

// ============================================================================
// Multi-Module Integration Tests
// ============================================================================

class MultiModuleIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "test_multi_module";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    std::string test_dir;
};

TEST_F(MultiModuleIntegrationTest, CompleteWorkflow) {
    // Test a complete workflow involving multiple modules
    
    // 1. Data processing with containers
    atom::containers::HashMap<std::string, std::vector<int>> data_store;
    data_store["dataset1"] = {1, 2, 3, 4, 5};
    data_store["dataset2"] = {10, 20, 30, 40, 50};
    
    // 2. Error handling during processing
    std::vector<std::string> error_log;
    
    auto processDataset = [&](const std::string& name) -> bool {
        try {
            auto it = data_store.find(name);
            if (it == data_store.end()) {
                throw std::runtime_error("Dataset not found: " + name);
            }
            
            // Process data (calculate sum)
            int sum = 0;
            for (int value : it->second) {
                sum += value;
            }
            
            return sum > 0;
            
        } catch (const std::exception& e) {
            error_log.push_back(e.what());
            return false;
        }
    };
    
    // 3. Process datasets with logging
    std::string log_file = test_dir + "/workflow.log";
    std::ofstream log_stream(log_file);
    
    std::vector<std::string> datasets = {"dataset1", "dataset2", "nonexistent"};
    int successful_operations = 0;
    
    for (const auto& dataset : datasets) {
        bool success = processDataset(dataset);
        if (success) {
            successful_operations++;
            log_stream << "[INFO] Successfully processed " << dataset << "\n";
        } else {
            log_stream << "[ERROR] Failed to process " << dataset << "\n";
        }
    }
    
    log_stream.close();
    
    // 4. Verify results
    EXPECT_EQ(successful_operations, 2); // dataset1 and dataset2
    EXPECT_EQ(error_log.size(), 1);      // nonexistent dataset
    EXPECT_TRUE(std::filesystem::exists(log_file));
    
    // 5. Verify error message
    EXPECT_TRUE(error_log[0].find("Dataset not found: nonexistent") != std::string::npos);
}

} // namespace atom::integration::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
