#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/rtype.hpp"

using namespace atom::type;

// Simple test classes
struct SimpleType {
    int id = 0;
    std::string name;
    double value = 0.0;
    bool active = false;
    std::vector<std::string> tags;
    std::vector<int> numbers;

    bool operator==(const SimpleType& other) const {
        return id == other.id && name == other.name && value == other.value &&
               active == other.active && tags == other.tags &&
               numbers == other.numbers;
    }
};

struct NestedType {
    int id = 0;
    std::string description;
    SimpleType inner;

    bool operator==(const NestedType& other) const {
        return id == other.id && description == other.description &&
               inner == other.inner;
    }
};

struct TypeWithValidation {
    int age = 0;
    std::string email;
};

class RTypeTest : public ::testing::Test {
protected:
    Reflectable<SimpleType> simpleTypeReflection = Reflectable<SimpleType>(
        make_field<SimpleType>("id", "The unique identifier", &SimpleType::id),
        make_field<SimpleType>("name", "The display name", &SimpleType::name),
        make_field<SimpleType>("value", "A numeric value", &SimpleType::value),
        make_field<SimpleType>("active", "Whether the item is active", &SimpleType::active),
        make_field<SimpleType>("tags", "Associated tags", &SimpleType::tags),
        make_field<SimpleType>("numbers", "Associated numbers", &SimpleType::numbers)
    );

    Reflectable<TypeWithValidation> validationTypeReflection = Reflectable<TypeWithValidation>(
        make_field<TypeWithValidation>(
            "age", "User age", &TypeWithValidation::age, true, 0,
            [](const int& age) { return age >= 0 && age <= 120; }),
        make_field<TypeWithValidation>(
            "email", "User email", &TypeWithValidation::email, true, "",
            [](const std::string& email) {
                return email.find('@') != std::string::npos &&
                        email.find('.') != std::string::npos;
            })
    );

    Reflectable<NestedType> nestedTypeReflection = Reflectable<NestedType>(
        make_field<NestedType>("id", "The nested type ID", &NestedType::id),
        make_field<NestedType>("description", "A description", &NestedType::description),
        make_field<NestedType>("inner", "The inner simple type", &NestedType::inner, simpleTypeReflection)
    );

    void SetUp() override {
        // No need to reinitialize in SetUp, already initialized in declaration
    }

    // Helper methods to create sample JSON and YAML objects for testing
    JsonObject createSimpleTypeJson() {
        JsonObject json;
        json["id"] = JsonValue(static_cast<double>(42));
        json["name"] = JsonValue(std::string("Test Item"));
        json["value"] = JsonValue(3.14);
        json["active"] = JsonValue(true);

        JsonArray tagsArray;
        tagsArray.push_back(JsonValue(std::string("tag1")));
        tagsArray.push_back(JsonValue(std::string("tag2")));
        json["tags"] = JsonValue(tagsArray);

        JsonArray numbersArray;
        numbersArray.push_back(JsonValue(static_cast<double>(1)));
        numbersArray.push_back(JsonValue(static_cast<double>(2)));
        numbersArray.push_back(JsonValue(static_cast<double>(3)));
        json["numbers"] = JsonValue(numbersArray);

        return json;
    }

    YamlObject createSimpleTypeYaml() {
        YamlObject yaml;
        yaml["id"] = YamlValue(static_cast<double>(42));
        yaml["name"] = YamlValue(std::string("Test Item"));
        yaml["value"] = YamlValue(3.14);
        yaml["active"] = YamlValue(true);

        YamlArray tagsArray;
        tagsArray.push_back(YamlValue(std::string("tag1")));
        tagsArray.push_back(YamlValue(std::string("tag2")));
        yaml["tags"] = YamlValue(tagsArray);

        YamlArray numbersArray;
        numbersArray.push_back(YamlValue(static_cast<double>(1)));
        numbersArray.push_back(YamlValue(static_cast<double>(2)));
        numbersArray.push_back(YamlValue(static_cast<double>(3)));
        yaml["numbers"] = YamlValue(numbersArray);

        return yaml;
    }

    JsonObject createNestedTypeJson() {
        JsonObject json;
        json["id"] = JsonValue(static_cast<double>(100));
        json["description"] = JsonValue(std::string("A nested type"));
        json["inner"] = JsonValue(createSimpleTypeJson());
        return json;
    }

    YamlObject createNestedTypeYaml() {
        YamlObject yaml;
        yaml["id"] = YamlValue(static_cast<double>(100));
        yaml["description"] = YamlValue(std::string("A nested type"));
        yaml["inner"] = YamlValue(createSimpleTypeYaml());
        return yaml;
    }
};

// Basic Functionality Tests
TEST_F(RTypeTest, SimpleTypeFromJson) {
    JsonObject json = createSimpleTypeJson();

    SimpleType obj = simpleTypeReflection.from_json(json);

    EXPECT_EQ(obj.id, 42);
    EXPECT_EQ(obj.name, "Test Item");
    EXPECT_DOUBLE_EQ(obj.value, 3.14);
    EXPECT_TRUE(obj.active);
    ASSERT_EQ(obj.tags.size(), 2);
    EXPECT_EQ(obj.tags[0], "tag1");
    EXPECT_EQ(obj.tags[1], "tag2");
    ASSERT_EQ(obj.numbers.size(), 3);
    EXPECT_EQ(obj.numbers[0], 1);
    EXPECT_EQ(obj.numbers[1], 2);
    EXPECT_EQ(obj.numbers[2], 3);
}

TEST_F(RTypeTest, SimpleTypeToJson) {
    SimpleType obj;
    obj.id = 42;
    obj.name = "Test Item";
    obj.value = 3.14;
    obj.active = true;
    obj.tags = {"tag1", "tag2"};
    obj.numbers = {1, 2, 3};

    JsonObject json = simpleTypeReflection.to_json(obj);

    EXPECT_EQ(json["id"].asNumber(), 42);
    EXPECT_EQ(json["name"].asString(), "Test Item");
    EXPECT_DOUBLE_EQ(json["value"].asNumber(), 3.14);
    EXPECT_TRUE(json["active"].asBool());

    auto tags = json["tags"].asArray();
    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0].asString(), "tag1");
    EXPECT_EQ(tags[1].asString(), "tag2");

    auto numbers = json["numbers"].asArray();
    ASSERT_EQ(numbers.size(), 3);
    EXPECT_EQ(numbers[0].asNumber(), 1);
    EXPECT_EQ(numbers[1].asNumber(), 2);
    EXPECT_EQ(numbers[2].asNumber(), 3);
}

TEST_F(RTypeTest, SimpleTypeFromYaml) {
    YamlObject yaml = createSimpleTypeYaml();

    SimpleType obj = simpleTypeReflection.from_yaml(yaml);

    EXPECT_EQ(obj.id, 42);
    EXPECT_EQ(obj.name, "Test Item");
    EXPECT_DOUBLE_EQ(obj.value, 3.14);
    EXPECT_TRUE(obj.active);
    ASSERT_EQ(obj.tags.size(), 2);
    EXPECT_EQ(obj.tags[0], "tag1");
    EXPECT_EQ(obj.tags[1], "tag2");
    ASSERT_EQ(obj.numbers.size(), 3);
    EXPECT_EQ(obj.numbers[0], 1);
    EXPECT_EQ(obj.numbers[1], 2);
    EXPECT_EQ(obj.numbers[2], 3);
}

TEST_F(RTypeTest, SimpleTypeToYaml) {
    SimpleType obj;
    obj.id = 42;
    obj.name = "Test Item";
    obj.value = 3.14;
    obj.active = true;
    obj.tags = {"tag1", "tag2"};
    obj.numbers = {1, 2, 3};

    YamlObject yaml = simpleTypeReflection.to_yaml(obj);

    EXPECT_EQ(yaml["id"].asNumber(), 42);
    EXPECT_EQ(yaml["name"].asString(), "Test Item");
    EXPECT_DOUBLE_EQ(yaml["value"].asNumber(), 3.14);
    EXPECT_TRUE(yaml["active"].asBool());

    auto tags = yaml["tags"].asArray();
    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0].asString(), "tag1");
    EXPECT_EQ(tags[1].asString(), "tag2");

    auto numbers = yaml["numbers"].asArray();
    ASSERT_EQ(numbers.size(), 3);
    EXPECT_EQ(numbers[0].asNumber(), 1);
    EXPECT_EQ(numbers[1].asNumber(), 2);
    EXPECT_EQ(numbers[2].asNumber(), 3);
}

// Nested Type Tests
TEST_F(RTypeTest, NestedTypeFromJson) {
    JsonObject json = createNestedTypeJson();

    NestedType obj = nestedTypeReflection.from_json(json);

    EXPECT_EQ(obj.id, 100);
    EXPECT_EQ(obj.description, "A nested type");

    // Verify nested object
    EXPECT_EQ(obj.inner.id, 42);
    EXPECT_EQ(obj.inner.name, "Test Item");
    EXPECT_DOUBLE_EQ(obj.inner.value, 3.14);
    EXPECT_TRUE(obj.inner.active);
    ASSERT_EQ(obj.inner.tags.size(), 2);
    EXPECT_EQ(obj.inner.tags[0], "tag1");
    EXPECT_EQ(obj.inner.tags[1], "tag2");
    ASSERT_EQ(obj.inner.numbers.size(), 3);
    EXPECT_EQ(obj.inner.numbers[0], 1);
    EXPECT_EQ(obj.inner.numbers[1], 2);
    EXPECT_EQ(obj.inner.numbers[2], 3);
}

TEST_F(RTypeTest, NestedTypeToJson) {
    NestedType obj;
    obj.id = 100;
    obj.description = "A nested type";

    obj.inner.id = 42;
    obj.inner.name = "Test Item";
    obj.inner.value = 3.14;
    obj.inner.active = true;
    obj.inner.tags = {"tag1", "tag2"};
    obj.inner.numbers = {1, 2, 3};

    JsonObject json = nestedTypeReflection.to_json(obj);

    EXPECT_EQ(json["id"].asNumber(), 100);
    EXPECT_EQ(json["description"].asString(), "A nested type");

    // Verify nested object
    JsonObject innerJson = json["inner"].asObject();
    EXPECT_EQ(innerJson["id"].asNumber(), 42);
    EXPECT_EQ(innerJson["name"].asString(), "Test Item");
    EXPECT_DOUBLE_EQ(innerJson["value"].asNumber(), 3.14);
    EXPECT_TRUE(innerJson["active"].asBool());

    auto tags = innerJson["tags"].asArray();
    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0].asString(), "tag1");
    EXPECT_EQ(tags[1].asString(), "tag2");

    auto numbers = innerJson["numbers"].asArray();
    ASSERT_EQ(numbers.size(), 3);
    EXPECT_EQ(numbers[0].asNumber(), 1);
    EXPECT_EQ(numbers[1].asNumber(), 2);
    EXPECT_EQ(numbers[2].asNumber(), 3);
}

TEST_F(RTypeTest, NestedTypeFromYaml) {
    YamlObject yaml = createNestedTypeYaml();

    NestedType obj = nestedTypeReflection.from_yaml(yaml);

    EXPECT_EQ(obj.id, 100);
    EXPECT_EQ(obj.description, "A nested type");

    // Verify nested object
    EXPECT_EQ(obj.inner.id, 42);
    EXPECT_EQ(obj.inner.name, "Test Item");
    EXPECT_DOUBLE_EQ(obj.inner.value, 3.14);
    EXPECT_TRUE(obj.inner.active);
    ASSERT_EQ(obj.inner.tags.size(), 2);
    EXPECT_EQ(obj.inner.tags[0], "tag1");
    EXPECT_EQ(obj.inner.tags[1], "tag2");
    ASSERT_EQ(obj.inner.numbers.size(), 3);
    EXPECT_EQ(obj.inner.numbers[0], 1);
    EXPECT_EQ(obj.inner.numbers[1], 2);
    EXPECT_EQ(obj.inner.numbers[2], 3);
}

TEST_F(RTypeTest, NestedTypeToYaml) {
    NestedType obj;
    obj.id = 100;
    obj.description = "A nested type";

    obj.inner.id = 42;
    obj.inner.name = "Test Item";
    obj.inner.value = 3.14;
    obj.inner.active = true;
    obj.inner.tags = {"tag1", "tag2"};
    obj.inner.numbers = {1, 2, 3};

    YamlObject yaml = nestedTypeReflection.to_yaml(obj);

    EXPECT_EQ(yaml["id"].asNumber(), 100);
    EXPECT_EQ(yaml["description"].asString(), "A nested type");

    // Verify nested object
    YamlObject innerYaml = yaml["inner"].asObject();
    EXPECT_EQ(innerYaml["id"].asNumber(), 42);
    EXPECT_EQ(innerYaml["name"].asString(), "Test Item");
    EXPECT_DOUBLE_EQ(innerYaml["value"].asNumber(), 3.14);
    EXPECT_TRUE(innerYaml["active"].asBool());

    auto tags = innerYaml["tags"].asArray();
    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0].asString(), "tag1");
    EXPECT_EQ(tags[1].asString(), "tag2");

    auto numbers = innerYaml["numbers"].asArray();
    ASSERT_EQ(numbers.size(), 3);
    EXPECT_EQ(numbers[0].asNumber(), 1);
    EXPECT_EQ(numbers[1].asNumber(), 2);
    EXPECT_EQ(numbers[2].asNumber(), 3);
}

// Required Field and Default Value Tests
TEST_F(RTypeTest, RequiredFieldsMissing) {
    JsonObject json;
    json["value"] = JsonValue(3.14);

    // Missing required "id" and "name" fields
    EXPECT_THROW(simpleTypeReflection.from_json(json), std::invalid_argument);
}

TEST_F(RTypeTest, OptionalFieldsWithDefaultValues) {
    auto optionalReflection = Reflectable<SimpleType>(
        make_field<SimpleType>("id", "The ID", &SimpleType::id, true),
        make_field<SimpleType>("name", "The name", &SimpleType::name, true),
        make_field<SimpleType>("value", "The value", &SimpleType::value, false, 99.9),
        make_field<SimpleType>("active", "Is active", &SimpleType::active, false, true)
    );

    JsonObject json;
    json["id"] = JsonValue(static_cast<double>(42));
    json["name"] = JsonValue(std::string("Test Item"));

    SimpleType obj = optionalReflection.from_json(json);

    EXPECT_EQ(obj.id, 42);
    EXPECT_EQ(obj.name, "Test Item");
    EXPECT_DOUBLE_EQ(obj.value, 99.9);
    EXPECT_TRUE(obj.active);
}

// Validation Tests
TEST_F(RTypeTest, ValidationPasses) {
    JsonObject json;
    json["age"] = JsonValue(30);
    json["email"] = JsonValue("test@example.com");

    TypeWithValidation obj = validationTypeReflection.from_json(json);

    EXPECT_EQ(obj.age, 30);
    EXPECT_EQ(obj.email, "test@example.com");
}

TEST_F(RTypeTest, ValidationFailsAge) {
    JsonObject json;
    json["age"] = JsonValue(150);
    json["email"] = JsonValue("test@example.com");

    EXPECT_THROW(validationTypeReflection.from_json(json),
                 std::invalid_argument);
}

TEST_F(RTypeTest, ValidationFailsEmail) {
    JsonObject json;
    json["age"] = JsonValue(30);
    json["email"] = JsonValue("invalid-email");

    EXPECT_THROW(validationTypeReflection.from_json(json),
                 std::invalid_argument);
}

// Map Container Tests
struct TypeWithMap {
    std::unordered_map<std::string, int> counts;
    std::unordered_map<std::string, std::string> mappings;
};

TEST_F(RTypeTest, MapContainerYaml) {
    auto mapReflection = Reflectable<TypeWithMap>(
        make_field<TypeWithMap>("counts", "Count values", &TypeWithMap::counts),
        make_field<TypeWithMap>("mappings", "String mappings",
                                &TypeWithMap::mappings));

    YamlObject yaml;

    YamlObject countsObj;
    countsObj["one"] = YamlValue(1);
    countsObj["two"] = YamlValue(2);
    countsObj["three"] = YamlValue(3);
    yaml["counts"] = YamlValue(countsObj);

    YamlObject mappingsObj;
    mappingsObj["key1"] = YamlValue("value1");
    mappingsObj["key2"] = YamlValue("value2");
    yaml["mappings"] = YamlValue(mappingsObj);

    TypeWithMap obj = mapReflection.from_yaml(yaml);

    ASSERT_EQ(obj.counts.size(), 3);
    EXPECT_EQ(obj.counts["one"], 1);
    EXPECT_EQ(obj.counts["two"], 2);
    EXPECT_EQ(obj.counts["three"], 3);

    ASSERT_EQ(obj.mappings.size(), 2);
    EXPECT_EQ(obj.mappings["key1"], "value1");
    EXPECT_EQ(obj.mappings["key2"], "value2");
}

// Edge Cases
TEST_F(RTypeTest, EmptyContainers) {
    JsonObject json;
    json["id"] = JsonValue(42);
    json["name"] = JsonValue("Test Item");
    json["value"] = JsonValue(3.14);
    json["active"] = JsonValue(true);
    json["tags"] = JsonValue(JsonArray{});
    json["numbers"] = JsonValue(JsonArray{});

    SimpleType obj = simpleTypeReflection.from_json(json);

    EXPECT_EQ(obj.id, 42);
    EXPECT_TRUE(obj.tags.empty());
    EXPECT_TRUE(obj.numbers.empty());
}

TEST_F(RTypeTest, UnsupportedType) {
    struct UnsupportedType {
        void* pointer;
    };

    auto unsupportedReflection =
        Reflectable<UnsupportedType>(make_field<UnsupportedType>(
            "pointer", "A pointer", &UnsupportedType::pointer));

    UnsupportedType obj;

    EXPECT_THROW(unsupportedReflection.to_json(obj), std::invalid_argument);

    JsonObject json;
    json["pointer"] = JsonValue("not convertible to pointer");

    EXPECT_THROW(unsupportedReflection.from_json(json), std::invalid_argument);
}

// Roundtrip Test (serialize then deserialize)
TEST_F(RTypeTest, RoundtripJsonSerialization) {
    NestedType originalObj;
    originalObj.id = 100;
    originalObj.description = "A nested type";
    originalObj.inner.id = 42;
    originalObj.inner.name = "Test Item";
    originalObj.inner.value = 3.14;
    originalObj.inner.active = true;
    originalObj.inner.tags = {"tag1", "tag2"};
    originalObj.inner.numbers = {1, 2, 3};

    JsonObject json = nestedTypeReflection.to_json(originalObj);

    NestedType deserializedObj = nestedTypeReflection.from_json(json);

    EXPECT_EQ(deserializedObj, originalObj);
}

TEST_F(RTypeTest, RoundtripYamlSerialization) {
    NestedType originalObj;
    originalObj.id = 100;
    originalObj.description = "A nested type";
    originalObj.inner.id = 42;
    originalObj.inner.name = "Test Item";
    originalObj.inner.value = 3.14;
    originalObj.inner.active = true;
    originalObj.inner.tags = {"tag1", "tag2"};
    originalObj.inner.numbers = {1, 2, 3};

    YamlObject yaml = nestedTypeReflection.to_yaml(originalObj);

    NestedType deserializedObj = nestedTypeReflection.from_yaml(yaml);

    EXPECT_EQ(deserializedObj, originalObj);
}