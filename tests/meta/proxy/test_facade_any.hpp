#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "atom/meta/facade_any.hpp"

using namespace atom::meta;
using ::testing::HasSubstr;

// Define custom type to test with EnhancedBoxedValue
class TestPerson {
public:
    TestPerson(std::string name, int age) : name_(std::move(name)), age_(age) {}

    // Copy constructor
    TestPerson(const TestPerson&) = default;

    // Copy assignment
    TestPerson& operator=(const TestPerson&) = default;

    // toString method for stringable_dispatch
    std::string toString() const {
        return name_ + " (" + std::to_string(age_) + ")";
    }

    // serialize method for serializable_dispatch
    std::string serialize() const {
        return "{\"name\":\"" + name_ + "\",\"age\":" + std::to_string(age_) +
               "}";
    }

    // deserialize method for serializable_dispatch
    bool deserialize(const std::string& json) {
        // Simple parsing for test purposes
        auto namePos = json.find("\"name\":\"");
        auto agePos = json.find("\"age\":");

        if (namePos != std::string::npos && agePos != std::string::npos) {
            namePos += 8;  // Move past "name":"
            auto nameEndPos = json.find("\"", namePos);
            if (nameEndPos != std::string::npos) {
                name_ = json.substr(namePos, nameEndPos - namePos);
            }

            agePos += 6;  // Move past "age":
            auto ageEndPos = json.find("}", agePos);
            if (ageEndPos != std::string::npos) {
                try {
                    age_ = std::stoi(json.substr(agePos, ageEndPos - agePos));
                    return true;
                } catch (...) {
                    return false;
                }
            }
        }
        return false;
    }

    // Clone method for cloneable_dispatch
    TestPerson clone() const { return TestPerson(name_, age_); }

    // Equality operator for comparable_dispatch
    bool operator==(const TestPerson& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }

    // Less than operator for comparable_dispatch
    bool operator<(const TestPerson& other) const {
        if (name_ != other.name_) {
            return name_ < other.name_;
        }
        return age_ < other.age_;
    }

    // Friend operator<< for printable_dispatch
    friend std::ostream& operator<<(std::ostream& os,
                                    const TestPerson& person) {
        os << "Person: " << person.name_ << ", Age: " << person.age_;
        return os;
    }

    // Getters
    const std::string& getName() const { return name_; }
    int getAge() const { return age_; }

private:
    std::string name_;
    int age_;
};

// Define a callable test class
class TestCallable {
public:
    TestCallable(int factor = 1) : factor_(factor) {}

    // No-argument call
    int operator()() const { return 42 * factor_; }

    // Single std::any argument call
    std::string operator()(const std::any& arg) const {
        try {
            if (arg.type() == typeid(int)) {
                int val = std::any_cast<int>(arg);
                return "Int: " + std::to_string(val * factor_);
            } else if (arg.type() == typeid(std::string)) {
                std::string val = std::any_cast<std::string>(arg);
                return "String: " + val;
            }
        } catch (const std::bad_any_cast&) {
            return "Bad cast";
        }
        return "Unknown type";
    }

private:
    int factor_;
};

// Test fixture for EnhancedBoxedValue tests
class EnhancedBoxedValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize common test values
        intValue = 42;
        doubleValue = 3.14159;
        stringValue = "Hello, World!";
        boolValue = true;
        personValue = TestPerson("Alice", 30);
        callableValue = TestCallable(2);
    }

    // Test values of different types
    int intValue;
    double doubleValue;
    std::string stringValue;
    bool boolValue;
    TestPerson personValue;
    TestCallable callableValue;
};

// Test basic construction and value retrieval
TEST_F(EnhancedBoxedValueTest, BasicConstruction) {
    // Test constructor with various types
    EnhancedBoxedValue intVal(intValue);
    EnhancedBoxedValue doubleVal(doubleValue);
    EnhancedBoxedValue stringVal(stringValue);
    EnhancedBoxedValue personVal(personValue);

    // Check if values are properly stored
    EXPECT_TRUE(intVal.hasValue());
    EXPECT_TRUE(intVal.hasProxy());
    EXPECT_TRUE(doubleVal.hasValue());
    EXPECT_TRUE(stringVal.hasValue());
    EXPECT_TRUE(personVal.hasValue());

    // Check if types are correctly identified
    EXPECT_TRUE(intVal.isType<int>());
    EXPECT_TRUE(doubleVal.isType<double>());
    EXPECT_TRUE(stringVal.isType<std::string>());
    EXPECT_TRUE(personVal.isType<TestPerson>());

    // Test construction with description
    EnhancedBoxedValue namedIntVal(intValue, "Answer to Life");
    EXPECT_TRUE(namedIntVal.hasValue());
    EXPECT_TRUE(namedIntVal.isType<int>());

    // Test default constructor (undefined value)
    EnhancedBoxedValue emptyVal;
    EXPECT_FALSE(emptyVal.hasValue());
    EXPECT_FALSE(emptyVal.hasProxy());
}

// Test string conversion using toString method
TEST_F(EnhancedBoxedValueTest, StringConversion) {
    // Create enhanced values
    EnhancedBoxedValue intVal(intValue);
    EnhancedBoxedValue doubleVal(doubleValue);
    EnhancedBoxedValue stringVal(stringValue);
    EnhancedBoxedValue personVal(personValue);
    EnhancedBoxedValue emptyVal;

    // Test toString() for various types
    EXPECT_EQ(intVal.toString(), "42");
    EXPECT_THAT(doubleVal.toString(), HasSubstr("3.14159"));
    EXPECT_EQ(stringVal.toString(), "Hello, World!");
    EXPECT_EQ(personVal.toString(),
              "Alice (30)");  // Uses TestPerson::toString()

    // Empty value should return some indication it's undefined
    EXPECT_THAT(emptyVal.toString(), HasSubstr("undef"));
}

// Test JSON serialization/deserialization
TEST_F(EnhancedBoxedValueTest, JsonSerialization) {
    // Create enhanced values
    EnhancedBoxedValue intVal(intValue);
    EnhancedBoxedValue stringVal(stringValue);
    EnhancedBoxedValue boolVal(boolValue);
    EnhancedBoxedValue personVal(personValue);

    // Test toJson() for various types
    EXPECT_EQ(intVal.toJson(), "42");  // Basic numeric serialization
    EXPECT_EQ(stringVal.toJson(),
              "\"Hello, World!\"");       // String should be quoted
    EXPECT_EQ(boolVal.toJson(), "true");  // Boolean values as true/false
    EXPECT_EQ(personVal.toJson(),
              "{\"name\":\"Alice\",\"age\":30}");  // Custom serialization

    // Test deserialization
    EnhancedBoxedValue newPerson(TestPerson("Bob", 25));
    EXPECT_EQ(newPerson.toString(), "Bob (25)");

    // Deserialize from JSON to update the value
    bool success = newPerson.fromJson("{\"name\":\"Charlie\",\"age\":35}");
    EXPECT_TRUE(success);
    EXPECT_EQ(newPerson.toString(), "Charlie (35)");

    // Test with invalid JSON
    success = newPerson.fromJson("{invalid json}");
    EXPECT_FALSE(success);
    EXPECT_EQ(newPerson.toString(), "Charlie (35)");  // Value shouldn't change
}

// Test printing to stream
TEST_F(EnhancedBoxedValueTest, PrintingCapabilities) {
    // Create enhanced values
    EnhancedBoxedValue intVal(intValue);
    EnhancedBoxedValue stringVal(stringValue);
    EnhancedBoxedValue personVal(personValue);

    // Test stream output for various types
    std::ostringstream ossInt, ossString, ossPerson;

    intVal.print(ossInt);
    EXPECT_EQ(ossInt.str(), "42");

    stringVal.print(ossString);
    EXPECT_EQ(ossString.str(), "Hello, World!");

    personVal.print(ossPerson);
    EXPECT_EQ(ossPerson.str(), "Person: Alice, Age: 30");  // Uses operator<<

    // Test stream insertion operator
    std::ostringstream ossOperator;
    ossOperator << personVal;
    EXPECT_EQ(ossOperator.str(), "Person: Alice, Age: 30");
}

// Test equality comparison
TEST_F(EnhancedBoxedValueTest, EqualityComparison) {
    // Create enhanced values for testing equality
    EnhancedBoxedValue intVal1(42);
    EnhancedBoxedValue intVal2(42);
    EnhancedBoxedValue intVal3(100);

    EnhancedBoxedValue person1(TestPerson("Alice", 30));
    EnhancedBoxedValue person2(TestPerson("Alice", 30));
    EnhancedBoxedValue person3(TestPerson("Bob", 25));

    // Test equality with same type and value
    EXPECT_TRUE(intVal1.equals(intVal2));
    EXPECT_TRUE(intVal1 == intVal2);  // Test operator==
    EXPECT_TRUE(person1.equals(person2));
    EXPECT_TRUE(person1 == person2);  // Test operator==

    // Test inequality with same type but different value
    EXPECT_FALSE(intVal1.equals(intVal3));
    EXPECT_FALSE(intVal1 == intVal3);
    EXPECT_FALSE(person1.equals(person3));
    EXPECT_FALSE(person1 == person3);

    // Test inequality with different types
    EXPECT_FALSE(intVal1.equals(person1));
    EXPECT_FALSE(intVal1 == person1);
}

// Test callable functionality
TEST_F(EnhancedBoxedValueTest, CallableFunction) {
    // Create enhanced value containing a callable object
    EnhancedBoxedValue callableVal(callableValue);

    // Test calling with no arguments
    std::any result = callableVal.call();
    ASSERT_FALSE(result.has_value() == false);
    EXPECT_EQ(std::any_cast<int>(result), 84);  // 42 * 2 (factor set in SetUp)

    // Test calling with one argument (int)
    std::vector<std::any> intArgs = {123};
    result = callableVal.call(intArgs);
    EXPECT_EQ(std::any_cast<std::string>(result), "Int: 246");  // 123 * 2

    // Test calling with one argument (string)
    std::vector<std::any> stringArgs = {std::string("test")};
    result = callableVal.call(stringArgs);
    EXPECT_EQ(std::any_cast<std::string>(result), "String: test");

    // Test with a lambda function
    auto lambda = [](int x) { return x * x; };
    EnhancedBoxedValue lambdaVal(lambda);

    // Lambda doesn't match our callable_dispatch interface expectations
    // so it will return an empty any
    result = lambdaVal.call({std::any(5)});
    EXPECT_FALSE(result.has_value());
}

// Test cloning
TEST_F(EnhancedBoxedValueTest, Cloning) {
    // Create an enhanced value to clone
    EnhancedBoxedValue personVal(personValue);

    // Clone the value
    EnhancedBoxedValue clonedVal = personVal.clone();

    // Verify clone is equal but separate
    EXPECT_TRUE(personVal == clonedVal);
    EXPECT_TRUE(personVal.hasProxy() && clonedVal.hasProxy());

    // Verify clone works with primitive types
    EnhancedBoxedValue intVal(intValue);
    EnhancedBoxedValue clonedInt = intVal.clone();
    EXPECT_TRUE(intVal == clonedInt);
    EXPECT_EQ(clonedInt.toString(), "42");
}

// Test attribute management
TEST_F(EnhancedBoxedValueTest, AttributeManagement) {
    // Create a value with attributes
    EnhancedBoxedValue personVal(personValue);

    // Add attributes
    personVal.setAttr("nickname", EnhancedBoxedValue(std::string("Al")));
    personVal.setAttr("score", EnhancedBoxedValue(95));

    // Check attribute existence
    EXPECT_TRUE(personVal.hasAttr("nickname"));
    EXPECT_TRUE(personVal.hasAttr("score"));
    EXPECT_FALSE(personVal.hasAttr("nonexistent"));

    // Retrieve and check attributes
    EnhancedBoxedValue nickname = personVal.getAttr("nickname");
    EXPECT_TRUE(nickname.isType<std::string>());
    EXPECT_EQ(nickname.toString(), "Al");

    EnhancedBoxedValue score = personVal.getAttr("score");
    EXPECT_TRUE(score.isType<int>());
    EXPECT_EQ(score.toString(), "95");

    // Test getting nonexistent attribute
    EnhancedBoxedValue nonexistent = personVal.getAttr("nonexistent");
    EXPECT_FALSE(nonexistent.hasValue());

    // List attributes
    auto attrNames = personVal.listAttrs();
    EXPECT_EQ(attrNames.size(), 2);
    EXPECT_TRUE(std::find(attrNames.begin(), attrNames.end(), "nickname") !=
                attrNames.end());
    EXPECT_TRUE(std::find(attrNames.begin(), attrNames.end(), "score") !=
                attrNames.end());

    // Remove an attribute
    personVal.removeAttr("nickname");
    EXPECT_FALSE(personVal.hasAttr("nickname"));
    EXPECT_TRUE(personVal.hasAttr("score"));

    // Reset the value
    personVal.reset();
    EXPECT_FALSE(personVal.hasValue());
    EXPECT_FALSE(personVal.hasProxy());
    EXPECT_FALSE(personVal.hasAttr("score"));  // Attributes should be gone
}

// Test type checking and casting
TEST_F(EnhancedBoxedValueTest, TypeCheckingAndCasting) {
    // Create enhanced values
    EnhancedBoxedValue intVal(intValue);
    EnhancedBoxedValue doubleVal(doubleValue);
    EnhancedBoxedValue stringVal(stringValue);
    EnhancedBoxedValue personVal(personValue);

    // Test type checking
    EXPECT_TRUE(intVal.isType<int>());
    EXPECT_FALSE(intVal.isType<double>());
    EXPECT_FALSE(intVal.isType<std::string>());

    EXPECT_TRUE(doubleVal.isType<double>());
    EXPECT_TRUE(stringVal.isType<std::string>());
    EXPECT_TRUE(personVal.isType<TestPerson>());

    // Test successful casting
    auto intOpt = intVal.tryCast<int>();
    EXPECT_TRUE(intOpt.has_value());
    EXPECT_EQ(*intOpt, 42);

    auto personOpt = personVal.tryCast<TestPerson>();
    EXPECT_TRUE(personOpt.has_value());
    EXPECT_EQ(personOpt->getName(), "Alice");
    EXPECT_EQ(personOpt->getAge(), 30);

    // Test failed casting
    auto failedCast = intVal.tryCast<std::string>();
    EXPECT_FALSE(failedCast.has_value());
}

// Test copy and move semantics
TEST_F(EnhancedBoxedValueTest, CopyAndMoveSemantics) {
    // Create original value
    EnhancedBoxedValue original(personValue);

    // Test copy constructor
    EnhancedBoxedValue copied(original);
    EXPECT_TRUE(copied.hasValue());
    EXPECT_TRUE(copied.hasProxy());
    EXPECT_TRUE(copied == original);

    // Test move constructor
    EnhancedBoxedValue moved(std::move(copied));
    EXPECT_TRUE(moved.hasValue());
    EXPECT_TRUE(moved.hasProxy());
    EXPECT_TRUE(moved == original);
    EXPECT_FALSE(copied.hasProxy());  // Source should be cleared

    // Test copy assignment
    EnhancedBoxedValue assigned;
    assigned = original;
    EXPECT_TRUE(assigned.hasValue());
    EXPECT_TRUE(assigned.hasProxy());
    EXPECT_TRUE(assigned == original);

    // Test move assignment
    EnhancedBoxedValue moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_TRUE(moveAssigned.hasValue());
    EXPECT_TRUE(moveAssigned.hasProxy());
    EXPECT_TRUE(moveAssigned == original);
    EXPECT_FALSE(moved.hasProxy());  // Source should be cleared

    // Test assignment from value
    EnhancedBoxedValue directAssigned;
    directAssigned = 100;
    EXPECT_TRUE(directAssigned.hasValue());
    EXPECT_TRUE(directAssigned.isType<int>());
    EXPECT_EQ(directAssigned.toString(), "100");
}

// Test edge cases and null values
TEST_F(EnhancedBoxedValueTest, EdgeCasesAndNullValues) {
    // Test null and undefined values
    EnhancedBoxedValue nullVal;
    EXPECT_FALSE(nullVal.hasValue());
    EXPECT_FALSE(nullVal.hasProxy());

    // Test that toString and toJson still work on null values
    EXPECT_FALSE(nullVal.toString().empty());
    EXPECT_FALSE(nullVal.toJson().empty());

    // Test equality comparison with null values
    EnhancedBoxedValue anotherNullVal;
    EXPECT_TRUE(nullVal == anotherNullVal);  // Two null values should be equal

    // Test that calling methods on null values doesn't crash
    std::ostringstream oss;
    nullVal.print(oss);
    EXPECT_FALSE(oss.str().empty());

    // Test call returns empty any
    std::any result = nullVal.call();
    EXPECT_FALSE(result.has_value());

    // Test cloning null produces null
    EnhancedBoxedValue clonedNull = nullVal.clone();
    EXPECT_FALSE(clonedNull.hasValue());

    // Test getting TypeInfo still works
    const TypeInfo& typeInfo = nullVal.getTypeInfo();
    EXPECT_FALSE(typeInfo.name().empty());
}

// Test convenience factory functions
TEST_F(EnhancedBoxedValueTest, ConvenienceFactoryFunctions) {
    // Test enhancedVar
    auto intVal = enhancedVar(42);
    EXPECT_TRUE(intVal.hasValue());
    EXPECT_TRUE(intVal.isType<int>());
    EXPECT_EQ(intVal.toString(), "42");

    // Test enhancedVarWithDesc
    auto stringVal = enhancedVarWithDesc(std::string("Hello"), "greeting");
    EXPECT_TRUE(stringVal.hasValue());
    EXPECT_TRUE(stringVal.isType<std::string>());
    EXPECT_EQ(stringVal.toString(), "Hello");
}
