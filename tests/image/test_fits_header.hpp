// filepath: atom/image/test_fits_header.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "atom/image/fits_header.hpp"

namespace atom::image::test {

class FITSHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup with some standard FITS keywords
        header.addKeyword("SIMPLE", "T");
        header.addKeyword("BITPIX", "16");
        header.addKeyword("NAXIS", "2");
        header.addKeyword("NAXIS1", "100");
        header.addKeyword("NAXIS2", "100");
    }

    // Helper to check if a specific data pattern exists in the serialized data
    bool containsPattern(const std::vector<char>& data, const std::string& pattern) {
        std::string data_str(data.begin(), data.end());
        return data_str.find(pattern) != std::string::npos;
    }

    FITSHeader header;
};

// Test adding and retrieving keywords
TEST_F(FITSHeaderTest, AddAndGetKeyword) {
    // Test existing keywords
    EXPECT_EQ(header.getKeywordValue("SIMPLE"), "T");
    EXPECT_EQ(header.getKeywordValue("BITPIX"), "16");
    EXPECT_EQ(header.getKeywordValue("NAXIS"), "2");

    // Add a new keyword
    header.addKeyword("OBJECT", "M31");
    EXPECT_EQ(header.getKeywordValue("OBJECT"), "M31");

    // Update an existing keyword
    header.addKeyword("BITPIX", "32");
    EXPECT_EQ(header.getKeywordValue("BITPIX"), "32");

    // Add a keyword with a longer value
    std::string long_value = "This is a longer value with spaces and special chars: !@#$%^&*()";
    header.addKeyword("COMMENT", long_value);
    EXPECT_EQ(header.getKeywordValue("COMMENT"), long_value);
}

// Test checking if a keyword exists
TEST_F(FITSHeaderTest, HasKeyword) {
    EXPECT_TRUE(header.hasKeyword("SIMPLE"));
    EXPECT_TRUE(header.hasKeyword("BITPIX"));
    EXPECT_FALSE(header.hasKeyword("NONEXIST"));

    // Check case sensitivity
    EXPECT_FALSE(header.hasKeyword("simple")); // FITS keywords should be case-sensitive
}

// Test removing keywords
TEST_F(FITSHeaderTest, RemoveKeyword) {
    EXPECT_TRUE(header.hasKeyword("BITPIX"));
    header.removeKeyword("BITPIX");
    EXPECT_FALSE(header.hasKeyword("BITPIX"));

    // Removing non-existent keyword should not throw
    EXPECT_NO_THROW(header.removeKeyword("NONEXIST"));
}

// Test getting all keywords
TEST_F(FITSHeaderTest, GetAllKeywords) {
    auto keywords = header.getAllKeywords();

    // Check that expected keywords are present
    EXPECT_THAT(keywords, ::testing::Contains("SIMPLE"));
    EXPECT_THAT(keywords, ::testing::Contains("BITPIX"));
    EXPECT_THAT(keywords, ::testing::Contains("NAXIS"));
    EXPECT_THAT(keywords, ::testing::Contains("NAXIS1"));
    EXPECT_THAT(keywords, ::testing::Contains("NAXIS2"));

    // Check that non-existent keywords are not present
    EXPECT_THAT(keywords, ::testing::Not(::testing::Contains("NONEXIST")));

    // Check the total count
    EXPECT_EQ(keywords.size(), 5);
}

// Test adding and retrieving comments
TEST_F(FITSHeaderTest, AddAndGetComments) {
    header.addComment("This is a test comment");
    header.addComment("Another comment");

    auto comments = header.getComments();
    EXPECT_EQ(comments.size(), 2);
    EXPECT_THAT(comments, ::testing::Contains("This is a test comment"));
    EXPECT_THAT(comments, ::testing::Contains("Another comment"));
}

// Test clearing comments
TEST_F(FITSHeaderTest, ClearComments) {
    header.addComment("Comment 1");
    header.addComment("Comment 2");

    EXPECT_EQ(header.getComments().size(), 2);

    header.clearComments();
    EXPECT_EQ(header.getComments().size(), 0);
}

// Test error handling for getKeywordValue
TEST_F(FITSHeaderTest, GetKeywordValueError) {
    EXPECT_THROW(header.getKeywordValue("NONEXIST"), FITSHeaderException);
}

// Test serialization
TEST_F(FITSHeaderTest, Serialization) {
    std::vector<char> data = header.serialize();

    // Check size is a multiple of FITS_HEADER_UNIT_SIZE
    EXPECT_EQ(data.size() % FITSHeader::FITS_HEADER_UNIT_SIZE, 0);

    // Check for expected patterns in the serialized data
    EXPECT_TRUE(containsPattern(data, "SIMPLE  =                    T"));
    EXPECT_TRUE(containsPattern(data, "BITPIX  =                   16"));
    EXPECT_TRUE(containsPattern(data, "NAXIS   =                    2"));

    // Check for END keyword at the end
    std::string end_pattern = "END     ";
    bool has_end = false;
    for (size_t i = 0; i <= data.size() - end_pattern.length(); i += FITSHeader::FITS_HEADER_CARD_SIZE) {
        if (std::strncmp(&data[i], end_pattern.c_str(), end_pattern.length()) == 0) {
            has_end = true;
            break;
        }
    }
    EXPECT_TRUE(has_end);
}

// Test deserialization
TEST_F(FITSHeaderTest, Deserialization) {
    // Serialize the current header
    std::vector<char> data = header.serialize();

    // Create a new header and deserialize into it
    FITSHeader new_header;
    new_header.deserialize(data);

    // Check that deserialized header has the same keywords
    EXPECT_TRUE(new_header.hasKeyword("SIMPLE"));
    EXPECT_TRUE(new_header.hasKeyword("BITPIX"));
    EXPECT_TRUE(new_header.hasKeyword("NAXIS"));
    EXPECT_TRUE(new_header.hasKeyword("NAXIS1"));
    EXPECT_TRUE(new_header.hasKeyword("NAXIS2"));

    // Check that values match
    EXPECT_EQ(new_header.getKeywordValue("SIMPLE"), "T");
    EXPECT_EQ(new_header.getKeywordValue("BITPIX"), "16");
    EXPECT_EQ(new_header.getKeywordValue("NAXIS"), "2");
}

// Test deserialization errors
TEST_F(FITSHeaderTest, DeserializationErrors) {
    // Test with empty data
    std::vector<char> empty_data;
    EXPECT_THROW(header.deserialize(empty_data), FITSHeaderException);

    // Test with data that's not a multiple of FITS_HEADER_CARD_SIZE
    std::vector<char> invalid_size_data(FITSHeader::FITS_HEADER_CARD_SIZE - 1, ' ');
    EXPECT_THROW(header.deserialize(invalid_size_data), FITSHeaderException);

    // Test with data that doesn't contain an END keyword
    std::vector<char> no_end_data(FITSHeader::FITS_HEADER_UNIT_SIZE, ' ');
    EXPECT_THROW(header.deserialize(no_end_data), FITSHeaderException);
}

// Test with very long keywords and values
TEST_F(FITSHeaderTest, LongKeywordsAndValues) {
    // Keyword longer than 8 chars should be truncated
    std::string long_keyword = "VERYLONGKEYWORD";
    header.addKeyword(long_keyword, "value");
    EXPECT_FALSE(header.hasKeyword(long_keyword));
    EXPECT_TRUE(header.hasKeyword(long_keyword.substr(0, 8)));

    // Value longer than 72 chars should be truncated
    std::string long_value(100, 'X');  // 100 X characters
    header.addKeyword("LONGVAL", long_value);
    EXPECT_EQ(header.getKeywordValue("LONGVAL").length(), 72);
}

// Test with special FITS keyword formats
TEST_F(FITSHeaderTest, SpecialKeywordFormats) {
    // Test HIERARCH convention for long keywords
    header.addKeyword("HIERARCH ESO DET CHIP TEMP", "-120.0");
    EXPECT_TRUE(header.hasKeyword("HIERARCH"));

    // Test with string value (should be quoted)
    header.addKeyword("TELESCOP", "'JWST'");
    EXPECT_EQ(header.getKeywordValue("TELESCOP"), "'JWST'");

    // Test with boolean value
    header.addKeyword("FLAG", "T");
    EXPECT_EQ(header.getKeywordValue("FLAG"), "T");

    // Test with numeric value
    header.addKeyword("EXPTIME", "1200.5");
    EXPECT_EQ(header.getKeywordValue("EXPTIME"), "1200.5");
}

// Test KeywordRecord constructor
TEST_F(FITSHeaderTest, KeywordRecordConstructor) {
    FITSHeader::KeywordRecord record("TEST", "value");

    // Check keyword is stored correctly
    std::array<char, 8> expected_keyword{'T', 'E', 'S', 'T', 0, 0, 0, 0};
    EXPECT_EQ(record.keyword, expected_keyword);

    // Check value is stored correctly
    std::array<char, 72> expected_value{};
    std::fill(expected_value.begin(), expected_value.end(), 0);
    std::copy(std::begin("value"), std::end("value"), expected_value.begin());
    EXPECT_EQ(record.value, expected_value);
}

// Test extensive FITS header
TEST_F(FITSHeaderTest, ExtensiveFITSHeader) {
    // Create a header with many keywords to test scaling behavior
    FITSHeader large_header;

    // Add 100 keywords
    for (int i = 0; i < 100; i++) {
        std::string keyword = "KEY" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        large_header.addKeyword(keyword.substr(0, 8), value);
    }

    // Check all keywords exist
    for (int i = 0; i < 100; i++) {
        std::string keyword = "KEY" + std::to_string(i);
        EXPECT_TRUE(large_header.hasKeyword(keyword.substr(0, 8)));
    }

    // Check serialization size
    std::vector<char> data = large_header.serialize();
    int expected_size = ((100 + 1) * FITSHeader::FITS_HEADER_CARD_SIZE + FITSHeader::FITS_HEADER_UNIT_SIZE - 1)
                         / FITSHeader::FITS_HEADER_UNIT_SIZE
                         * FITSHeader::FITS_HEADER_UNIT_SIZE;
    EXPECT_EQ(data.size(), expected_size);
}

// Test required FITS keywords
TEST_F(FITSHeaderTest, RequiredFITSKeywords) {
    // Create a minimal valid FITS header
    FITSHeader minimal_header;
    minimal_header.addKeyword("SIMPLE", "T");
    minimal_header.addKeyword("BITPIX", "16");
    minimal_header.addKeyword("NAXIS", "0");

    // Serialize and check
    std::vector<char> data = minimal_header.serialize();
    EXPECT_TRUE(containsPattern(data, "SIMPLE  =                    T"));
    EXPECT_TRUE(containsPattern(data, "BITPIX  =                   16"));
    EXPECT_TRUE(containsPattern(data, "NAXIS   =                    0"));

    // Required keywords should be in the correct order
    std::string data_str(data.begin(), data.end());
    size_t simple_pos = data_str.find("SIMPLE");
    size_t bitpix_pos = data_str.find("BITPIX");
    size_t naxis_pos = data_str.find("NAXIS");

    EXPECT_LT(simple_pos, bitpix_pos);
    EXPECT_LT(bitpix_pos, naxis_pos);
}

// Test CONTINUE keyword for long string values
TEST_F(FITSHeaderTest, ContinueKeyword) {
    // Create a header with a long string that requires CONTINUE
    FITSHeader header_with_continue;

    std::string long_string(150, 'A');  // 150 'A' characters
    header_with_continue.addKeyword("HISTORY", long_string);

    // Serialize and check for CONTINUE
    std::vector<char> data = header_with_continue.serialize();
    EXPECT_TRUE(containsPattern(data, "HISTORY "));
    EXPECT_TRUE(containsPattern(data, "CONTINUE"));
}

// Test COMMENT vs HISTORY keywords
TEST_F(FITSHeaderTest, CommentVsHistory) {
    header.addComment("This is a comment");
    header.addKeyword("HISTORY", "This is a history entry");

    // Serialize and check both are present
    std::vector<char> data = header.serialize();
    EXPECT_TRUE(containsPattern(data, "COMMENT This is a comment"));
    EXPECT_TRUE(containsPattern(data, "HISTORY This is a history entry"));

    // COMMENT should not appear in normal getAllKeywords list
    auto keywords = header.getAllKeywords();
    EXPECT_THAT(keywords, ::testing::Contains("HISTORY"));
}

// Test with empty values
TEST_F(FITSHeaderTest, EmptyValues) {
    header.addKeyword("EMPTY", "");
    EXPECT_EQ(header.getKeywordValue("EMPTY"), "");

    // Serialize and check
    std::vector<char> data = header.serialize();
    EXPECT_TRUE(containsPattern(data, "EMPTY   ="));
}

// Test round-trip with all kinds of values
TEST_F(FITSHeaderTest, RoundTripValues) {
    FITSHeader test_header;

    // Add various types of values
    test_header.addKeyword("BOOLEAN", "T");
    test_header.addKeyword("INTEGER", "42");
    test_header.addKeyword("FLOAT", "3.14159");
    test_header.addKeyword("STRING", "'Hello World'");
    test_header.addKeyword("DATE", "'2023-01-01T12:00:00'");
    test_header.addKeyword("EMPTY", "");
    test_header.addComment("Test comment");

    // Serialize and deserialize
    std::vector<char> data = test_header.serialize();
    FITSHeader deserialized;
    deserialized.deserialize(data);

    // Check all values survived round-trip
    EXPECT_EQ(deserialized.getKeywordValue("BOOLEAN"), "T");
    EXPECT_EQ(deserialized.getKeywordValue("INTEGER"), "42");
    EXPECT_EQ(deserialized.getKeywordValue("FLOAT"), "3.14159");
    EXPECT_EQ(deserialized.getKeywordValue("STRING"), "'Hello World'");
    EXPECT_EQ(deserialized.getKeywordValue("DATE"), "'2023-01-01T12:00:00'");
    EXPECT_EQ(deserialized.getKeywordValue("EMPTY"), "");
    EXPECT_THAT(deserialized.getComments(), ::testing::Contains("Test comment"));
}

// Test with multi-line serialization
TEST_F(FITSHeaderTest, MultilineComment) {
    header.addComment("Line 1\nLine 2\nLine 3");

    auto comments = header.getComments();
    EXPECT_EQ(comments.size(), 1);
    EXPECT_EQ(comments[0], "Line 1\nLine 2\nLine 3");

    // Serialize and check - should be flattened or split into multiple COMMENT lines
    std::vector<char> data = header.serialize();

    // Either approach is valid, just make sure the data is preserved
    FITSHeader deserialized;
    deserialized.deserialize(data);
    auto deserialized_comments = deserialized.getComments();

    std::string original = comments[0];
    std::string reconstructed;
    for (const auto& c : deserialized_comments) {
        if (!reconstructed.empty()) reconstructed += "\n";
        reconstructed += c;
    }

    // Check that content is preserved, even if format changes
    EXPECT_TRUE(reconstructed.find("Line 1") != std::string::npos);
    EXPECT_TRUE(reconstructed.find("Line 2") != std::string::npos);
    EXPECT_TRUE(reconstructed.find("Line 3") != std::string::npos);
}

} // namespace atom::image::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
