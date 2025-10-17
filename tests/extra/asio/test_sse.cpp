#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Temporarily disable SSE tests due to missing dependencies
#if 0
#include "atom/extra/asio/sse/event.hpp"
#include "atom/extra/asio/sse/event_store.hpp"
#include "atom/extra/asio/sse/sse.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using namespace testing;
using namespace atom::extra::asio::sse;

namespace atom::extra::asio::test {

class SseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for event store tests
        temp_dir_ = std::filesystem::temp_directory_path() / "sse_test";
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        // Cleanup temporary directory
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
    }

    std::filesystem::path temp_dir_;
};

// Test SSE event creation with string data
TEST_F(SseTest, EventCreationString) {
    std::string id = "event_001";
    std::string type = "message";
    std::string data = "Hello, SSE!";

    Event event(id, type, data);

    EXPECT_EQ(event.id(), id);
    EXPECT_EQ(event.event_type(), type);
    EXPECT_EQ(event.data(), data);
    EXPECT_GT(event.timestamp(), 0);
    EXPECT_FALSE(event.is_json());
    EXPECT_FALSE(event.is_compressed());
}

// Test SSE event creation with metadata
TEST_F(SseTest, EventCreationWithMetadata) {
    std::string id = "event_002";
    std::string type = "notification";
    std::string data = "Test notification";
    std::unordered_map<std::string, std::string> metadata = {
        {"priority", "high"},
        {"source", "test_system"}
    };

    Event event(id, type, data, metadata);

    EXPECT_EQ(event.id(), id);
    EXPECT_EQ(event.event_type(), type);
    EXPECT_EQ(event.data(), data);
    EXPECT_EQ(event.metadata().at("priority"), "high");
    EXPECT_EQ(event.metadata().at("source"), "test_system");
}

// Test SSE event creation with JSON data
TEST_F(SseTest, EventCreationJson) {
    std::string id = "event_003";
    std::string type = "json_message";
    nlohmann::json json_data = {
        {"message", "Hello, JSON!"},
        {"count", 42},
        {"active", true}
    };

    Event event(id, type, json_data);

    EXPECT_EQ(event.id(), id);
    EXPECT_EQ(event.event_type(), type);
    EXPECT_TRUE(event.is_json());
    EXPECT_FALSE(event.is_compressed());

    // Verify JSON data can be parsed back
    auto parsed_json = nlohmann::json::parse(event.data());
    EXPECT_EQ(parsed_json["message"], "Hello, JSON!");
    EXPECT_EQ(parsed_json["count"], 42);
    EXPECT_EQ(parsed_json["active"], true);
}

// Test event serialization to SSE format
TEST_F(SseTest, EventSerialization) {
    std::string id = "event_004";
    std::string type = "test";
    std::string data = "Test data\nwith newlines";

    Event event(id, type, data);
    std::string serialized = event.serialize();

    // Check SSE format
    EXPECT_THAT(serialized, HasSubstr("id: " + id));
    EXPECT_THAT(serialized, HasSubstr("event: " + type));
    EXPECT_THAT(serialized, HasSubstr("data: Test data"));
    EXPECT_THAT(serialized, HasSubstr("data: with newlines"));
    EXPECT_THAT(serialized, EndsWith("\n\n"));
}

// Test event serialization with retry field
TEST_F(SseTest, EventSerializationWithRetry) {
    std::string id = "event_005";
    std::string type = "retry_test";
    std::string data = "Retry test data";

    Event event(id, type, data);
    event.set_retry(5000); // 5 seconds

    std::string serialized = event.serialize();

    EXPECT_THAT(serialized, HasSubstr("retry: 5000"));
    EXPECT_THAT(serialized, HasSubstr("id: " + id));
    EXPECT_THAT(serialized, HasSubstr("event: " + type));
    EXPECT_THAT(serialized, HasSubstr("data: " + data));
}

// Test event store basic operations
TEST_F(SseTest, EventStoreBasicOperations) {
    EventStore store(temp_dir_.string());

    std::string id = "store_test_001";
    std::string type = "test";
    std::string data = "Store test data";

    Event event(id, type, data);

    // Initially, event should not be seen
    EXPECT_FALSE(store.has_seen_event(id));

    // Store the event
    store.store_event(event);

    // Now it should be seen
    EXPECT_TRUE(store.has_seen_event(id));

    // Latest event ID should be this event
    EXPECT_EQ(store.get_latest_event_id(), id);
}

// Test event store persistence
TEST_F(SseTest, EventStorePersistence) {
    std::string id = "persist_test_001";
    std::string type = "persistence";
    std::string data = "Persistence test data";

    // Create event and store it
    {
        EventStore store(temp_dir_.string());
        Event event(id, type, data);
        store.store_event(event);
        EXPECT_TRUE(store.has_seen_event(id));
    }

    // Create new store instance and verify persistence
    {
        EventStore store(temp_dir_.string());
        EXPECT_TRUE(store.has_seen_event(id));
        EXPECT_EQ(store.get_latest_event_id(), id);
    }
}

// Test event store with multiple events
TEST_F(SseTest, EventStoreMultipleEvents) {
    EventStore store(temp_dir_.string());

    std::vector<std::string> event_ids = {
        "multi_001", "multi_002", "multi_003"
    };

    // Store multiple events
    for (size_t i = 0; i < event_ids.size(); ++i) {
        Event event(event_ids[i], "multi_test", "Data " + std::to_string(i));
        store.store_event(event);

        // Each event should be seen
        EXPECT_TRUE(store.has_seen_event(event_ids[i]));
    }

    // All events should be seen
    for (const auto& id : event_ids) {
        EXPECT_TRUE(store.has_seen_event(id));
    }

    // Latest event should be the last one stored
    EXPECT_EQ(store.get_latest_event_id(), event_ids.back());
}

// Test event store duplicate handling
TEST_F(SseTest, EventStoreDuplicateHandling) {
    EventStore store(temp_dir_.string());

    std::string id = "duplicate_test";
    std::string type = "test";
    std::string data1 = "First data";
    std::string data2 = "Second data";

    Event event1(id, type, data1);
    Event event2(id, type, data2); // Same ID, different data

    // Store first event
    store.store_event(event1);
    EXPECT_TRUE(store.has_seen_event(id));

    // Store second event with same ID (should be ignored)
    store.store_event(event2);
    EXPECT_TRUE(store.has_seen_event(id));

    // Verify only one file exists for this ID
    auto event_files = std::filesystem::directory_iterator(temp_dir_);
    int file_count = 0;
    for (const auto& entry : event_files) {
        if (entry.path().filename().string().find(id) != std::string::npos) {
            file_count++;
        }
    }
    EXPECT_EQ(file_count, 1);
}

// Test event compression (if enabled)
#ifdef USE_COMPRESSION
TEST_F(SseTest, EventCompression) {
    std::string test_data = "This is a test string for compression. ";
    // Make it larger to ensure compression is worthwhile
    for (int i = 0; i < 10; ++i) {
        test_data += test_data;
    }

    // Test compression
    std::string compressed = compress_data(test_data);
    EXPECT_LT(compressed.size(), test_data.size());

    // Test decompression
    std::string decompressed = decompress_data(compressed);
    EXPECT_EQ(decompressed, test_data);
}

TEST_F(SseTest, EventWithCompression) {
    std::string id = "compress_test";
    std::string type = "compressed";
    std::string large_data = "Large data for compression test. ";
    for (int i = 0; i < 100; ++i) {
        large_data += large_data;
    }

    Event event(id, type, large_data);
    event.set_compressed(true);

    EXPECT_TRUE(event.is_compressed());
    EXPECT_EQ(event.id(), id);
    EXPECT_EQ(event.event_type(), type);
}
#endif

// Test event metadata operations
TEST_F(SseTest, EventMetadataOperations) {
    std::string id = "meta_test";
    std::string type = "metadata";
    std::string data = "Metadata test";

    Event event(id, type, data);

    // Initially no metadata
    EXPECT_TRUE(event.metadata().empty());

    // Add metadata
    event.add_metadata("key1", "value1");
    event.add_metadata("key2", "value2");

    EXPECT_EQ(event.metadata().size(), 2);
    EXPECT_EQ(event.metadata().at("key1"), "value1");
    EXPECT_EQ(event.metadata().at("key2"), "value2");

    // Test metadata in serialization
    std::string serialized = event.serialize();
    EXPECT_THAT(serialized, HasSubstr("id: " + id));
    EXPECT_THAT(serialized, HasSubstr("event: " + type));
    EXPECT_THAT(serialized, HasSubstr("data: " + data));
}

// Test event timestamp functionality
TEST_F(SseTest, EventTimestamp) {
    auto before = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    Event event("time_test", "timestamp", "Timestamp test");

    auto after = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto event_time = event.timestamp();
    EXPECT_GE(event_time, before);
    EXPECT_LE(event_time, after);
}

// Test event equality and comparison
TEST_F(SseTest, EventEquality) {
    Event event1("test_id", "test_type", "test_data");
    Event event2("test_id", "test_type", "test_data");
    Event event3("different_id", "test_type", "test_data");

    // Events with same ID should be considered equal
    EXPECT_EQ(event1.id(), event2.id());
    EXPECT_NE(event1.id(), event3.id());
}

// Test event store thread safety (basic test)
TEST_F(SseTest, EventStoreThreadSafety) {
    EventStore store(temp_dir_.string());

    // This is a basic test - in a real scenario, you'd want to test
    // concurrent access from multiple threads
    std::string id = "thread_test";
    Event event(id, "thread", "Thread safety test");

    store.store_event(event);
    EXPECT_TRUE(store.has_seen_event(id));

    // Multiple calls should be safe
    EXPECT_TRUE(store.has_seen_event(id));
    EXPECT_TRUE(store.has_seen_event(id));

    std::string latest = store.get_latest_event_id();
    EXPECT_EQ(latest, id);
}

// Test event store with invalid path
TEST_F(SseTest, EventStoreInvalidPath) {
    // Test with a path that cannot be created (e.g., under a file)
    auto invalid_path = temp_dir_ / "file.txt" / "invalid";

    // Create a file first
    std::ofstream file(temp_dir_ / "file.txt");
    file << "test";
    file.close();

    // This should handle the error gracefully
    EXPECT_NO_THROW({
        EventStore store(invalid_path.string());
    });
}

// Test event serialization with special characters
TEST_F(SseTest, EventSerializationSpecialCharacters) {
    std::string id = "special_test";
    std::string type = "special";
    std::string data = "Data with\nspecial\rcharacters\tand\x00null";

    Event event(id, type, data);
    std::string serialized = event.serialize();

    // Should handle special characters properly
    EXPECT_THAT(serialized, HasSubstr("id: " + id));
    EXPECT_THAT(serialized, HasSubstr("event: " + type));
    EXPECT_THAT(serialized, EndsWith("\n\n"));
}

// Test event with empty data
TEST_F(SseTest, EventEmptyData) {
    std::string id = "empty_test";
    std::string type = "empty";
    std::string data = "";

    Event event(id, type, data);

    EXPECT_EQ(event.id(), id);
    EXPECT_EQ(event.event_type(), type);
    EXPECT_EQ(event.data(), data);

    std::string serialized = event.serialize();
    EXPECT_THAT(serialized, HasSubstr("id: " + id));
    EXPECT_THAT(serialized, HasSubstr("event: " + type));
    EXPECT_THAT(serialized, HasSubstr("data: "));
}

} // namespace atom::extra::asio::test

#endif  // Temporarily disabled SSE tests
