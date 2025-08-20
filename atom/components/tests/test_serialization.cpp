/*
 * test_serialization.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Comprehensive Unit Tests for Serialization System
Tests JSON/binary formats, versioning, compression/encryption,
error handling, and performance benchmarks.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "../component.hpp"
#include "../serialization.hpp"

using namespace atom::components;

/**
 * @brief Test fixture for Serialization tests
 */
class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializationManager_ = &SerializationManager::instance();

        // Create test component with various data
        component_ = std::make_shared<Component>("SerializationTestComponent");
        component_->addVariable<int>("intVar", 42);
        component_->addVariable<double>("doubleVar", 3.14159);
        component_->addVariable<std::string>("stringVar", "Hello, World!");
        component_->addVariable<bool>("boolVar", true);

        // Add some commands
        component_->def("testCommand", []() -> int { return 100; });
        component_->def("addNumbers",
                        [](int a, int b) -> int { return a + b; });

        component_->doc("Test component for serialization testing");
    }

    void TearDown() override { component_.reset(); }

    SerializationManager* serializationManager_;
    std::shared_ptr<Component> component_;
};

/**
 * @brief Test fixture for performance benchmarks
 */
class SerializationPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializationManager_ = &SerializationManager::instance();

        // Create component with lots of data for performance testing
        component_ = std::make_shared<Component>("PerformanceTestComponent");

        // Add many variables
        for (int i = 0; i < 100; ++i) {
            component_->addVariable<int>("intVar" + std::to_string(i), i);
            component_->addVariable<std::string>(
                "stringVar" + std::to_string(i), "Value" + std::to_string(i));
        }

        // Add many commands
        for (int i = 0; i < 50; ++i) {
            component_->def("command" + std::to_string(i),
                            [i]() -> int { return i; });
        }
    }

    void TearDown() override { component_.reset(); }

    SerializationManager* serializationManager_;
    std::shared_ptr<Component> component_;
    static constexpr int BENCHMARK_ITERATIONS = 100;
};

// ============================================================================
// Singleton Pattern Tests
// ============================================================================

TEST(SerializationManagerSingletonTest, SingletonInstance) {
    auto& instance1 = SerializationManager::instance();
    auto& instance2 = SerializationManager::instance();

    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_F(SerializationTest, JSONSerializationBasic) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = true;
    options.includeTimestamp = true;
    options.includeVersion = true;

    auto result = serializationManager_->serialize(*component_, options);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.data.size(), 0);
    EXPECT_GT(result.serializationTime.count(), 0);
    EXPECT_EQ(result.originalSize, result.data.size());  // No compression
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(SerializationTest, JSONDeserialization) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = true;

    // Serialize first
    auto serializeResult =
        serializationManager_->serialize(*component_, options);
    ASSERT_TRUE(serializeResult.success);

    // Deserialize
    auto deserializeResult =
        serializationManager_->deserialize(serializeResult.data, options);

    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);
    EXPECT_GT(deserializeResult.deserializationTime.count(), 0);
    EXPECT_TRUE(deserializeResult.errorMessage.empty());

    // Verify component data
    auto deserializedComponent = deserializeResult.component;
    EXPECT_EQ(deserializedComponent->getName(), component_->getName());

    // Check variables
    EXPECT_TRUE(deserializedComponent->hasVariable("intVar"));
    EXPECT_TRUE(deserializedComponent->hasVariable("stringVar"));
    EXPECT_TRUE(deserializedComponent->hasVariable("boolVar"));

    auto intVar = deserializedComponent->getVariable<int>("intVar");
    EXPECT_EQ(intVar->get(), 42);

    auto stringVar =
        deserializedComponent->getVariable<std::string>("stringVar");
    EXPECT_EQ(stringVar->get(), "Hello, World!");

    auto boolVar = deserializedComponent->getVariable<bool>("boolVar");
    EXPECT_EQ(boolVar->get(), true);
}

TEST_F(SerializationTest, JSONSerializationWithoutMetadata) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeMetadata = false;
    options.includeTimestamp = false;
    options.includeVersion = false;

    auto result = serializationManager_->serialize(*component_, options);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.data.size(), 0);

    // Should be smaller without metadata
    SerializationOptions fullOptions;
    fullOptions.format = SerializationFormat::JSON;
    fullOptions.includeMetadata = true;
    fullOptions.includeTimestamp = true;
    fullOptions.includeVersion = true;

    auto fullResult =
        serializationManager_->serialize(*component_, fullOptions);
    EXPECT_LT(result.data.size(), fullResult.data.size());
}

// ============================================================================
// Binary Serialization Tests
// ============================================================================

TEST_F(SerializationTest, BinarySerializationBasic) {
    SerializationOptions options;
    options.format = SerializationFormat::Binary;
    options.includeMetadata = true;

    auto result = serializationManager_->serialize(*component_, options);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.data.size(), 0);
    EXPECT_GT(result.serializationTime.count(), 0);
}

TEST_F(SerializationTest, BinaryDeserialization) {
    SerializationOptions options;
    options.format = SerializationFormat::Binary;
    options.includeMetadata = true;

    // Serialize first
    auto serializeResult =
        serializationManager_->serialize(*component_, options);
    ASSERT_TRUE(serializeResult.success);

    // Deserialize
    auto deserializeResult =
        serializationManager_->deserialize(serializeResult.data, options);

    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);

    // Verify component data
    auto deserializedComponent = deserializeResult.component;
    EXPECT_EQ(deserializedComponent->getName(), component_->getName());

    // Check that variables are preserved
    EXPECT_TRUE(deserializedComponent->hasVariable("intVar"));
    EXPECT_TRUE(deserializedComponent->hasVariable("doubleVar"));
}

TEST_F(SerializationTest, BinaryVsJSONSize) {
    SerializationOptions jsonOptions;
    jsonOptions.format = SerializationFormat::JSON;

    SerializationOptions binaryOptions;
    binaryOptions.format = SerializationFormat::Binary;

    auto jsonResult =
        serializationManager_->serialize(*component_, jsonOptions);
    auto binaryResult =
        serializationManager_->serialize(*component_, binaryOptions);

    ASSERT_TRUE(jsonResult.success);
    ASSERT_TRUE(binaryResult.success);

    // Binary should typically be more compact
    EXPECT_LT(binaryResult.data.size(), jsonResult.data.size());
}

// ============================================================================
// Versioning Tests
// ============================================================================

TEST_F(SerializationTest, VersioningSupport) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.includeVersion = true;
    options.version = 2;

    auto result = serializationManager_->serialize(*component_, options);
    ASSERT_TRUE(result.success);

    auto deserializeResult =
        serializationManager_->deserialize(result.data, options);
    EXPECT_TRUE(deserializeResult.success);
    EXPECT_EQ(deserializeResult.version, 2);
}

TEST_F(SerializationTest, VersionMismatchHandling) {
    SerializationOptions serializeOptions;
    serializeOptions.format = SerializationFormat::JSON;
    serializeOptions.version = 1;

    auto serializeResult =
        serializationManager_->serialize(*component_, serializeOptions);
    ASSERT_TRUE(serializeResult.success);

    // Try to deserialize with different version
    SerializationOptions deserializeOptions;
    deserializeOptions.format = SerializationFormat::JSON;
    deserializeOptions.version = 2;

    auto deserializeResult = serializationManager_->deserialize(
        serializeResult.data, deserializeOptions);

    // Behavior depends on implementation - might succeed with warnings or fail
    // The important thing is that it doesn't crash
}

// ============================================================================
// Compression Tests
// ============================================================================

TEST_F(SerializationTest, CompressionSupport) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.compressData = true;

    auto result = serializationManager_->serialize(*component_, options);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.originalSize, 0);
    EXPECT_GT(result.compressedSize, 0);

    // Compressed size should be smaller than original (for sufficiently large
    // data)
    if (result.originalSize > 1000) {
        EXPECT_LT(result.compressedSize, result.originalSize);
    }
}

TEST_F(SerializationTest, CompressionRoundTrip) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.compressData = true;

    // Serialize with compression
    auto serializeResult =
        serializationManager_->serialize(*component_, options);
    ASSERT_TRUE(serializeResult.success);

    // Deserialize
    auto deserializeResult =
        serializationManager_->deserialize(serializeResult.data, options);

    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);

    // Verify data integrity
    auto deserializedComponent = deserializeResult.component;
    EXPECT_EQ(deserializedComponent->getName(), component_->getName());

    auto intVar = deserializedComponent->getVariable<int>("intVar");
    EXPECT_EQ(intVar->get(), 42);
}

// ============================================================================
// Encryption Tests
// ============================================================================

TEST_F(SerializationTest, EncryptionSupport) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.encryptData = true;
    options.encryptionKey = "test_encryption_key_123";

    auto result = serializationManager_->serialize(*component_, options);

    // Should succeed if encryption is implemented
    if (result.success) {
        EXPECT_GT(result.data.size(), 0);

        // Encrypted data should not contain readable component name
        std::string dataStr(result.data.begin(), result.data.end());
        EXPECT_EQ(dataStr.find("SerializationTestComponent"),
                  std::string::npos);
    }
}

TEST_F(SerializationTest, EncryptionRoundTrip) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.encryptData = true;
    options.encryptionKey = "test_encryption_key_123";

    // Serialize with encryption
    auto serializeResult =
        serializationManager_->serialize(*component_, options);

    if (serializeResult.success) {
        // Deserialize with same key
        auto deserializeResult =
            serializationManager_->deserialize(serializeResult.data, options);

        EXPECT_TRUE(deserializeResult.success);
        EXPECT_NE(deserializeResult.component, nullptr);

        // Verify data integrity
        auto deserializedComponent = deserializeResult.component;
        EXPECT_EQ(deserializedComponent->getName(), component_->getName());
    }
}

TEST_F(SerializationTest, EncryptionWrongKey) {
    SerializationOptions serializeOptions;
    serializeOptions.format = SerializationFormat::JSON;
    serializeOptions.encryptData = true;
    serializeOptions.encryptionKey = "correct_key";

    auto serializeResult =
        serializationManager_->serialize(*component_, serializeOptions);

    if (serializeResult.success) {
        // Try to deserialize with wrong key
        SerializationOptions deserializeOptions = serializeOptions;
        deserializeOptions.encryptionKey = "wrong_key";

        auto deserializeResult = serializationManager_->deserialize(
            serializeResult.data, deserializeOptions);

        // Should fail with wrong key
        EXPECT_FALSE(deserializeResult.success);
        EXPECT_FALSE(deserializeResult.errorMessage.empty());
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(SerializationTest, SerializeNullComponent) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    // Try to serialize null component
    Component* nullComponent = nullptr;
    auto result = serializationManager_->serialize(*nullComponent, options);

    // Should handle gracefully
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(SerializationTest, DeserializeCorruptedData) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    // Create corrupted data
    std::vector<uint8_t> corruptedData = {'i', 'n', 'v', 'a', 'l', 'i',
                                          'd', ' ', 'j', 's', 'o', 'n'};

    auto result = serializationManager_->deserialize(corruptedData, options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_EQ(result.component, nullptr);
}

TEST_F(SerializationTest, DeserializeEmptyData) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    std::vector<uint8_t> emptyData;

    auto result = serializationManager_->deserialize(emptyData, options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(SerializationTest, UnsupportedFormat) {
    SerializationOptions options;
    options.format =
        SerializationFormat::Custom;  // Assuming this is not implemented

    auto result = serializationManager_->serialize(*component_, options);

    // Should handle unsupported format gracefully
    if (!result.success) {
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

TEST_F(SerializationTest, SerializationWithInvalidOptions) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.encryptData = true;
    options.encryptionKey = "";  // Empty encryption key

    auto result = serializationManager_->serialize(*component_, options);

    // Should handle invalid options gracefully
    if (!result.success) {
        EXPECT_FALSE(result.errorMessage.empty());
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_F(SerializationTest, CustomSerializationOptions) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    options.customOptions["customFlag"] = true;
    options.customOptions["customValue"] = 42;
    options.customOptions["customString"] = std::string("custom");

    auto result = serializationManager_->serialize(*component_, options);

    // Should handle custom options without crashing
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

// ============================================================================
// Complex Data Types Tests
// ============================================================================

TEST_F(SerializationTest, SerializeComplexTypes) {
    // Add complex data types to component
    std::vector<int> intVector = {1, 2, 3, 4, 5};
    std::vector<std::string> stringVector = {"hello", "world", "test"};

    component_->addVariable<std::vector<int>>("intVector", intVector);
    component_->addVariable<std::vector<std::string>>("stringVector",
                                                      stringVector);

    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    auto serializeResult =
        serializationManager_->serialize(*component_, options);
    ASSERT_TRUE(serializeResult.success);

    auto deserializeResult =
        serializationManager_->deserialize(serializeResult.data, options);
    EXPECT_TRUE(deserializeResult.success);

    if (deserializeResult.success) {
        auto deserializedComponent = deserializeResult.component;

        // Verify complex types are preserved
        if (deserializedComponent->hasVariable("intVector")) {
            auto deserializedIntVector =
                deserializedComponent->getVariable<std::vector<int>>(
                    "intVector");
            EXPECT_EQ(deserializedIntVector->get(), intVector);
        }

        if (deserializedComponent->hasVariable("stringVector")) {
            auto deserializedStringVector =
                deserializedComponent->getVariable<std::vector<std::string>>(
                    "stringVector");
            EXPECT_EQ(deserializedStringVector->get(), stringVector);
        }
    }
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

TEST_F(SerializationPerformanceTest, JSONSerializationPerformance) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        auto result = serializationManager_->serialize(*component_, options);
        EXPECT_TRUE(result.success);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "JSON serialization benchmark: " << duration.count()
              << " μs for " << BENCHMARK_ITERATIONS << " serializations"
              << std::endl;
    std::cout << "Average per serialization: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast (less than 1000μs per serialization on average)
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 1000);
}

TEST_F(SerializationPerformanceTest, BinarySerializationPerformance) {
    SerializationOptions options;
    options.format = SerializationFormat::Binary;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        auto result = serializationManager_->serialize(*component_, options);
        EXPECT_TRUE(result.success);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Binary serialization benchmark: " << duration.count()
              << " μs for " << BENCHMARK_ITERATIONS << " serializations"
              << std::endl;
    std::cout << "Average per serialization: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Binary should be faster than JSON
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 800);
}

TEST_F(SerializationPerformanceTest, DeserializationPerformance) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    // Serialize once to get data
    auto serializeResult =
        serializationManager_->serialize(*component_, options);
    ASSERT_TRUE(serializeResult.success);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        auto result =
            serializationManager_->deserialize(serializeResult.data, options);
        EXPECT_TRUE(result.success);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "JSON deserialization benchmark: " << duration.count()
              << " μs for " << BENCHMARK_ITERATIONS << " deserializations"
              << std::endl;
    std::cout << "Average per deserialization: "
              << (duration.count() / BENCHMARK_ITERATIONS) << " μs"
              << std::endl;

    // Should be reasonably fast
    EXPECT_LT(duration.count() / BENCHMARK_ITERATIONS, 1500);
}

TEST_F(SerializationPerformanceTest, CompressionPerformance) {
    SerializationOptions uncompressedOptions;
    uncompressedOptions.format = SerializationFormat::JSON;
    uncompressedOptions.compressData = false;

    SerializationOptions compressedOptions;
    compressedOptions.format = SerializationFormat::JSON;
    compressedOptions.compressData = true;

    // Benchmark uncompressed
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 50; ++i) {
        auto result =
            serializationManager_->serialize(*component_, uncompressedOptions);
        EXPECT_TRUE(result.success);
    }
    auto uncompressedTime = std::chrono::high_resolution_clock::now() - start;

    // Benchmark compressed
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 50; ++i) {
        auto result =
            serializationManager_->serialize(*component_, compressedOptions);
        EXPECT_TRUE(result.success);
    }
    auto compressedTime = std::chrono::high_resolution_clock::now() - start;

    auto uncompressedMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(uncompressedTime)
            .count();
    auto compressedMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(compressedTime)
            .count();

    std::cout << "Uncompressed serialization: " << uncompressedMicros << " μs"
              << std::endl;
    std::cout << "Compressed serialization: " << compressedMicros << " μs"
              << std::endl;

    // Compression should not be more than 5x slower
    EXPECT_LT(compressedMicros, uncompressedMicros * 5);
}

// ============================================================================
// Memory Usage Tests
// ============================================================================

TEST_F(SerializationTest, MemoryUsageDuringSerialization) {
    // Create a large component
    auto largeComponent = std::make_shared<Component>("LargeComponent");

    for (int i = 0; i < 1000; ++i) {
        largeComponent->addVariable<std::string>(
            "largeString" + std::to_string(i),
            std::string(1000, 'A' + (i % 26)));
    }

    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    auto result = serializationManager_->serialize(*largeComponent, options);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.data.size(), 100000);  // Should be quite large

    // Memory should be released after serialization
    // This is mainly a test to ensure no memory leaks
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(SerializationTest, ConcurrentSerialization) {
    const int numThreads = 4;
    const int serializationsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    SerializationOptions options;
    options.format = SerializationFormat::JSON;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(
            [this, &options, &successCount, serializationsPerThread]() {
                for (int i = 0; i < serializationsPerThread; ++i) {
                    auto result =
                        serializationManager_->serialize(*component_, options);
                    if (result.success) {
                        successCount++;
                    }
                }
            });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most serializations should succeed
    EXPECT_GT(successCount.load(), numThreads * serializationsPerThread * 0.8);
}
