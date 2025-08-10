// atom/meta/test_enum.hpp
#ifndef ATOM_TEST_ENUM_HPP
#define ATOM_TEST_ENUM_HPP

#include <gtest/gtest.h>
#include "atom/meta/enum.hpp"

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

// Specialize EnumTraits in the correct namespace
namespace atom::meta {

// Complete EnumTraits specialization for Color
template <>
struct EnumTraits<test::Color> {
    using enum_type = test::Color;
    using underlying_type = std::underlying_type_t<test::Color>;

    static constexpr std::array<test::Color, 4> values = {
        test::Color::Red, test::Color::Green, test::Color::Blue, test::Color::Yellow};

    static constexpr std::array<std::string_view, 4> names = {
        "Red", "Green", "Blue", "Yellow"};

    static constexpr std::array<std::string_view, 4> descriptions = {
        "The color red", "The color green", "The color blue", "The color yellow"};

    static constexpr std::array<std::string_view, 4> aliases = {
        "", "", "", ""};

    static constexpr bool is_flags = false;
    static constexpr bool is_sequential = true;
    static constexpr bool is_continuous = true;
    static constexpr test::Color default_value = test::Color::Red;
    static constexpr std::string_view type_name = "Color";
    static constexpr std::string_view type_description = "Color enumeration";

    static constexpr underlying_type min_value() noexcept {
        return 0;
    }

    static constexpr underlying_type max_value() noexcept {
        return 3;
    }

    static constexpr size_t size() noexcept { return values.size(); }
    static constexpr bool empty() noexcept { return false; }

    static constexpr bool contains(test::Color value) noexcept {
        for (const auto& val : values) {
            if (val == value) return true;
        }
        return false;
    }
};

// Complete EnumTraits specialization for Permissions (as flag enum)
template <>
struct EnumTraits<test::Permissions> {
    using enum_type = test::Permissions;
    using underlying_type = std::underlying_type_t<test::Permissions>;

    static constexpr std::array<test::Permissions, 5> values = {
        test::Permissions::None, test::Permissions::Read, test::Permissions::Write,
        test::Permissions::Execute, test::Permissions::All};

    static constexpr std::array<std::string_view, 5> names = {
        "None", "Read", "Write", "Execute", "All"};

    static constexpr std::array<std::string_view, 5> descriptions = {
        "No permissions", "Read permission", "Write permission",
        "Execute permission", "All permissions"};

    static constexpr std::array<std::string_view, 5> aliases = {
        "Empty", "R", "W", "X", "RWX"};

    static constexpr bool is_flags = true;
    static constexpr bool is_sequential = false;
    static constexpr bool is_continuous = false;
    static constexpr test::Permissions default_value = test::Permissions::None;
    static constexpr std::string_view type_name = "Permissions";
    static constexpr std::string_view type_description = "Permission flags";

    static constexpr underlying_type min_value() noexcept {
        return 0;
    }

    static constexpr underlying_type max_value() noexcept {
        return 7;
    }

    static constexpr size_t size() noexcept { return values.size(); }
    static constexpr bool empty() noexcept { return false; }

    static constexpr bool contains(test::Permissions value) noexcept {
        for (const auto& val : values) {
            if (val == value) return true;
        }
        return false;
    }
};

}  // namespace atom::meta

namespace atom::test {

// Make operators available
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
    EXPECT_EQ(atom::meta::enum_to_integer(Permissions::All), 7);
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
    EXPECT_EQ(atom::meta::enum_to_integer(readXorAll), 6);  // 1 ^ 7 = 6 (Write|Execute)

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

// Test case-insensitive enum conversion
TEST_F(EnumTest, CaseInsensitiveEnumCast) {
    // Test basic case insensitive matching
    auto red = atom::meta::enum_cast_icase<Color>("red");
    EXPECT_TRUE(red.has_value());
    EXPECT_EQ(red.value(), Color::Red);

    auto green = atom::meta::enum_cast_icase<Color>("GREEN");
    EXPECT_TRUE(green.has_value());
    EXPECT_EQ(green.value(), Color::Green);

    auto blue = atom::meta::enum_cast_icase<Color>("bLuE");
    EXPECT_TRUE(blue.has_value());
    EXPECT_EQ(blue.value(), Color::Blue);

    // Test with flag enum
    auto write = atom::meta::enum_cast_icase<Permissions>("WRITE");
    EXPECT_TRUE(write.has_value());
    EXPECT_EQ(write.value(), Permissions::Write);

    // Test with non-existent value
    auto invalid = atom::meta::enum_cast_icase<Color>("purple");
    EXPECT_FALSE(invalid.has_value());
}

// Test prefix matching for enum names
TEST_F(EnumTest, PrefixMatching) {
    // Test prefix matches
    auto matches = atom::meta::enum_cast_prefix<Color>("Gr");
    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], Color::Green);

    auto yMatches = atom::meta::enum_cast_prefix<Color>("Y");
    EXPECT_EQ(yMatches.size(), 1);
    EXPECT_EQ(yMatches[0], Color::Yellow);

    // Test with no matches
    auto noMatches = atom::meta::enum_cast_prefix<Color>("Purple");
    EXPECT_TRUE(noMatches.empty());

    // Test with empty prefix (should match all)
    auto allMatches = atom::meta::enum_cast_prefix<Color>("");
    EXPECT_EQ(allMatches.size(), 4);
}

// Test fuzzy matching (corrected from existing test)
TEST_F(EnumTest, FuzzyMatchingCorrected) {
    // Test partial matches
    auto blueMatches = atom::meta::enum_cast_fuzzy<Color>("lu");
    EXPECT_EQ(blueMatches.size(), 1);
    EXPECT_EQ(blueMatches[0], Color::Blue);

    auto greenMatches = atom::meta::enum_cast_fuzzy<Color>("ree");
    EXPECT_EQ(greenMatches.size(), 1);
    EXPECT_EQ(greenMatches[0], Color::Green);

    // Test with no matches
    auto noMatches = atom::meta::enum_cast_fuzzy<Color>("Purple");
    EXPECT_TRUE(noMatches.empty());

    // Test matching multiple values
    auto eMatches = atom::meta::enum_cast_fuzzy<Color>("e");
    EXPECT_GE(eMatches.size(), 2); // Both Green and Blue contain 'e'
}

// Test flag enum specific functions
TEST_F(EnumTest, FlagEnumFunctions) {
    // Create combined flags
    Permissions readWrite = Permissions::Read | Permissions::Write;

    // Test has_flag function
    EXPECT_TRUE(atom::meta::has_flag(readWrite, Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(readWrite, Permissions::Write));
    EXPECT_FALSE(atom::meta::has_flag(readWrite, Permissions::Execute));

    // Test set_flag function
    auto withExecute = atom::meta::set_flag(readWrite, Permissions::Execute);
    EXPECT_TRUE(atom::meta::has_flag(withExecute, Permissions::Execute));
    EXPECT_TRUE(atom::meta::has_flag(withExecute, Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(withExecute, Permissions::Write));

    // Test clear_flag function
    auto withoutRead = atom::meta::clear_flag(readWrite, Permissions::Read);
    EXPECT_FALSE(atom::meta::has_flag(withoutRead, Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(withoutRead, Permissions::Write));

    // Test toggle_flag function
    auto toggled = atom::meta::toggle_flag(readWrite, Permissions::Execute);
    EXPECT_TRUE(atom::meta::has_flag(toggled, Permissions::Execute));
    EXPECT_TRUE(atom::meta::has_flag(toggled, Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(toggled, Permissions::Write));

    auto toggledBack = atom::meta::toggle_flag(toggled, Permissions::Execute);
    EXPECT_FALSE(atom::meta::has_flag(toggledBack, Permissions::Execute));
    EXPECT_EQ(toggledBack, readWrite);
}

// Test get_set_flags function
TEST_F(EnumTest, GetSetFlags) {
    Permissions readWrite = Permissions::Read | Permissions::Write;

    auto setFlags = atom::meta::get_set_flags(readWrite);
    EXPECT_EQ(setFlags.size(), 2);

    // Flags should be in the order they appear in the enum values array
    bool foundRead = false, foundWrite = false;
    for (const auto& flag : setFlags) {
        if (flag == Permissions::Read) foundRead = true;
        if (flag == Permissions::Write) foundWrite = true;
    }
    EXPECT_TRUE(foundRead);
    EXPECT_TRUE(foundWrite);

    // Test with no flags set
    auto noFlags = atom::meta::get_set_flags(Permissions::None);
    EXPECT_EQ(noFlags.size(), 1); // None itself is a flag
    EXPECT_EQ(noFlags[0], Permissions::None);

    // Test with all flags
    auto allFlags = atom::meta::get_set_flags(Permissions::All);
    EXPECT_GE(allFlags.size(), 1); // At least the All flag itself
}

// Test flag serialization and deserialization
TEST_F(EnumTest, FlagSerialization) {
    // Test serializing individual flags
    std::string readStr = atom::meta::serialize_flags(Permissions::Read);
    EXPECT_EQ(readStr, "Read");

    // Test serializing combined flags
    Permissions readWrite = Permissions::Read | Permissions::Write;
    std::string readWriteStr = atom::meta::serialize_flags(readWrite);

    // Should contain both flag names separated by |
    EXPECT_TRUE(readWriteStr.find("Read") != std::string::npos);
    EXPECT_TRUE(readWriteStr.find("Write") != std::string::npos);
    EXPECT_TRUE(readWriteStr.find("|") != std::string::npos);

    // Test with custom separator
    std::string customSep = atom::meta::serialize_flags(readWrite, ",");
    EXPECT_TRUE(customSep.find(",") != std::string::npos);

    // Test serializing no flags
    std::string noneStr = atom::meta::serialize_flags(Permissions::None);
    EXPECT_EQ(noneStr, "None");
}

// Test flag deserialization
TEST_F(EnumTest, FlagDeserialization) {
    // Test deserializing single flag
    auto read = atom::meta::deserialize_flags<Permissions>("Read");
    EXPECT_TRUE(read.has_value());
    EXPECT_EQ(read.value(), Permissions::Read);

    // Test deserializing combined flags
    auto readWrite = atom::meta::deserialize_flags<Permissions>("Read|Write");
    EXPECT_TRUE(readWrite.has_value());
    EXPECT_TRUE(atom::meta::has_flag(readWrite.value(), Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(readWrite.value(), Permissions::Write));

    // Test with custom separator
    auto customSep = atom::meta::deserialize_flags<Permissions>("Read,Write", ",");
    EXPECT_TRUE(customSep.has_value());
    EXPECT_TRUE(atom::meta::has_flag(customSep.value(), Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(customSep.value(), Permissions::Write));

    // Test with whitespace
    auto withSpaces = atom::meta::deserialize_flags<Permissions>("Read | Write");
    EXPECT_TRUE(withSpaces.has_value());
    EXPECT_TRUE(atom::meta::has_flag(withSpaces.value(), Permissions::Read));
    EXPECT_TRUE(atom::meta::has_flag(withSpaces.value(), Permissions::Write));

    // Test empty string
    auto empty = atom::meta::deserialize_flags<Permissions>("");
    EXPECT_TRUE(empty.has_value());
    EXPECT_EQ(empty.value(), static_cast<Permissions>(0));

    // Test invalid flag name
    auto invalid = atom::meta::deserialize_flags<Permissions>("Read|Invalid");
    EXPECT_FALSE(invalid.has_value());
}

// Test EnumValidator functionality
TEST_F(EnumTest, EnumValidator) {
    // Create validator that only allows primary colors
    atom::meta::EnumValidator<Color> primaryColorValidator(
        [](Color c) {
            return c == Color::Red || c == Color::Green || c == Color::Blue;
        },
        "Only primary colors allowed"
    );

    // Test validation
    EXPECT_TRUE(primaryColorValidator.validate(Color::Red));
    EXPECT_TRUE(primaryColorValidator.validate(Color::Green));
    EXPECT_TRUE(primaryColorValidator.validate(Color::Blue));
    EXPECT_FALSE(primaryColorValidator.validate(Color::Yellow));

    // Test error message
    EXPECT_EQ(primaryColorValidator.error_message(), "Only primary colors allowed");

    // Test validated_cast
    auto red = primaryColorValidator.validated_cast("Red");
    EXPECT_TRUE(red.has_value());
    EXPECT_EQ(red.value(), Color::Red);

    auto yellow = primaryColorValidator.validated_cast("Yellow");
    EXPECT_FALSE(yellow.has_value());

    auto invalid = primaryColorValidator.validated_cast("Purple");
    EXPECT_FALSE(invalid.has_value());
}

// Test EnumIterator and enum_range functionality
TEST_F(EnumTest, EnumIteratorAndRange) {
    // Test iterator functionality
    atom::meta::EnumIterator<Color> it(0);
    EXPECT_EQ(*it, Color::Red);

    // Test increment
    ++it;
    EXPECT_EQ(*it, Color::Green);

    auto it2 = it++;
    EXPECT_EQ(*it2, Color::Green);
    EXPECT_EQ(*it, Color::Blue);

    // Test equality
    atom::meta::EnumIterator<Color> it3(1);
    EXPECT_EQ(it2, it3);
    EXPECT_NE(it, it3);

    // Test range-based for loop
    std::vector<Color> colors;
    for (auto color : atom::meta::enum_range<Color>()) {
        colors.push_back(color);
    }

    EXPECT_EQ(colors.size(), 4);
    EXPECT_EQ(colors[0], Color::Red);
    EXPECT_EQ(colors[1], Color::Green);
    EXPECT_EQ(colors[2], Color::Blue);
    EXPECT_EQ(colors[3], Color::Yellow);
}

// Test EnumReflection functionality
TEST_F(EnumTest, EnumReflection) {
    using ColorReflection = atom::meta::EnumReflection<Color>;
    using PermissionReflection = atom::meta::EnumReflection<Permissions>;

    // Test count
    EXPECT_EQ(ColorReflection::count(), 4);
    EXPECT_EQ(PermissionReflection::count(), 5);

    // Test metadata flags
    EXPECT_FALSE(ColorReflection::is_flags());
    EXPECT_TRUE(PermissionReflection::is_flags());

    // Test type information
    EXPECT_EQ(ColorReflection::type_name(), "Color");
    EXPECT_EQ(PermissionReflection::type_name(), "Permissions");

    // Test get_name and get_description
    EXPECT_EQ(ColorReflection::get_name(Color::Blue), "Blue");
    EXPECT_EQ(ColorReflection::get_description(Color::Red), "The color red");

    // Test from_name and from_integer
    auto red = ColorReflection::from_name("Red");
    EXPECT_TRUE(red.has_value());
    EXPECT_EQ(red.value(), Color::Red);

    auto redFromInt = ColorReflection::from_integer(0);
    EXPECT_TRUE(redFromInt.has_value());
    EXPECT_EQ(redFromInt.value(), Color::Red);
}

// Test edge cases and error conditions
TEST_F(EnumTest, EdgeCasesAndErrorConditions) {
    // Test with invalid enum values created by casting
    Color invalidColor = static_cast<Color>(999);
    EXPECT_TRUE(atom::meta::enum_name(invalidColor).empty());
    EXPECT_FALSE(atom::meta::enum_contains(invalidColor));
    EXPECT_TRUE(atom::meta::enum_description(invalidColor).empty());

    // Test enum_in_range with invalid values
    EXPECT_FALSE(atom::meta::enum_in_range(invalidColor, Color::Red, Color::Yellow));

    // Test integer_to_enum with invalid values
    auto invalidFromInt = atom::meta::integer_to_enum<Color>(999);
    EXPECT_FALSE(invalidFromInt.has_value());

    // Test empty string cases
    auto emptyEnum = atom::meta::enum_cast<Color>("");
    EXPECT_FALSE(emptyEnum.has_value());

    auto emptyIcase = atom::meta::enum_cast_icase<Color>("");
    EXPECT_FALSE(emptyIcase.has_value());
}

// Test string helper functions
TEST_F(EnumTest, StringHelperFunctions) {
    using namespace atom::meta::detail;

    // Test iequals
    EXPECT_TRUE(iequals("Red", "red"));
    EXPECT_TRUE(iequals("RED", "red"));
    EXPECT_TRUE(iequals("Red", "Red"));
    EXPECT_FALSE(iequals("Red", "Blue"));
    EXPECT_FALSE(iequals("Red", "Reda"));

    // Test starts_with
    EXPECT_TRUE(starts_with("Red", "R"));
    EXPECT_TRUE(starts_with("Green", "Gr"));
    EXPECT_TRUE(starts_with("Blue", "Blue"));
    EXPECT_FALSE(starts_with("Red", "Bl"));
    EXPECT_FALSE(starts_with("Red", "Reda"));

    // Test contains_substring
    EXPECT_TRUE(contains_substring("Blue", "lu"));
    EXPECT_TRUE(contains_substring("Green", "ree"));
    EXPECT_TRUE(contains_substring("Red", "Red"));
    EXPECT_TRUE(contains_substring("Yellow", ""));
    EXPECT_FALSE(contains_substring("Red", "Blue"));
    EXPECT_FALSE(contains_substring("Red", "RedBlue"));
}

// Test serialization and deserialization
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

// Test enum range functionality
TEST_F(EnumTest, EnumInRange) {
    EXPECT_TRUE(atom::meta::enum_in_range(Color::Green, Color::Red, Color::Yellow));
    EXPECT_TRUE(atom::meta::enum_in_range(Color::Red, Color::Red, Color::Blue));
    EXPECT_TRUE(atom::meta::enum_in_range(Color::Yellow, Color::Yellow, Color::Yellow));
    EXPECT_FALSE(atom::meta::enum_in_range(Color::Yellow, Color::Red, Color::Blue));

    // Test with flag enum
    EXPECT_TRUE(atom::meta::enum_in_range(Permissions::Write, Permissions::None, Permissions::All));
    EXPECT_FALSE(atom::meta::enum_in_range(Permissions::All, Permissions::None, Permissions::Execute));
}

// Test integer in enum range
TEST_F(EnumTest, IntegerInEnumRange) {
    EXPECT_TRUE(atom::meta::integer_in_enum_range<Color>(0));    // Red
    EXPECT_TRUE(atom::meta::integer_in_enum_range<Color>(3));    // Yellow
    EXPECT_FALSE(atom::meta::integer_in_enum_range<Color>(99));  // Invalid

    EXPECT_TRUE(atom::meta::integer_in_enum_range<Permissions>(0));  // None
    EXPECT_TRUE(atom::meta::integer_in_enum_range<Permissions>(7));  // All
    EXPECT_FALSE(atom::meta::integer_in_enum_range<Permissions>(99));  // Invalid
}

}  // namespace atom::test

#endif  // ATOM_TEST_ENUM_HPP
