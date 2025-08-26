#include "atom/components/serialization.hpp"
#include "atom/components/component.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace atom::components;

// Test component for serialization testing
class TestSerializationComponent : public Component {
public:
    TestSerializationComponent(const std::string& name) : Component(name) {
        // Add some test variables
        addVariable<int>("intValue", 42, "Test integer");
        addVariable<std::string>("stringValue", "Hello World", "Test string");
        addVariable<double>("doubleValue", 3.14159, "Test double");
        addVariable<bool>("boolValue", true, "Test boolean");
    }
};

// Test fixture for Serialization tests
class SerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        component_ = std::make_shared<TestSerializationComponent>("SerializationTestComponent");
        
        // Set up default options
        options_.format = SerializationFormat::JSON;
        options_.includeMetadata = true;
        options_.includeVariables = true;
        options_.includeCommands = false;
        options_.prettyPrint = false;
        options_.enableCompression = false;
        options_.enableEncryption = false;
        options_.version = 1;
    }

    std::shared_ptr<TestSerializationComponent> component_;
    SerializationOptions options_;
};

// Test fixture for JsonSerializer tests
class JsonSerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer_ = std::make_unique<JsonSerializer>();
        component_ = std::make_shared<TestSerializationComponent>("JsonTestComponent");
        
        options_.format = SerializationFormat::JSON;
        options_.includeMetadata = true;
        options_.includeVariables = true;
        options_.prettyPrint = true;
    }

    std::unique_ptr<JsonSerializer> serializer_;
    std::shared_ptr<TestSerializationComponent> component_;
    SerializationOptions options_;
};

// Test fixture for BinarySerializer tests
class BinarySerializerTest : public ::testing::Test {
protected:
    void SetUp() override {
        serializer_ = std::make_unique<BinarySerializer>();
        component_ = std::make_shared<TestSerializationComponent>("BinaryTestComponent");
        
        options_.format = SerializationFormat::Binary;
        options_.includeMetadata = true;
        options_.includeVariables = true;
        options_.enableCompression = false;
    }

    std::unique_ptr<BinarySerializer> serializer_;
    std::shared_ptr<TestSerializationComponent> component_;
    SerializationOptions options_;
};

// Test fixture for SerializationManager tests
class SerializationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = &SerializationManager::instance();
        component_ = std::make_shared<TestSerializationComponent>("ManagerTestComponent");
    }

    SerializationManager* manager_;
    std::shared_ptr<TestSerializationComponent> component_;
};

// ============================================================================
// SerializationFormat Tests
// ============================================================================

TEST(SerializationFormatTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(SerializationFormat::JSON), 0);
    EXPECT_EQ(static_cast<uint8_t>(SerializationFormat::Binary), 1);
    EXPECT_EQ(static_cast<uint8_t>(SerializationFormat::XML), 2);
    EXPECT_EQ(static_cast<uint8_t>(SerializationFormat::MessagePack), 3);
    EXPECT_EQ(static_cast<uint8_t>(SerializationFormat::Custom), 4);
}

// ============================================================================
// SerializationResult Tests
// ============================================================================

TEST(SerializationResultTest, DefaultConstruction) {
    SerializationResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.data.empty());
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_EQ(result.originalSize, 0);
    EXPECT_EQ(result.compressedSize, 0);
}

TEST(SerializationResultTest, SuccessfulResult) {
    SerializationResult result;
    result.success = true;
    result.data = {0x01, 0x02, 0x03, 0x04};
    result.originalSize = 100;
    result.compressedSize = 80;
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.data.size(), 4);
    EXPECT_EQ(result.originalSize, 100);
    EXPECT_EQ(result.compressedSize, 80);
}

// ============================================================================
// DeserializationResult Tests
// ============================================================================

TEST(DeserializationResultTest, DefaultConstruction) {
    DeserializationResult result;
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.component, nullptr);
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_EQ(result.version, 0);
}

TEST(DeserializationResultTest, SuccessfulResult) {
    DeserializationResult result;
    result.success = true;
    result.component = std::make_shared<Component>("TestComponent");
    result.version = 1;
    
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.component, nullptr);
    EXPECT_EQ(result.version, 1);
}

// ============================================================================
// JsonSerializer Tests
// ============================================================================

TEST_F(JsonSerializerTest, SupportsFormat) {
    EXPECT_TRUE(serializer_->supportsFormat(SerializationFormat::JSON));
    EXPECT_FALSE(serializer_->supportsFormat(SerializationFormat::Binary));
    EXPECT_FALSE(serializer_->supportsFormat(SerializationFormat::XML));
}

TEST_F(JsonSerializerTest, GetFormatName) {
    EXPECT_EQ(serializer_->getFormatName(), "JSON");
}

TEST_F(JsonSerializerTest, SerializeComponent) {
    auto result = serializer_->serialize(*component_, options_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_GT(result.originalSize, 0);
    
    // Verify it's valid JSON by checking for basic JSON structure
    std::string jsonStr(result.data.begin(), result.data.end());
    EXPECT_NE(jsonStr.find("{"), std::string::npos);
    EXPECT_NE(jsonStr.find("}"), std::string::npos);
}

TEST_F(JsonSerializerTest, SerializeWithPrettyPrint) {
    options_.prettyPrint = true;
    auto result = serializer_->serialize(*component_, options_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
    
    std::string jsonStr(result.data.begin(), result.data.end());
    // Pretty printed JSON should contain newlines and indentation
    EXPECT_NE(jsonStr.find("\n"), std::string::npos);
}

TEST_F(JsonSerializerTest, SerializeWithoutVariables) {
    options_.includeVariables = false;
    auto result = serializer_->serialize(*component_, options_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
}

TEST_F(JsonSerializerTest, SerializeWithoutMetadata) {
    options_.includeMetadata = false;
    auto result = serializer_->serialize(*component_, options_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
}

TEST_F(JsonSerializerTest, DeserializeComponent) {
    // First serialize a component
    auto serializeResult = serializer_->serialize(*component_, options_);
    ASSERT_TRUE(serializeResult.success);
    
    // Then deserialize it
    auto deserializeResult = serializer_->deserialize(serializeResult.data, options_);
    
    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);
    EXPECT_TRUE(deserializeResult.errorMessage.empty());
    
    if (deserializeResult.component) {
        EXPECT_EQ(deserializeResult.component->getName(), "JsonTestComponent");
    }
}

TEST_F(JsonSerializerTest, DeserializeInvalidData) {
    std::vector<uint8_t> invalidData = {'i', 'n', 'v', 'a', 'l', 'i', 'd'};
    
    auto result = serializer_->deserialize(invalidData, options_);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.component, nullptr);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(JsonSerializerTest, RoundTripSerialization) {
    // Serialize
    auto serializeResult = serializer_->serialize(*component_, options_);
    ASSERT_TRUE(serializeResult.success);
    
    // Deserialize
    auto deserializeResult = serializer_->deserialize(serializeResult.data, options_);
    ASSERT_TRUE(deserializeResult.success);
    ASSERT_NE(deserializeResult.component, nullptr);
    
    // Verify component properties are preserved
    EXPECT_EQ(deserializeResult.component->getName(), component_->getName());
    
    // Verify variables are preserved (if included)
    if (options_.includeVariables) {
        EXPECT_TRUE(deserializeResult.component->hasVariable("intValue"));
        EXPECT_TRUE(deserializeResult.component->hasVariable("stringValue"));
    }
}

// ============================================================================
// BinarySerializer Tests
// ============================================================================

TEST_F(BinarySerializerTest, SupportsFormat) {
    EXPECT_TRUE(serializer_->supportsFormat(SerializationFormat::Binary));
    EXPECT_FALSE(serializer_->supportsFormat(SerializationFormat::JSON));
    EXPECT_FALSE(serializer_->supportsFormat(SerializationFormat::XML));
}

TEST_F(BinarySerializerTest, GetFormatName) {
    EXPECT_EQ(serializer_->getFormatName(), "Binary");
}

TEST_F(BinarySerializerTest, SerializeComponent) {
    auto result = serializer_->serialize(*component_, options_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
    EXPECT_TRUE(result.errorMessage.empty());
    EXPECT_GT(result.originalSize, 0);
    
    // Verify binary header magic number
    if (result.data.size() >= 4) {
        uint32_t magic = *reinterpret_cast<const uint32_t*>(result.data.data());
        EXPECT_EQ(magic, 0x41544F4D); // "ATOM"
    }
}

TEST_F(BinarySerializerTest, SerializeWithCompression) {
    options_.enableCompression = true;
    auto result = serializer_->serialize(*component_, options_);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
    
    // With compression, compressed size should be reported
    if (result.success && options_.enableCompression) {
        EXPECT_GT(result.compressedSize, 0);
        EXPECT_LE(result.compressedSize, result.originalSize);
    }
}

TEST_F(BinarySerializerTest, DeserializeComponent) {
    // First serialize a component
    auto serializeResult = serializer_->serialize(*component_, options_);
    ASSERT_TRUE(serializeResult.success);
    
    // Then deserialize it
    auto deserializeResult = serializer_->deserialize(serializeResult.data, options_);
    
    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);
    EXPECT_TRUE(deserializeResult.errorMessage.empty());
}

TEST_F(BinarySerializerTest, DeserializeInvalidData) {
    std::vector<uint8_t> invalidData = {0x00, 0x01, 0x02, 0x03}; // Invalid magic
    
    auto result = serializer_->deserialize(invalidData, options_);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.component, nullptr);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// SerializationManager Tests
// ============================================================================

TEST_F(SerializationManagerTest, Singleton) {
    auto& manager1 = SerializationManager::instance();
    auto& manager2 = SerializationManager::instance();
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(SerializationManagerTest, RegisterSerializer) {
    auto customSerializer = std::make_unique<JsonSerializer>();
    manager_->registerSerializer(SerializationFormat::Custom, std::move(customSerializer));
    
    EXPECT_TRUE(manager_->hasSerializer(SerializationFormat::Custom));
}

TEST_F(SerializationManagerTest, SerializeWithManager) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    
    auto result = manager_->serialize(*component_, options);
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.data.empty());
}

TEST_F(SerializationManagerTest, DeserializeWithManager) {
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    
    // First serialize
    auto serializeResult = manager_->serialize(*component_, options);
    ASSERT_TRUE(serializeResult.success);
    
    // Then deserialize
    auto deserializeResult = manager_->deserialize(serializeResult.data, options);
    
    EXPECT_TRUE(deserializeResult.success);
    EXPECT_NE(deserializeResult.component, nullptr);
}

TEST_F(SerializationManagerTest, UnsupportedFormat) {
    SerializationOptions options;
    options.format = static_cast<SerializationFormat>(99); // Invalid format
    
    auto result = manager_->serialize(*component_, options);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(SerializationErrorTest, EmptyComponent) {
    JsonSerializer serializer;
    Component emptyComponent("");
    SerializationOptions options;
    options.format = SerializationFormat::JSON;
    
    auto result = serializer.serialize(emptyComponent, options);
    
    // Should handle empty component gracefully
    EXPECT_TRUE(result.success || !result.errorMessage.empty());
}

TEST(SerializationErrorTest, NullPointerHandling) {
    // Test that the system handles null pointers gracefully
    DeserializationResult result;
    result.component = nullptr;
    
    EXPECT_EQ(result.component, nullptr);
    EXPECT_FALSE(result.success);
}
