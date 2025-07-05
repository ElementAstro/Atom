#include <gtest/gtest.h>

#include "atom/type/string.hpp"

// New test fixture for String class
class StringTest : public ::testing::Test {
protected:
    String emptyString;
    String basicString{"Hello, world!"};
    String multilineString{"Line 1\nLine 2\nLine 3"};
    String unicodeString{"こんにちは世界"};
    String spacedString{"  Hello  world  "};
};

// Construction Tests
TEST_F(StringTest, Construction) {
    // Default constructor
    String s1;
    EXPECT_TRUE(s1.empty());
    EXPECT_EQ(s1.length(), 0);

    // C-string constructor
    String s2("test");
    EXPECT_EQ(s2.data(), "test");

    // std::string constructor
    std::string stdStr = "standard string";
    String s3(stdStr);
    EXPECT_EQ(s3.data(), stdStr);

    // string_view constructor
    std::string_view sv = "string view";
    String s4(sv);
    EXPECT_EQ(s4.data(), sv);

    // nullptr handling
    String s5(nullptr);
    EXPECT_TRUE(s5.empty());

    // Copy constructor
    String s6(basicString);
    EXPECT_EQ(s6.data(), basicString.data());

    // Move constructor
    String temp("move me");
    String s7(std::move(temp));
    EXPECT_EQ(s7.data(), "move me");
}

// Assignment Tests
TEST_F(StringTest, Assignment) {
    // Copy assignment
    String s1;
    s1 = basicString;
    EXPECT_EQ(s1.data(), basicString.data());

    // Move assignment
    String temp("move me");
    String s2;
    s2 = std::move(temp);
    EXPECT_EQ(s2.data(), "move me");

    // Self-assignment
    String s3("self");
    s3 = s3;
    EXPECT_EQ(s3.data(), "self");
}

// Comparison Tests
TEST_F(StringTest, Comparison) {
    String s1("abc");
    String s2("abc");
    String s3("def");

    // Equality
    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 == s3);

    // Three-way comparison
    EXPECT_TRUE(s1 < s3);
    EXPECT_FALSE(s3 < s1);
    EXPECT_TRUE(s3 > s1);

    // Case-insensitive comparison
    String uppercase("ABC");
    EXPECT_FALSE(s1 == uppercase);
    EXPECT_TRUE(s1.equalsIgnoreCase(uppercase));
}

// Concatenation Tests
TEST_F(StringTest, Concatenation) {
    String s1("Hello");
    String s2(" World");

    // += operator with String
    s1 += s2;
    EXPECT_EQ(s1.data(), "Hello World");

    // += operator with C-string
    s1 += "!";
    EXPECT_EQ(s1.data(), "Hello World!");

    // += operator with char
    s1 += '?';
    EXPECT_EQ(s1.data(), "Hello World!?");

    // + operator
    String s3 = String("a") + String("b");
    EXPECT_EQ(s3.data(), "ab");

    // Error handling
    EXPECT_THROW(s1 += nullptr, StringException);
}

// Access Methods Tests
TEST_F(StringTest, Access) {
    // cStr
    EXPECT_STREQ(basicString.cStr(), "Hello, world!");

    // length and size
    EXPECT_EQ(basicString.length(), 13);
    EXPECT_EQ(basicString.size(), 13);

    // capacity
    EXPECT_GE(basicString.capacity(), basicString.length());

    // reserve
    String s1("test");
    size_t originalCapacity = s1.capacity();
    s1.reserve(100);
    EXPECT_GE(s1.capacity(), 100);
    EXPECT_EQ(s1.data(), "test");  // Content should be unchanged

    // at (with bounds checking)
    EXPECT_EQ(basicString.at(0), 'H');
    EXPECT_EQ(basicString.at(12), '!');
    EXPECT_THROW(basicString.at(13), StringException);

    // operator[] (no bounds checking)
    EXPECT_EQ(basicString[0], 'H');
    EXPECT_EQ(basicString[12], '!');

    // data and dataRef
    EXPECT_EQ(basicString.data(), "Hello, world!");
    EXPECT_EQ(basicString.dataRef(), "Hello, world!");

    // dataRef modification
    String s2("modify");
    s2.dataRef() = "changed";
    EXPECT_EQ(s2.data(), "changed");
}

// String Operations Tests
TEST_F(StringTest, Substring) {
    // Basic substring
    EXPECT_EQ(basicString.substr(0, 5).data(), "Hello");
    EXPECT_EQ(basicString.substr(7, 5).data(), "world");

    // Empty substring
    EXPECT_EQ(basicString.substr(13, 5).data(), "");

    // Substring to end
    EXPECT_EQ(basicString.substr(7).data(), "world!");

    // Exception cases
    EXPECT_THROW(basicString.substr(14), StringException);
}

TEST_F(StringTest, Find) {
    // Basic find
    EXPECT_EQ(basicString.find(String("Hello")), 0);
    EXPECT_EQ(basicString.find(String("world")), 7);
    EXPECT_EQ(basicString.find(String("!")), 12);

    // Not found
    EXPECT_EQ(basicString.find(String("xyz")), String::NPOS);

    // Find with offset
    EXPECT_EQ(basicString.find(String("o"), 0), 4);
    EXPECT_EQ(basicString.find(String("o"), 5), 8);

    // Empty string
    EXPECT_EQ(basicString.find(String("")), 0);
    EXPECT_EQ(emptyString.find(String("a")), String::NPOS);

    // FindOptimized should work the same for these test cases
    EXPECT_EQ(basicString.findOptimized(String("Hello")), 0);
    EXPECT_EQ(basicString.findOptimized(String("xyz")), String::NPOS);
}

TEST_F(StringTest, Replace) {
    // Single replace
    String s1(basicString);
    EXPECT_TRUE(s1.replace(String("Hello"), String("Hi")));
    EXPECT_EQ(s1.data(), "Hi, world!");

    // Replace not found
    EXPECT_FALSE(s1.replace(String("xyz"), String("abc")));
    EXPECT_EQ(s1.data(), "Hi, world!");

    // Replace with empty string
    EXPECT_TRUE(s1.replace(String("Hi"), String("")));
    EXPECT_EQ(s1.data(), ", world!");

    // Replace empty string (should return false)
    EXPECT_FALSE(s1.replace(String(""), String("xyz")));
    EXPECT_EQ(s1.data(), ", world!");
}

TEST_F(StringTest, ReplaceAll) {
    // Multiple replacements
    String s1("one two one two one");
    size_t count = s1.replaceAll(String("one"), String("three"));
    EXPECT_EQ(count, 3);
    EXPECT_EQ(s1.data(), "three two three two three");

    // Replace with longer string
    String s2("aaa");
    count = s2.replaceAll(String("a"), String("bb"));
    EXPECT_EQ(count, 3);
    EXPECT_EQ(s2.data(), "bbbbbb");

    // Replace with shorter string
    String s3("aaa");
    count = s3.replaceAll(String("a"), String(""));
    EXPECT_EQ(count, 3);
    EXPECT_EQ(s3.data(), "");

    // Empty string to replace
    EXPECT_THROW(s1.replaceAll(String(""), String("x")), StringException);

    // No matches
    String s4("abc");
    count = s4.replaceAll(String("x"), String("y"));
    EXPECT_EQ(count, 0);
    EXPECT_EQ(s4.data(), "abc");

    // ReplaceAllParallel (for smaller strings it falls back to replaceAll)
    String s5("one two one two one");
    count = s5.replaceAllParallel(String("one"), String("three"));
    EXPECT_EQ(count, 3);
    EXPECT_EQ(s5.data(), "three two three two three");
}

TEST_F(StringTest, StringTransformations) {
    // toUpper
    String s1("Hello, World!");
    EXPECT_EQ(s1.toUpper().data(), "HELLO, WORLD!");

    // toLower
    String s2("Hello, World!");
    EXPECT_EQ(s2.toLower().data(), "hello, world!");

    // reverse
    String s3("abcdef");
    EXPECT_EQ(s3.reverse().data(), "fedcba");
    EXPECT_EQ(emptyString.reverse().data(), "");

    // reverseWords
    String s4("one two three");
    EXPECT_EQ(s4.reverseWords().data(), "three two one");
    EXPECT_EQ(String(" ").reverseWords().data(), " ");
}

TEST_F(StringTest, SplitAndJoin) {
    // Split
    String s1("one,two,three");
    auto parts = s1.split(String(","));
    ASSERT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0].data(), "one");
    EXPECT_EQ(parts[1].data(), "two");
    EXPECT_EQ(parts[2].data(), "three");

    // Split with empty delimiter (returns whole string)
    parts = s1.split(String(""));
    ASSERT_EQ(parts.size(), 1);
    EXPECT_EQ(parts[0].data(), s1.data());

    // Split empty string
    parts = emptyString.split(String(","));
    EXPECT_TRUE(parts.empty());

    // Join
    std::vector<String> strings = {String("a"), String("b"), String("c")};
    EXPECT_EQ(String::join(strings, String("-")).data(), "a-b-c");

    // Join empty vector
    std::vector<String> emptyVec;
    EXPECT_EQ(String::join(emptyVec, String("-")).data(), "");

    // Join with empty separator
    EXPECT_EQ(String::join(strings, String("")).data(), "abc");
}

TEST_F(StringTest, TrimOperations) {
    // trim
    String s1("  Hello  ");
    s1.trim();
    EXPECT_EQ(s1.data(), "Hello");

    // ltrim
    String s2("  Hello  ");
    s2.ltrim();
    EXPECT_EQ(s2.data(), "Hello  ");

    // rtrim
    String s3("  Hello  ");
    s3.rtrim();
    EXPECT_EQ(s3.data(), "  Hello");

    // Various whitespace characters
    String s4("\t\n Hello \r\n");
    s4.trim();
    EXPECT_EQ(s4.data(), "Hello");
}

TEST_F(StringTest, PrefixSuffixOperations) {
    // startsWith
    EXPECT_TRUE(basicString.startsWith(String("Hello")));
    EXPECT_FALSE(basicString.startsWith(String("hello")));
    EXPECT_TRUE(basicString.startsWith(String("")));
    EXPECT_FALSE(basicString.startsWith(String("Hello, world!!")));

    // endsWith
    EXPECT_TRUE(basicString.endsWith(String("world!")));
    EXPECT_FALSE(basicString.endsWith(String("World!")));
    EXPECT_TRUE(basicString.endsWith(String("")));
    EXPECT_FALSE(basicString.endsWith(String("Hello, world!!")));

    // removePrefix
    String s1(basicString);
    EXPECT_TRUE(s1.removePrefix(String("Hello, ")));
    EXPECT_EQ(s1.data(), "world!");
    EXPECT_FALSE(s1.removePrefix(String("Hello")));
    EXPECT_EQ(s1.data(), "world!");

    // removeSuffix
    String s2(basicString);
    EXPECT_TRUE(s2.removeSuffix(String("world!")));
    EXPECT_EQ(s2.data(), "Hello, ");
    EXPECT_FALSE(s2.removeSuffix(String("World!")));
    EXPECT_EQ(s2.data(), "Hello, ");
}

TEST_F(StringTest, ContainsMethods) {
    // contains string
    EXPECT_TRUE(basicString.contains(String("Hello")));
    EXPECT_TRUE(basicString.contains(String("world")));
    EXPECT_TRUE(basicString.contains(String("")));
    EXPECT_FALSE(basicString.contains(String("xyz")));

    // contains char
    EXPECT_TRUE(basicString.contains('H'));
    EXPECT_TRUE(basicString.contains('!'));
    EXPECT_FALSE(basicString.contains('z'));
}

TEST_F(StringTest, CharacterOperations) {
    // replace char
    String s1("hello");
    size_t count = s1.replace('l', 'x');
    EXPECT_EQ(count, 2);
    EXPECT_EQ(s1.data(), "hexxo");

    // insert char
    String s2("hello");
    s2.insert(0, '*');
    EXPECT_EQ(s2.data(), "*hello");
    s2.insert(6, '*');
    EXPECT_EQ(s2.data(), "*hello*");
    EXPECT_THROW(s2.insert(8, '*'), StringException);

    // insert string
    String s3("hello");
    s3.insert(0, String("**"));
    EXPECT_EQ(s3.data(), "**hello");
    s3.insert(7, String("**"));
    EXPECT_EQ(s3.data(), "**hello**");
    EXPECT_THROW(s3.insert(10, String("**")), StringException);

    // remove char
    String s4("hello");
    count = s4.remove('l');
    EXPECT_EQ(count, 2);
    EXPECT_EQ(s4.data(), "heo");

    // removeAll string
    String s5("hello hello");
    count = s5.removeAll(String("lo"));
    EXPECT_EQ(count, 2);
    EXPECT_EQ(s5.data(), "hel hel");

    // erase
    String s6("hello");
    s6.erase(1, 3);
    EXPECT_EQ(s6.data(), "ho");
    EXPECT_THROW(s6.erase(3, 1), StringException);
}

TEST_F(StringTest, PaddingMethods) {
    // padLeft
    String s1("hello");
    s1.padLeft(10);
    EXPECT_EQ(s1.data(), "     hello");
    s1.padLeft(5);  // No change if already longer
    EXPECT_EQ(s1.data(), "     hello");

    // padRight
    String s2("hello");
    s2.padRight(10);
    EXPECT_EQ(s2.data(), "hello     ");
    s2.padRight(5);  // No change if already longer
    EXPECT_EQ(s2.data(), "hello     ");

    // Custom padding character
    String s3("hello");
    s3.padLeft(10, '*');
    EXPECT_EQ(s3.data(), "*****hello");

    String s4("hello");
    s4.padRight(10, '*');
    EXPECT_EQ(s4.data(), "hello*****");
}

TEST_F(StringTest, UtilityMethods) {
    // compressSpaces
    String s1("hello   world    test");
    s1.compressSpaces();
    EXPECT_EQ(s1.data(), "hello world test");

    // hash
    String s2("hello");
    String s3("hello");
    String s4("world");
    EXPECT_EQ(s2.hash(), s3.hash());
    EXPECT_NE(s2.hash(), s4.hash());

    // swap
    String a("first");
    String b("second");
    a.swap(b);
    EXPECT_EQ(a.data(), "second");
    EXPECT_EQ(b.data(), "first");

    // Global swap function
    swap(a, b);
    EXPECT_EQ(a.data(), "first");
    EXPECT_EQ(b.data(), "second");
}

TEST_F(StringTest, RegexOperations) {
    // Basic regex replace
    String s1("hello123world456");
    String result = s1.replaceRegex("\\d+", "X");
    EXPECT_EQ(result.data(), "helloXworldX");

    // More complex regex
    String s2("2023-01-15");
    result = s2.replaceRegex("(\\d{4})-(\\d{2})-(\\d{2})", "$2/$3/$1");
    EXPECT_EQ(result.data(), "01/15/2023");

    // Invalid regex should throw
    EXPECT_THROW(s1.replaceRegex("[", "X"), StringException);
}

TEST_F(StringTest, FormatMethods) {
    // Basic formatting
    String result = String::format("Hello, {}!", "world");
    EXPECT_EQ(result.data(), "Hello, world!");

    // Multiple arguments
    result = String::format("{} + {} = {}", 1, 2, 3);
    EXPECT_EQ(result.data(), "1 + 2 = 3");

    // Positional arguments
    result = String::format("{1} {0} {1}", "world", "Hello");
    EXPECT_EQ(result.data(), "Hello world Hello");

    // Invalid format should throw
    EXPECT_THROW(String::format("{", "error"), StringException);

    // Safe version shouldn't throw
    auto optResult = String::formatSafe("{", "error");
    EXPECT_FALSE(optResult.has_value());

    optResult = String::formatSafe("Valid {}", "format");
    EXPECT_TRUE(optResult.has_value());
    if (optResult) {
        EXPECT_EQ(optResult->data(), "Valid format");
    }
}

TEST_F(StringTest, StreamOperations) {
    // Output stream
    std::ostringstream oss;
    oss << basicString;
    EXPECT_EQ(oss.str(), "Hello, world!");

    // Input stream
    std::istringstream iss("test");
    String s;
    iss >> s;
    EXPECT_EQ(s.data(), "test");

    // Input stream with error handling
    std::istringstream badStream;
    badStream.setstate(std::ios::failbit);
    String s2("original");
    badStream >> s2;
    EXPECT_EQ(s2.data(), "original");  // Should remain unchanged
}
