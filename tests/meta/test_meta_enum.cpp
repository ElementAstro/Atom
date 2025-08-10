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

    static constexpr underlying_type min_value() noexcept { return 0; }
    static constexpr underlying_type max_value() noexcept { return 3; }
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

    static constexpr underlying_type min_value() noexcept { return 0; }
    static constexpr underlying_type max_value() noexcept { return 7; }
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
    void SetUp() override {}
    void TearDown() override {}
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

    // Test invalid flag name
    auto invalid = atom::meta::deserialize_flags<Permissions>("Read|Invalid");
    EXPECT_FALSE(invalid.has_value());
}

// Test edge cases and error conditions
TEST_F(EnumTest, EdgeCasesAndErrorConditions) {
    // Test with invalid enum values created by casting
    Color invalidColor = static_cast<Color>(999);
    EXPECT_TRUE(atom::meta::enum_name(invalidColor).empty());
    EXPECT_FALSE(atom::meta::enum_contains(invalidColor));

    // Test integer_to_enum with invalid values
    auto invalidFromInt = atom::meta::integer_to_enum<Color>(999);
    EXPECT_FALSE(invalidFromInt.has_value());

    // Test empty string cases
    auto emptyEnum = atom::meta::enum_cast<Color>("");
    EXPECT_FALSE(emptyEnum.has_value());

    auto emptyIcase = atom::meta::enum_cast_icase<Color>("");
    EXPECT_FALSE(emptyIcase.has_value());
}

}  // namespace atom::test
