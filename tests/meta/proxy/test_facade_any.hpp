#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <array>
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
    TestPerson personValue{"", 0};
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

//==============================================================================
// Additional crafted types to exercise every reachable skill-dispatch branch
//==============================================================================

// Convertible to std::string (no toString/to_string) -> stringable convertible
// branch and printable "[unprintable]" fallback.
struct StringConvertible {
    std::string s;
    explicit StringConvertible(std::string v = "conv") : s(std::move(v)) {}
    operator std::string() const { return s; }
};

// snake_case to_string() only -> stringable has_to_string + printable
// has_to_string branches.
struct SnakeStringable {
    std::string to_string() const { return "snake"; }
};

// toString() but no operator<< -> printable has_toString branch.
struct ToStringOnly {
    std::string toString() const { return "camel"; }
};

// No printable/stringable/comparable/serializable traits at all -> every
// fallback branch (unprintable, no string conversion, equals=false,
// serialize "null", deserialize false, clone via copy-construct).
struct Opaque {
    int v = 0;
};

// toJson()/fromJson() but no serialize/deserialize -> serialize_impl toJson
// branch, json_convertible toJson/fromJson branches.
struct JsonCamel {
    int n = 7;
    std::string toJson() const { return "{\"n\":" + std::to_string(n) + "}"; }
    bool fromJson(const std::string& j) {
        n = static_cast<int>(j.size());
        return true;
    }
};

// to_json()/from_json() (snake_case) -> the snake_case JSON branches.
struct JsonSnake {
    int n = 3;
    std::string to_json() const { return "snake_json"; }
    bool from_json(const std::string&) { return true; }
};

// Larger than restrict_layout<256> -> forced onto the deep-copying HeapHolder,
// exercising HeapHolder operator==/operator</operator<< and the copy-only
// init path. Provides comparison + streaming so the held type is usable.
struct BigStreamable {
    std::array<char, 512> pad{};
    int id = 0;
    bool operator==(const BigStreamable& o) const { return id == o.id; }
    bool operator<(const BigStreamable& o) const { return id < o.id; }
    friend std::ostream& operator<<(std::ostream& os, const BigStreamable& b) {
        return os << "Big(" << b.id << ")";
    }
};

// Callable returning void (no args and single-arg) -> the void branches of
// callable_dispatch::call_impl.
struct VoidCallable {
    mutable int calls = 0;
    void operator()() const { ++calls; }
    void operator()(const std::any&) const { ++calls; }
};

TEST(FacadeAnyDispatchTest, StringableConvertibleBranch) {
    EnhancedBoxedValue v(StringConvertible{"hi"});
    EXPECT_EQ(v.toString(), "hi");
}

TEST(FacadeAnyDispatchTest, StringableSnakeToStringBranch) {
    EnhancedBoxedValue v(SnakeStringable{});
    EXPECT_EQ(v.toString(), "snake");
    std::ostringstream oss;
    v.print(oss);
    EXPECT_EQ(oss.str(), "snake");
}

TEST(FacadeAnyDispatchTest, PrintableToStringBranch) {
    EnhancedBoxedValue v(ToStringOnly{});
    std::ostringstream oss;
    v.print(oss);
    EXPECT_EQ(oss.str(), "camel");
}

TEST(FacadeAnyDispatchTest, AllFallbackBranchesForOpaqueType) {
    EnhancedBoxedValue a(Opaque{1});
    EnhancedBoxedValue b(Opaque{1});

    // stringable fallback
    EXPECT_THAT(a.toString(), HasSubstr("no string conversion"));
    // printable fallback
    std::ostringstream oss;
    a.print(oss);
    EXPECT_THAT(oss.str(), HasSubstr("unprintable"));
    // comparable fallback: no operator== -> equals returns false even when the
    // underlying values are identical.
    EXPECT_FALSE(a.equals(b));
    // serializable fallback -> "null"
    EXPECT_EQ(a.toJson(), "null");
    // deserialize fallback -> false
    EXPECT_FALSE(a.fromJson("{}"));
    // clone via copy constructor (no clone() member)
    EnhancedBoxedValue c = a.clone();
    EXPECT_TRUE(c.isType<Opaque>());
}

TEST(FacadeAnyDispatchTest, SerializeArithmeticAndBoolAndStringBranches) {
    EXPECT_EQ(EnhancedBoxedValue(true).toJson(), "true");
    EXPECT_EQ(EnhancedBoxedValue(false).toJson(), "false");
    EXPECT_EQ(EnhancedBoxedValue(123).toJson(), "123");
    EXPECT_EQ(EnhancedBoxedValue(std::string("hey")).toJson(), "\"hey\"");
}

TEST(FacadeAnyDispatchTest, JsonCamelCaseBranches) {
    EnhancedBoxedValue v(JsonCamel{});
    EXPECT_EQ(v.toJson(), "{\"n\":7}");
    EXPECT_TRUE(v.fromJson("abcd"));
}

TEST(FacadeAnyDispatchTest, JsonSnakeCaseBranches) {
    EnhancedBoxedValue v(JsonSnake{});
    EXPECT_EQ(v.toJson(), "snake_json");
    EXPECT_TRUE(v.fromJson("anything"));
}

TEST(FacadeAnyDispatchTest, HeapHolderPathForLargeType) {
    BigStreamable big;
    big.id = 5;
    EnhancedBoxedValue v(big);
    EXPECT_TRUE(v.hasProxy());
    EXPECT_TRUE(v.isType<BigStreamable>());

    // HeapHolder operator<< via print
    std::ostringstream oss;
    v.print(oss);
    EXPECT_EQ(oss.str(), "Big(5)");

    // HeapHolder operator== via equals
    EnhancedBoxedValue same(big);
    EXPECT_TRUE(v.equals(same));

    BigStreamable other;
    other.id = 9;
    EnhancedBoxedValue diff(other);
    EXPECT_FALSE(v.equals(diff));

    // clone of a heap-held value
    EnhancedBoxedValue cloned = v.clone();
    EXPECT_TRUE(cloned.isType<BigStreamable>());
}

TEST(FacadeAnyDispatchTest, VoidCallableBranches) {
    EnhancedBoxedValue v(VoidCallable{});
    // no-arg void call returns empty any
    std::any r0 = v.call();
    EXPECT_FALSE(r0.has_value());
    // single-arg void call returns empty any
    std::any r1 = v.call({std::any(1)});
    EXPECT_FALSE(r1.has_value());
}

TEST(FacadeAnyDispatchTest, ConstructFromBoxedValueUsesVisitor) {
    // The BoxedValue constructor routes through initProxy()/ProxyVisitor
    // rather than the typed fast-path.
    BoxedValue bv(42);
    EnhancedBoxedValue v(bv);
    EXPECT_TRUE(v.hasProxy());
    EXPECT_TRUE(v.isType<int>());
    EXPECT_EQ(v.toString(), "42");
}

TEST(FacadeAnyDispatchTest, GetProxyAndBoxedValueAccessors) {
    EnhancedBoxedValue v(7);
    // getBoxedValue accessor
    EXPECT_TRUE(v.getBoxedValue().isType<int>());
    // getProxy succeeds when a proxy exists
    EXPECT_NO_THROW((void)v.getProxy());

    // getProxy throws when there is no proxy
    EnhancedBoxedValue empty;
    EXPECT_THROW((void)empty.getProxy(), std::runtime_error);
}

TEST(FacadeAnyDispatchTest, GetTypeInfoForTypedValue) {
    EnhancedBoxedValue v(3.5);
    EXPECT_FALSE(v.getTypeInfo().name().empty());
}

TEST(FacadeAnyDispatchTest, CopyAssignEmptyOverValueResetsProxy) {
    EnhancedBoxedValue valued(99);
    EnhancedBoxedValue empty;
    valued = empty;  // copy-assign with other.has_proxy_ == false
    EXPECT_FALSE(valued.hasProxy());
    EXPECT_FALSE(valued.hasValue());
}

//==============================================================================
// Direct exercise of the skill-dispatch implementations.
//
// EnhancedBoxedValue routes JSON through json_convertible_dispatch and never
// invokes comparable_dispatch::less_than_impl or serializable_dispatch
// directly, so several reachable branches in those implementations can only be
// covered by calling the static dispatch helpers explicitly. These are part of
// the public skill protocol and are meaningful to test on their own.
//==============================================================================

namespace eas = atom::meta::enhanced_any_skills;

TEST(FacadeAnyDispatchImplTest, ComparableEqualsImplBranches) {
    int a = 1, b = 1, c = 2;
    EXPECT_TRUE(eas::comparable_dispatch::equals_impl<int>(&a, &b, typeid(int)));
    EXPECT_FALSE(eas::comparable_dispatch::equals_impl<int>(&a, &c, typeid(int)));

    // Type mismatch -> early false.
    double d = 1.0;
    EXPECT_FALSE(
        eas::comparable_dispatch::equals_impl<int>(&a, &d, typeid(double)));

    // No operator== (Opaque) -> fallback false even for identical values.
    Opaque o1{1}, o2{1};
    EXPECT_FALSE(eas::comparable_dispatch::equals_impl<Opaque>(&o1, &o2,
                                                              typeid(Opaque)));
}

TEST(FacadeAnyDispatchImplTest, ComparableLessThanImplBranches) {
    int a = 1, b = 2;
    // Arithmetic less-than branch.
    EXPECT_TRUE(
        eas::comparable_dispatch::less_than_impl<int>(&a, &b, typeid(int)));
    EXPECT_FALSE(
        eas::comparable_dispatch::less_than_impl<int>(&b, &a, typeid(int)));

    // Type mismatch -> typeid ordering (deterministic, just exercise it).
    double d = 1.0;
    (void)eas::comparable_dispatch::less_than_impl<int>(&a, &d, typeid(double));

    // Custom operator< (BigStreamable).
    BigStreamable lo;
    lo.id = 1;
    BigStreamable hi;
    hi.id = 2;
    EXPECT_TRUE(eas::comparable_dispatch::less_than_impl<BigStreamable>(
        &lo, &hi, typeid(BigStreamable)));

    // No operator< (Opaque) -> fallback false.
    Opaque o1{1}, o2{2};
    EXPECT_FALSE(eas::comparable_dispatch::less_than_impl<Opaque>(
        &o1, &o2, typeid(Opaque)));
}

TEST(FacadeAnyDispatchImplTest, SerializeImplAllBranches) {
    TestPerson tp("Alice", 30);  // has serialize()
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<TestPerson>(&tp),
              "{\"name\":\"Alice\",\"age\":30}");

    JsonCamel jc;  // has toJson()
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<JsonCamel>(&jc),
              "{\"n\":7}");

    JsonSnake js;  // has to_json()
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<JsonSnake>(&js),
              "snake_json");

    std::string str = "x";
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<std::string>(&str),
              "\"x\"");
    bool bt = true;
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<bool>(&bt), "true");
    int n = 5;
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<int>(&n), "5");
    Opaque o{0};  // no serializer -> "null"
    EXPECT_EQ(eas::serializable_dispatch::serialize_impl<Opaque>(&o), "null");
}

TEST(FacadeAnyDispatchImplTest, DeserializeImplAllBranches) {
    TestPerson tp("a", 1);  // has deserialize()
    EXPECT_TRUE(eas::serializable_dispatch::deserialize_impl<TestPerson>(
        &tp, "{\"name\":\"b\",\"age\":2}"));

    JsonCamel jc;  // has fromJson()
    EXPECT_TRUE(
        eas::serializable_dispatch::deserialize_impl<JsonCamel>(&jc, "abcd"));

    JsonSnake js;  // has from_json()
    EXPECT_TRUE(
        eas::serializable_dispatch::deserialize_impl<JsonSnake>(&js, "x"));

    Opaque o{0};  // no deserializer -> false
    EXPECT_FALSE(eas::serializable_dispatch::deserialize_impl<Opaque>(&o, "x"));
}

// A pointer value: the typed fast-path declines (pointers are excluded) and
// falls through to the visitor, which also declines pointers -> no proxy.
TEST(FacadeAnyDispatchImplTest, PointerValueFallsThroughToNoProxy) {
    int x = 5;
    EnhancedBoxedValue v(&x);
    EXPECT_FALSE(v.hasProxy());
    EXPECT_TRUE(v.hasValue());
}
