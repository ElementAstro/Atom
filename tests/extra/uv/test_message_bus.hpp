#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/extra/uv/message_bus.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace msgbus::test {

// Simple serializable message type for testing
struct TestMessage {
    int id;
    std::string content;

    TestMessage() : id(0), content("") {}
    TestMessage(int i, std::string c) : id(i), content(std::move(c)) {}

    // Equality operator for testing
    bool operator==(const TestMessage& other) const {
        return id == other.id && content == other.content;
    }

    // Serialization methods
    std::string serialize() const { return std::to_string(id) + ":" + content; }

    static TestMessage deserialize(const std::string& data) {
        size_t pos = data.find(':');
        if (pos == std::string::npos) {
            return TestMessage{};
        }
        int id = std::stoi(data.substr(0, pos));
        std::string content = data.substr(pos + 1);
        return TestMessage{id, content};
    }
};

// Test fixture for MessageBus tests
class MessageBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for all tests
    }

    void TearDown() override {
        // Clean up after each test
    }
};

// Test that TestMessage satisfies the Serializable concept
TEST_F(MessageBusTest, TestMessageIsSerializable) {
    EXPECT_TRUE(Serializable<TestMessage>);

    TestMessage msg{42, "test content"};
    std::string serialized = msg.serialize();
    EXPECT_EQ(serialized, "42:test content");

    TestMessage deserialized = TestMessage::deserialize(serialized);
    EXPECT_EQ(deserialized.id, 42);
    EXPECT_EQ(deserialized.content, "test content");
    EXPECT_EQ(msg, deserialized);
}

// Test that TestMessage satisfies the MessageType concept
TEST_F(MessageBusTest, TestMessageIsMessageType) {
    EXPECT_TRUE(MessageType<TestMessage>);

    // Test copyability
    TestMessage original{1, "original"};
    TestMessage copy = original;
    EXPECT_EQ(copy.id, 1);
    EXPECT_EQ(copy.content, "original");

    // Modify copy and ensure original is unchanged
    copy.id = 2;
    copy.content = "modified";
    EXPECT_EQ(original.id, 1);
    EXPECT_EQ(original.content, "original");

    // Test default initialization
    TestMessage defaultMsg;
    EXPECT_EQ(defaultMsg.id, 0);
    EXPECT_EQ(defaultMsg.content, "");
}

// Test MessageEnvelope functionality
TEST_F(MessageBusTest, MessageEnvelopeTest) {
    // Create a message envelope
    TestMessage payload{123, "envelope test"};
    std::string topic = "test/topic";
    std::string sender = "test-sender";

    MessageEnvelope<TestMessage> envelope(topic, payload, sender);

    // Verify envelope properties
    EXPECT_EQ(envelope.topic, topic);
    EXPECT_EQ(envelope.payload.id, payload.id);
    EXPECT_EQ(envelope.payload.content, payload.content);
    EXPECT_EQ(envelope.sender_id, sender);
    EXPECT_GT(envelope.message_id, 0);

    // Verify timestamp is recent (within last second)
    auto now = std::chrono::system_clock::now();
    auto diff = now - envelope.timestamp;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(diff).count(),
              1);

    // Create another envelope and verify message_id is incremented
    MessageEnvelope<TestMessage> envelope2(topic, payload, sender);
    EXPECT_GT(envelope2.message_id, envelope.message_id);
}

// Test MessageFilter functionality
TEST_F(MessageBusTest, MessageFilterTest) {
    TestMessage msg{42, "filter test"};
    MessageEnvelope<TestMessage> envelope("test/topic", msg, "sender");

    // Test filter that matches message id
    MessageFilter<TestMessage> idFilter =
        [](const MessageEnvelope<TestMessage>& e) {
            return e.payload.id == 42;
        };
    EXPECT_TRUE(idFilter(envelope));

    // Test filter that doesn't match
    MessageFilter<TestMessage> nonMatchingFilter =
        [](const MessageEnvelope<TestMessage>& e) {
            return e.payload.id > 100;
        };
    EXPECT_FALSE(nonMatchingFilter(envelope));

    // Test filter based on topic
    MessageFilter<TestMessage> topicFilter =
        [](const MessageEnvelope<TestMessage>& e) {
            return e.topic == "test/topic";
        };
    EXPECT_TRUE(topicFilter(envelope));

    // Test combined filter
    MessageFilter<TestMessage> combinedFilter =
        [](const MessageEnvelope<TestMessage>& e) {
            return e.topic == "test/topic" && e.payload.id == 42;
        };
    EXPECT_TRUE(combinedFilter(envelope));
}

// Test HandlerRegistration functionality
TEST_F(MessageBusTest, HandlerRegistrationTest) {
    bool cleanupCalled = false;

    {
        HandlerRegistration reg(123, "test/topic/+",
                                [&cleanupCalled]() { cleanupCalled = true; });

        EXPECT_EQ(reg.id, 123);
        EXPECT_EQ(reg.topic_pattern, "test/topic/+");
        EXPECT_FALSE(cleanupCalled);
    }

    // Verify cleanup was called when registration went out of scope
    EXPECT_TRUE(cleanupCalled);

    // Test with SubscriptionHandle (unique_ptr wrapper)
    cleanupCalled = false;
    {
        SubscriptionHandle handle = std::make_unique<HandlerRegistration>(
            456, "another/topic/#",
            [&cleanupCalled]() { cleanupCalled = true; });

        EXPECT_EQ(handle->id, 456);
        EXPECT_EQ(handle->topic_pattern, "another/topic/#");
        EXPECT_FALSE(cleanupCalled);
    }

    // Verify cleanup was called when handle was destroyed
    EXPECT_TRUE(cleanupCalled);
}

// Test BackPressureConfig functionality
TEST_F(MessageBusTest, BackPressureConfigTest) {
    // Test default values
    BackPressureConfig defaultConfig;
    EXPECT_EQ(defaultConfig.max_queue_size, 10000);
    EXPECT_EQ(defaultConfig.timeout.count(), 1000);
    EXPECT_TRUE(defaultConfig.drop_oldest);

    // Test custom configuration
    BackPressureConfig customConfig;
    customConfig.max_queue_size = 500;
    customConfig.timeout = std::chrono::milliseconds(2000);
    customConfig.drop_oldest = false;

    EXPECT_EQ(customConfig.max_queue_size, 500);
    EXPECT_EQ(customConfig.timeout.count(), 2000);
    EXPECT_FALSE(customConfig.drop_oldest);
}

// Test handler concepts
TEST_F(MessageBusTest, HandlerConceptsTest) {
    // Test synchronous handler
    auto syncHandler = [](TestMessage msg) { return; };
    EXPECT_TRUE((MessageHandler<decltype(syncHandler), TestMessage>));

    // Test asynchronous handler
    auto asyncHandler = [](TestMessage msg) -> std::future<void> {
        std::promise<void> promise;
        auto future = promise.get_future();
        promise.set_value();
        return future;
    };
    EXPECT_TRUE((MessageHandler<decltype(asyncHandler), TestMessage>));
    EXPECT_TRUE((AsyncMessageHandler<decltype(asyncHandler), TestMessage>));

    // Non-handler function (wrong parameter type)
    auto wrongHandler = [](std::string msg) { return; };
    EXPECT_FALSE((MessageHandler<decltype(wrongHandler), TestMessage>));

    // Non-async handler (returns void instead of future)
    auto nonAsyncHandler = [](TestMessage msg) { return; };
    EXPECT_FALSE((AsyncMessageHandler<decltype(nonAsyncHandler), TestMessage>));
}

// Test Result (expected) type with success
TEST_F(MessageBusTest, ResultSuccessTest) {
    Result<int> result = 42;

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_FALSE(result.has_value());
}

// Test Result (expected) type with error
TEST_F(MessageBusTest, ResultErrorTest) {
    Result<int> result = std::unexpected(MessageBusError::QueueFull);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MessageBusError::QueueFull);
}

// Since we can't fully test the coroutine support without the full
// implementation, we'll test what we can of the MessageAwaiter structure
TEST_F(MessageBusTest, MessageAwaiterBasicsTest) {
    MessageAwaiter<TestMessage> awaiter;
    awaiter.topic = "test/topic";
    awaiter.timeout = std::chrono::milliseconds(500);

    // Test await_ready always returns false (meaning it will suspend)
    EXPECT_FALSE(awaiter.await_ready());

    // We can't fully test await_suspend and await_resume without the
    // implementation, but we can test that they exist and have the correct
    // signatures
    static_assert(
        std::is_same_v<decltype(std::declval<MessageAwaiter<TestMessage>>()
                                    .await_resume()),
                       Result<MessageEnvelope<TestMessage>>>,
        "await_resume() should return Result<MessageEnvelope<T>>");
}

// Integration-style test that simulates different message patterns
TEST_F(MessageBusTest, MessageFlowSimulationTest) {
    // This test simulates a basic message flow without actually using the
    // message bus implementation (which we don't have access to in this test)

    // Create a few messages and envelopes
    TestMessage msg1{1, "first message"};
    TestMessage msg2{2, "second message"};
    TestMessage msg3{3, "third message"};

    MessageEnvelope<TestMessage> env1("topic/1", msg1, "sender-A");
    MessageEnvelope<TestMessage> env2("topic/2", msg2, "sender-B");
    MessageEnvelope<TestMessage> env3("topic/1", msg3, "sender-A");

    // Create filters
    auto topic1Filter = [](const MessageEnvelope<TestMessage>& e) {
        return e.topic == "topic/1";
    };

    auto senderAFilter = [](const MessageEnvelope<TestMessage>& e) {
        return e.sender_id == "sender-A";
    };

    // Apply filters
    std::vector<MessageEnvelope<TestMessage>> messages{env1, env2, env3};
    std::vector<MessageEnvelope<TestMessage>> topic1Messages;
    std::vector<MessageEnvelope<TestMessage>> senderAMessages;

    for (const auto& msg : messages) {
        if (topic1Filter(msg)) {
            topic1Messages.push_back(msg);
        }
        if (senderAFilter(msg)) {
            senderAMessages.push_back(msg);
        }
    }

    // Check filter results
    EXPECT_EQ(topic1Messages.size(), 2);
    EXPECT_EQ(senderAMessages.size(), 2);

    // Check specific envelope contents
    if (!topic1Messages.empty()) {
        EXPECT_EQ(topic1Messages[0].payload.id, 1);
        EXPECT_EQ(topic1Messages[1].payload.id, 3);
    }

    if (!senderAMessages.empty()) {
        EXPECT_EQ(senderAMessages[0].topic, "topic/1");
        EXPECT_EQ(senderAMessages[1].topic, "topic/1");
    }
}

// Test that verifies metadata functionality
TEST_F(MessageBusTest, MessageEnvelopeMetadataTest) {
    TestMessage msg{42, "metadata test"};
    MessageEnvelope<TestMessage> envelope("test/topic", msg);

    // Metadata should start empty
    EXPECT_TRUE(envelope.metadata.empty());

    // Add metadata
    envelope.metadata["priority"] = "high";
    envelope.metadata["retention"] = "24h";
    envelope.metadata["source"] = "unit-test";

    // Verify metadata
    EXPECT_EQ(envelope.metadata.size(), 3);
    EXPECT_EQ(envelope.metadata["priority"], "high");
    EXPECT_EQ(envelope.metadata["retention"], "24h");
    EXPECT_EQ(envelope.metadata["source"], "unit-test");

    // Update metadata
    envelope.metadata["priority"] = "critical";
    EXPECT_EQ(envelope.metadata["priority"], "critical");

    // Remove metadata
    envelope.metadata.erase("source");
    EXPECT_EQ(envelope.metadata.size(), 2);
    EXPECT_EQ(envelope.metadata.find("source"), envelope.metadata.end());
}

}  // namespace msgbus::test
