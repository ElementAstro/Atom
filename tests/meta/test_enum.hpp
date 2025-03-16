// atom/function/test_enum.hpp
#ifndef ATOM_TEST_ENUM_HPP
#define ATOM_TEST_ENUM_HPP

#include <gtest/gtest.h>
#include "atom/function/enum.hpp"

#include <array>
#include <string_view>

namespace atom::test {

// Simple test enum
enum class Color { Red, Green, Blue, Yellow };

// Flag test enum with power-of-two values for bitwise operations
enum class Permissions : uint8_t {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    All = Read | Write | Execute  // 7
};

}  // namespace atom::test

// 在正确的命名空间下特化EnumTraits
namespace atom::meta {

// Specialized EnumTraits for Color
template <>
struct EnumTraits<test::Color> {
    static constexpr std::array<test::Color, 4> values = {
        test::Color::Red, test::Color::Green, test::Color::Blue,
        test::Color::Yellow};

    static constexpr std::array<std::string_view, 4> names = {"Red", "Green",
                                                              "Blue", "Yellow"};

    static constexpr std::array<std::string_view, 4> descriptions = {
        "The color red", "The color green", "The color blue",
        "The color yellow"};
};

// Specialized EnumTraits for Permissions
template <>
struct EnumTraits<test::Permissions> {
    static constexpr std::array<test::Permissions, 5> values = {
        test::Permissions::None, test::Permissions::Read,
        test::Permissions::Write, test::Permissions::Execute,
        test::Permissions::All};

    static constexpr std::array<std::string_view, 5> names = {
        "None", "Read", "Write", "Execute", "All"};

    static constexpr std::array<std::string_view, 5> descriptions = {
        "No permissions", "Read permission", "Write permission",
        "Execute permission", "All permissions"};
};

// Specialized EnumAliasTraits for Permissions
template <>
struct EnumAliasTraits<test::Permissions> {
    static constexpr std::array<std::string_view, 5> aliases = {
        "Empty", "R", "W", "X", "RWX"};
};

}  // namespace atom::meta

namespace atom::test {

// 使运算符重载可用
using atom::meta::operator|;
using atom::meta::operator&;
using atom::meta::operator^;
using atom::meta::operator~;
using atom::meta::operator|=;
using atom::meta::operator&=;
using atom::meta::operator^=;

// Test fixture for enum tests
class EnumTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Teardown code if needed
    }
};

// Test converting enum to string name
TEST_F(EnumTest, EnumToString) {
    // Test basic enum value to string conversion
    EXPECT_EQ(atom::meta::enum_name(Color::Red), "Red");
    EXPECT_EQ(atom::meta::enum_name(Color::Green), "Green");
    EXPECT_EQ(atom::meta::enum_name(Color::Blue), "Blue");
    EXPECT_EQ(atom::meta::enum_name(Color::Yellow), "Yellow");

    // Test with flag enum
    EXPECT_EQ(atom::meta::enum_name(Permissions::Read), "Read");
    EXPECT_EQ(atom::meta::enum_name(Permissions::Write), "Write");
    EXPECT_EQ(atom::meta::enum_name(Permissions::None), "None");
    EXPECT_EQ(atom::meta::enum_name(Permissions::All), "All");

    // Test with invalid value (using a cast to create an invalid value)
    // Should return empty string for invalid enums
    Color invalidColor = static_cast<Color>(99);
    EXPECT_TRUE(atom::meta::enum_name(invalidColor).empty());
}

// Test string to enum conversion
TEST_F(EnumTest, StringToEnum) {
    // Basic cast
    auto red = atom::meta::enum_cast<Color>("Red");
    EXPECT_TRUE(red.has_value());
    EXPECT_EQ(red.value(), Color::Red);

    // Test with flag enum
    auto write = atom::meta::enum_cast<Permissions>("Write");
    EXPECT_TRUE(write.has_value());
    EXPECT_EQ(write.value(), Permissions::Write);

    // Test with non-existent value
    auto none = atom::meta::enum_cast<Color>("Purple");
    EXPECT_FALSE(none.has_value());
}

// Test converting enum to integer
TEST_F(EnumTest, EnumToInteger) {
    // Test with simple enum
    EXPECT_EQ(atom::meta::enum_to_integer(Color::Red), 0);
    EXPECT_EQ(atom::meta::enum_to_integer(Color::Green), 1);
    EXPECT_EQ(atom::meta::enum_to_integer(Color::Blue), 2);

    // Test with flag enum that has explicit values
    EXPECT_EQ(atom::meta::enum_to_integer(Permissions::None), 0);
    EXPECT_EQ(atom::meta::enum_to_integer(Permissions::Read), 1);
    EXPECT_EQ(atom::meta::enum_to_integer(Permissions::Write), 2);
    EXPECT_EQ(atom::meta::enum_to_integer(Permissions::Execute), 4);
    EXPECT_EQ(atom::meta::enum_to_integer(Permissions::All),
              7);  // Read | Write | Execute = 1 | 2 | 4 = 7
}

// Test converting integer to enum
TEST_F(EnumTest, IntegerToEnum) {
    // Test with simple enum
    auto red = atom::meta::integer_to_enum<Color>(0);
    EXPECT_TRUE(red.has_value());
    EXPECT_EQ(red.value(), Color::Red);

    // Test with flag enum
    auto write = atom::meta::integer_to_enum<Permissions>(2);
    EXPECT_TRUE(write.has_value());
    EXPECT_EQ(write.value(), Permissions::Write);

    auto all = atom::meta::integer_to_enum<Permissions>(7);
    EXPECT_TRUE(all.has_value());
    EXPECT_EQ(all.value(), Permissions::All);

    // Test with non-existent value
    auto invalid = atom::meta::integer_to_enum<Color>(99);
    EXPECT_FALSE(invalid.has_value());
}

// Test checking if enum contains value
TEST_F(EnumTest, EnumContains) {
    // Test with valid values
    EXPECT_TRUE(atom::meta::enum_contains(Color::Red));
    EXPECT_TRUE(atom::meta::enum_contains(Color::Green));
    EXPECT_TRUE(atom::meta::enum_contains(Permissions::Read));
    EXPECT_TRUE(atom::meta::enum_contains(Permissions::All));

    // Test with invalid value
    Color invalidColor = static_cast<Color>(99);
    EXPECT_FALSE(atom::meta::enum_contains(invalidColor));

    Permissions invalidPerm = static_cast<Permissions>(99);
    EXPECT_FALSE(atom::meta::enum_contains(invalidPerm));
}

// Test getting all enum entries
TEST_F(EnumTest, EnumEntries) {
    // Get all Color entries
    auto colorEntries = atom::meta::enum_entries<Color>();
    EXPECT_EQ(colorEntries.size(), 4);

    // Check first entry
    EXPECT_EQ(colorEntries[0].first, Color::Red);
    EXPECT_EQ(colorEntries[0].second, "Red");

    // Check last entry
    EXPECT_EQ(colorEntries[3].first, Color::Yellow);
    EXPECT_EQ(colorEntries[3].second, "Yellow");

    // Get all Permission entries
    auto permEntries = atom::meta::enum_entries<Permissions>();
    EXPECT_EQ(permEntries.size(), 5);

    // Check All permission
    EXPECT_EQ(permEntries[4].first, Permissions::All);
    EXPECT_EQ(permEntries[4].second, "All");
}

// Test bitwise operations on flag enum
TEST_F(EnumTest, BitwiseOperations) {
    // Test OR operation
    auto readWrite = Permissions::Read | Permissions::Write;
    EXPECT_EQ(atom::meta::enum_to_integer(readWrite), 3);  // 1 | 2 = 3

    // Test AND operation
    auto readAndAll = Permissions::Read & Permissions::All;
    EXPECT_EQ(readAndAll, Permissions::Read);

    // Test XOR operation
    auto readXorAll = Permissions::Read ^ Permissions::All;
    EXPECT_EQ(atom::meta::enum_to_integer(readXorAll),
              6);  // 1 ^ 7 = 6 (Write|Execute)

    // Test NOT operation
    auto notRead = ~Permissions::Read;
    // ~1 = 11111110 in binary for uint8_t
    EXPECT_EQ(atom::meta::enum_to_integer(notRead), 0xFE);

    // Test compound assignment
    Permissions perms = Permissions::Read;
    perms |= Permissions::Write;
    EXPECT_EQ(atom::meta::enum_to_integer(perms), 3);  // Read|Write

    perms &= Permissions::Write;
    EXPECT_EQ(perms, Permissions::Write);

    perms ^= Permissions::All;
    EXPECT_EQ(atom::meta::enum_to_integer(perms), 5);  // Write^All = 2^7 = 5
}

// Test getting default enum value
TEST_F(EnumTest, EnumDefault) {
    EXPECT_EQ(atom::meta::enum_default<Color>(), Color::Red);
    EXPECT_EQ(atom::meta::enum_default<Permissions>(), Permissions::None);
}

// Test sorting enum values by name
TEST_F(EnumTest, SortingByName) {
    auto sortedByName = atom::meta::enum_sorted_by_name<Color>();

    // Alphabetical order: Blue, Green, Red, Yellow
    EXPECT_EQ(sortedByName[0].first, Color::Blue);
    EXPECT_EQ(sortedByName[1].first, Color::Green);
    EXPECT_EQ(sortedByName[2].first, Color::Red);
    EXPECT_EQ(sortedByName[3].first, Color::Yellow);
}

// Test sorting enum values by value
TEST_F(EnumTest, SortingByValue) {
    auto sortedByValue = atom::meta::enum_sorted_by_value<Permissions>();

    // Value order: None(0), Read(1), Write(2), Execute(4), All(7)
    EXPECT_EQ(sortedByValue[0].first, Permissions::None);
    EXPECT_EQ(sortedByValue[1].first, Permissions::Read);
    EXPECT_EQ(sortedByValue[2].first, Permissions::Write);
    EXPECT_EQ(sortedByValue[3].first, Permissions::Execute);
    EXPECT_EQ(sortedByValue[4].first, Permissions::All);
}

// Test fuzzy matching for enum names
TEST_F(EnumTest, FuzzyMatching) {
    // Test partial matches
    auto blueMatch =
        atom::meta::enum_cast_fuzzy<Color>("lu");  // should match "Blue"
    EXPECT_TRUE(blueMatch.has_value());
    EXPECT_EQ(blueMatch.value(), Color::Blue);

    auto greenMatch =
        atom::meta::enum_cast_fuzzy<Color>("ree");  // should match "Green"
    EXPECT_TRUE(greenMatch.has_value());
    EXPECT_EQ(greenMatch.value(), Color::Green);

    // Test non-matching
    auto noMatch = atom::meta::enum_cast_fuzzy<Color>("Purple");
    EXPECT_FALSE(noMatch.has_value());
}

// Test checking if integer is within enum range
TEST_F(EnumTest, IntegerInEnumRange) {
    EXPECT_TRUE(atom::meta::integer_in_enum_range<Color>(0));    // Red
    EXPECT_TRUE(atom::meta::integer_in_enum_range<Color>(3));    // Yellow
    EXPECT_FALSE(atom::meta::integer_in_enum_range<Color>(99));  // Invalid

    EXPECT_TRUE(atom::meta::integer_in_enum_range<Permissions>(0));  // None
    EXPECT_TRUE(atom::meta::integer_in_enum_range<Permissions>(7));  // All
    EXPECT_FALSE(atom::meta::integer_in_enum_range<Permissions>(
        3));  // Not explicitly defined
    EXPECT_FALSE(
        atom::meta::integer_in_enum_range<Permissions>(99));  // Invalid
}

// Test enum aliases
TEST_F(EnumTest, EnumAliases) {
    // Test with valid aliases
    auto read = atom::meta::enum_cast_with_alias<Permissions>("R");
    EXPECT_TRUE(read.has_value());
    EXPECT_EQ(read.value(), Permissions::Read);

    auto all = atom::meta::enum_cast_with_alias<Permissions>("RWX");
    EXPECT_TRUE(all.has_value());
    EXPECT_EQ(all.value(), Permissions::All);

    // Test with original names still working
    auto write = atom::meta::enum_cast_with_alias<Permissions>("Write");
    EXPECT_TRUE(write.has_value());
    EXPECT_EQ(write.value(), Permissions::Write);

    // Test with non-existent alias
    auto nonExistent =
        atom::meta::enum_cast_with_alias<Permissions>("NotExists");
    EXPECT_FALSE(nonExistent.has_value());
}

// Test enum descriptions
TEST_F(EnumTest, EnumDescriptions) {
    EXPECT_EQ(atom::meta::enum_description(Color::Red), "The color red");
    EXPECT_EQ(atom::meta::enum_description(Color::Green), "The color green");

    EXPECT_EQ(atom::meta::enum_description(Permissions::Read),
              "Read permission");
    EXPECT_EQ(atom::meta::enum_description(Permissions::All),
              "All permissions");

    // Test with invalid value
    Color invalidColor = static_cast<Color>(99);
    EXPECT_TRUE(atom::meta::enum_description(invalidColor).empty());
}

// Test enum serialization and deserialization
TEST_F(EnumTest, EnumSerialization) {
    // Serialize enum to string
    std::string redString = atom::meta::serialize_enum(Color::Red);
    EXPECT_EQ(redString, "Red");

    std::string writeString = atom::meta::serialize_enum(Permissions::Write);
    EXPECT_EQ(writeString, "Write");

    // Deserialize string to enum
    auto red = atom::meta::deserialize_enum<Color>("Red");
    EXPECT_TRUE(red.has_value());
    EXPECT_EQ(red.value(), Color::Red);

    auto write = atom::meta::deserialize_enum<Permissions>("Write");
    EXPECT_TRUE(write.has_value());
    EXPECT_EQ(write.value(), Permissions::Write);

    // Test with invalid string
    auto invalid = atom::meta::deserialize_enum<Color>("NotAColor");
    EXPECT_FALSE(invalid.has_value());
}

// Test checking if enum value is within range
TEST_F(EnumTest, EnumInRange) {
    EXPECT_TRUE(
        atom::meta::enum_in_range(Color::Green, Color::Red, Color::Yellow));
    EXPECT_TRUE(atom::meta::enum_in_range(Color::Red, Color::Red, Color::Blue));
    EXPECT_TRUE(
        atom::meta::enum_in_range(Color::Yellow, Color::Yellow, Color::Yellow));
    EXPECT_FALSE(
        atom::meta::enum_in_range(Color::Yellow, Color::Red, Color::Blue));

    // Test with flag enum
    EXPECT_TRUE(atom::meta::enum_in_range(Permissions::Write, Permissions::None,
                                          Permissions::All));
    EXPECT_FALSE(atom::meta::enum_in_range(Permissions::All, Permissions::None,
                                           Permissions::Execute));
}

// Test bitmask functions
TEST_F(EnumTest, Bitmask) {
    EXPECT_EQ(atom::meta::enum_bitmask(Permissions::None), 0);
    EXPECT_EQ(atom::meta::enum_bitmask(Permissions::Read), 1);
    EXPECT_EQ(atom::meta::enum_bitmask(Permissions::Write), 2);
    EXPECT_EQ(atom::meta::enum_bitmask(Permissions::Execute), 4);
    EXPECT_EQ(atom::meta::enum_bitmask(Permissions::All), 7);

    // Test bitmask to enum conversion
    auto readFromMask = atom::meta::bitmask_to_enum<Permissions>(1);
    EXPECT_TRUE(readFromMask.has_value());
    EXPECT_EQ(readFromMask.value(), Permissions::Read);

    auto allFromMask = atom::meta::bitmask_to_enum<Permissions>(7);
    EXPECT_TRUE(allFromMask.has_value());
    EXPECT_EQ(allFromMask.value(), Permissions::All);

    // Test with non-existent bitmask
    auto nonExistent = atom::meta::bitmask_to_enum<Permissions>(
        3);  // Read|Write, but not defined as an enum value
    EXPECT_FALSE(nonExistent.has_value());
}

// Test with more complex usage patterns
TEST_F(EnumTest, ComplexUsage) {
    // Test combining operations
    Permissions perms = Permissions::None;

    // Add read permission
    if (!atom::meta::enum_cast<Permissions>("Read").has_value()) {
        FAIL() << "Read permission not found";
    }
    perms |= atom::meta::enum_cast<Permissions>("Read").value();

    // Add write permission using alias
    if (!atom::meta::enum_cast_with_alias<Permissions>("W").has_value()) {
        FAIL() << "Write permission alias not found";
    }
    perms |= atom::meta::enum_cast_with_alias<Permissions>("W").value();

    // Check result
    EXPECT_EQ(atom::meta::enum_to_integer(perms), 3);  // Read|Write = 3

    // Convert permission to string for display
    std::string permStr = atom::meta::serialize_enum(perms);
    // This won't be "Read|Write" because perms doesn't match any single enum
    // value exactly It will return the name of the enum value that exactly
    // matches perms, or empty if none
    EXPECT_TRUE(
        permStr.empty());  // Because Read|Write is not named in our enum

    // Check if specific permissions are set
    bool hasRead = (perms & Permissions::Read) == Permissions::Read;
    bool hasWrite = (perms & Permissions::Write) == Permissions::Write;
    bool hasExecute = (perms & Permissions::Execute) == Permissions::Execute;

    EXPECT_TRUE(hasRead);
    EXPECT_TRUE(hasWrite);
    EXPECT_FALSE(hasExecute);
}

// Test compile-time extraction of enum name
// Note: This test is more to ensure the code compiles rather than runtime
// behavior
TEST_F(EnumTest, CompileTimeEnumName) {
    // Can't directly test the compile-time function, but we can test its usage
    constexpr auto redName = atom::meta::enum_name<Color, Color::Red>();
    // We can check if it's a constexpr result
    static_assert(!redName.empty(), "Enum name should not be empty");

    // Also check if enum_name with runtime value gives consistent results
    EXPECT_EQ(std::string_view(redName), atom::meta::enum_name(Color::Red));
}

}  // namespace atom::test

#endif  // ATOM_TEST_ENUM_HPP