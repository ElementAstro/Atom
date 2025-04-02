// filepath: /home/max/Atom-1/atom/utils/test_xml.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "xml.hpp"

using namespace atom::utils;
using ::testing::HasSubstr;

class XMLReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test XML file
        const char* testXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<root>"
            "  <config id=\"main-config\" version=\"1.0\">"
            "    <server host=\"localhost\" port=\"8080\">Production "
            "Server</server>"
            "    <database>"
            "      <connection type=\"mysql\">localhost:3306</connection>"
            "      <credentials username=\"admin\" password=\"secret\"/>"
            "    </database>"
            "    <logging level=\"info\" enabled=\"true\"/>"
            "  </config>"
            "  <users>"
            "    <user id=\"1\" role=\"admin\">"
            "      <name>John Doe</name>"
            "      <email>john@example.com</email>"
            "      <preferences>"
            "        <theme>dark</theme>"
            "        <notifications>true</notifications>"
            "      </preferences>"
            "    </user>"
            "    <user id=\"2\" role=\"user\">"
            "      <name>Jane Smith</name>"
            "      <email>jane@example.com</email>"
            "      <preferences>"
            "        <theme>light</theme>"
            "        <notifications>false</notifications>"
            "      </preferences>"
            "    </user>"
            "  </users>"
            "</root>";

        testFilePath = "test_xml_file.xml";
        std::ofstream out(testFilePath);
        out << testXml;
        out.close();

        emptyFilePath = "empty_xml_file.xml";
        std::ofstream empty(emptyFilePath);
        empty << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root/>";
        empty.close();

        invalidFilePath = "invalid_xml_file.xml";
        std::ofstream invalid(invalidFilePath);
        invalid << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root>";
        invalid.close();
    }

    void TearDown() override {
        // Clean up the test files
        std::remove(testFilePath.c_str());
        std::remove(emptyFilePath.c_str());
        std::remove(invalidFilePath.c_str());
    }

    std::string testFilePath;
    std::string emptyFilePath;
    std::string invalidFilePath;
};

// Test constructor with valid file
TEST_F(XMLReaderTest, ConstructorWithValidFile) {
    EXPECT_NO_THROW(XMLReader reader(testFilePath));
}

// Test constructor with empty file
TEST_F(XMLReaderTest, ConstructorWithEmptyFile) {
    EXPECT_NO_THROW(XMLReader reader(emptyFilePath));
}

// Test constructor with invalid file
TEST_F(XMLReaderTest, ConstructorWithInvalidFile) {
    EXPECT_THROW(XMLReader reader(invalidFilePath), std::runtime_error);
}

// Test constructor with non-existent file
TEST_F(XMLReaderTest, ConstructorWithNonExistentFile) {
    EXPECT_THROW(XMLReader reader("non_existent_file.xml"), std::runtime_error);
}

// Test getChildElementNames with valid parent
TEST_F(XMLReaderTest, GetChildElementNamesWithValidParent) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementNames("config");

    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(result));
    auto names = std::get<std::vector<std::string>>(result);

    EXPECT_EQ(names.size(), 3);
    EXPECT_THAT(names, ::testing::Contains("server"));
    EXPECT_THAT(names, ::testing::Contains("database"));
    EXPECT_THAT(names, ::testing::Contains("logging"));
}

// Test getChildElementNames with invalid parent
TEST_F(XMLReaderTest, GetChildElementNamesWithInvalidParent) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementNames("non_existent");

    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getElementText with valid element
TEST_F(XMLReaderTest, GetElementTextWithValidElement) {
    XMLReader reader(testFilePath);
    auto result = reader.getElementText("server");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "Production Server");
}

// Test getElementText with invalid element
TEST_F(XMLReaderTest, GetElementTextWithInvalidElement) {
    XMLReader reader(testFilePath);
    auto result = reader.getElementText("non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getElementText with element having no text
TEST_F(XMLReaderTest, GetElementTextWithNoTextElement) {
    XMLReader reader(testFilePath);
    auto result = reader.getElementText("credentials");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("no text"));
}

// Test getAttributeValue with valid element and attribute
TEST_F(XMLReaderTest, GetAttributeValueWithValidElementAndAttribute) {
    XMLReader reader(testFilePath);
    auto result = reader.getAttributeValue("server", "host");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "localhost");
}

// Test getAttributeValue with valid element but invalid attribute
TEST_F(XMLReaderTest, GetAttributeValueWithValidElementButInvalidAttribute) {
    XMLReader reader(testFilePath);
    auto result = reader.getAttributeValue("server", "non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getAttributeValue with invalid element
TEST_F(XMLReaderTest, GetAttributeValueWithInvalidElement) {
    XMLReader reader(testFilePath);
    auto result = reader.getAttributeValue("non_existent", "host");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getRootElementNames
TEST_F(XMLReaderTest, GetRootElementNames) {
    XMLReader reader(testFilePath);
    auto names = reader.getRootElementNames();

    EXPECT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "root");
}

// Test hasChildElement with valid parent and child
TEST_F(XMLReaderTest, HasChildElementWithValidParentAndChild) {
    XMLReader reader(testFilePath);
    EXPECT_TRUE(reader.hasChildElement("config", "server"));
}

// Test hasChildElement with valid parent but invalid child
TEST_F(XMLReaderTest, HasChildElementWithValidParentButInvalidChild) {
    XMLReader reader(testFilePath);
    EXPECT_FALSE(reader.hasChildElement("config", "non_existent"));
}

// Test hasChildElement with invalid parent
TEST_F(XMLReaderTest, HasChildElementWithInvalidParent) {
    XMLReader reader(testFilePath);
    EXPECT_FALSE(reader.hasChildElement("non_existent", "server"));
}

// Test getChildElementText with valid parent and child
TEST_F(XMLReaderTest, GetChildElementTextWithValidParentAndChild) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementText("user", "name");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "John Doe");
}

// Test getChildElementText with valid parent but invalid child
TEST_F(XMLReaderTest, GetChildElementTextWithValidParentButInvalidChild) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementText("user", "non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementText with invalid parent
TEST_F(XMLReaderTest, GetChildElementTextWithInvalidParent) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementText("non_existent", "name");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementAttributeValue with valid parameters
TEST_F(XMLReaderTest, GetChildElementAttributeValueWithValidParameters) {
    XMLReader reader(testFilePath);
    auto result =
        reader.getChildElementAttributeValue("database", "connection", "type");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "mysql");
}

// Test getChildElementAttributeValue with valid parent and child but invalid
// attribute
TEST_F(
    XMLReaderTest,
    GetChildElementAttributeValueWithValidParentAndChildButInvalidAttribute) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementAttributeValue("database", "connection",
                                                       "non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementAttributeValue with valid parent but invalid child
TEST_F(XMLReaderTest,
       GetChildElementAttributeValueWithValidParentButInvalidChild) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementAttributeValue("database",
                                                       "non_existent", "type");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementAttributeValue with invalid parent
TEST_F(XMLReaderTest, GetChildElementAttributeValueWithInvalidParent) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementAttributeValue("non_existent",
                                                       "connection", "type");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getValueByPath with valid path
TEST_F(XMLReaderTest, GetValueByPathWithValidPath) {
    XMLReader reader(testFilePath);
    auto result = reader.getValueByPath("root/users/user/name");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "John Doe");
}

// Test getValueByPath with invalid path
TEST_F(XMLReaderTest, GetValueByPathWithInvalidPath) {
    XMLReader reader(testFilePath);
    auto result = reader.getValueByPath("root/non_existent/user/name");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getValueByPath with malformed path
TEST_F(XMLReaderTest, GetValueByPathWithMalformedPath) {
    XMLReader reader(testFilePath);
    auto result = reader.getValueByPath("root//user/name");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("Invalid path"));
}

// Test getAttributeValueByPath with valid path and attribute
TEST_F(XMLReaderTest, GetAttributeValueByPathWithValidPathAndAttribute) {
    XMLReader reader(testFilePath);
    auto result = reader.getAttributeValueByPath("root/users/user", "id");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "1");
}

// Test getAttributeValueByPath with valid path but invalid attribute
TEST_F(XMLReaderTest, GetAttributeValueByPathWithValidPathButInvalidAttribute) {
    XMLReader reader(testFilePath);
    auto result =
        reader.getAttributeValueByPath("root/users/user", "non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getAttributeValueByPath with invalid path
TEST_F(XMLReaderTest, GetAttributeValueByPathWithInvalidPath) {
    XMLReader reader(testFilePath);
    auto result =
        reader.getAttributeValueByPath("root/non_existent/user", "id");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test hasChildElementByPath with valid path and child
TEST_F(XMLReaderTest, HasChildElementByPathWithValidPathAndChild) {
    XMLReader reader(testFilePath);
    EXPECT_TRUE(reader.hasChildElementByPath("root/users/user", "name"));
}

// Test hasChildElementByPath with valid path but invalid child
TEST_F(XMLReaderTest, HasChildElementByPathWithValidPathButInvalidChild) {
    XMLReader reader(testFilePath);
    EXPECT_FALSE(
        reader.hasChildElementByPath("root/users/user", "non_existent"));
}

// Test hasChildElementByPath with invalid path
TEST_F(XMLReaderTest, HasChildElementByPathWithInvalidPath) {
    XMLReader reader(testFilePath);
    EXPECT_FALSE(
        reader.hasChildElementByPath("root/non_existent/user", "name"));
}

// Test getChildElementTextByPath with valid path and child
TEST_F(XMLReaderTest, GetChildElementTextByPathWithValidPathAndChild) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementTextByPath("root/users/user", "email");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "john@example.com");
}

// Test getChildElementTextByPath with valid path but invalid child
TEST_F(XMLReaderTest, GetChildElementTextByPathWithValidPathButInvalidChild) {
    XMLReader reader(testFilePath);
    auto result =
        reader.getChildElementTextByPath("root/users/user", "non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementTextByPath with invalid path
TEST_F(XMLReaderTest, GetChildElementTextByPathWithInvalidPath) {
    XMLReader reader(testFilePath);
    auto result =
        reader.getChildElementTextByPath("root/non_existent/user", "email");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementAttributeValueByPath with valid parameters
TEST_F(XMLReaderTest, GetChildElementAttributeValueByPathWithValidParameters) {
    XMLReader reader(testFilePath);
    auto result =
        reader.getChildElementAttributeValueByPath("root/users", "user", "id");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), "1");
}

// Test getChildElementAttributeValueByPath with valid path and child but
// invalid attribute
TEST_F(
    XMLReaderTest,
    GetChildElementAttributeValueByPathWithValidPathAndChildButInvalidAttribute) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementAttributeValueByPath(
        "root/users", "user", "non_existent");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementAttributeValueByPath with valid path but invalid child
TEST_F(XMLReaderTest,
       GetChildElementAttributeValueByPathWithValidPathButInvalidChild) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementAttributeValueByPath(
        "root/users", "non_existent", "id");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test getChildElementAttributeValueByPath with invalid path
TEST_F(XMLReaderTest, GetChildElementAttributeValueByPathWithInvalidPath) {
    XMLReader reader(testFilePath);
    auto result = reader.getChildElementAttributeValueByPath(
        "root/non_existent", "user", "id");

    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("not found"));
}

// Test saveToFile functionality
TEST_F(XMLReaderTest, SaveToFile) {
    XMLReader reader(testFilePath);
    std::string newFilePath = "new_test_xml_file.xml";

    auto result = reader.saveToFile(newFilePath);
    ASSERT_TRUE(std::holds_alternative<bool>(result));
    EXPECT_TRUE(std::get<bool>(result));

    // Check that the file exists
    std::ifstream file(newFilePath);
    EXPECT_TRUE(file.good());
    file.close();

    // Clean up
    std::remove(newFilePath.c_str());
}

// Test saveToFile with invalid path
TEST_F(XMLReaderTest, SaveToFileWithInvalidPath) {
    XMLReader reader(testFilePath);
    std::string newFilePath = "/invalid/path/test_xml_file.xml";

    auto result = reader.saveToFile(newFilePath);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_THAT(std::get<std::string>(result), HasSubstr("Failed to save"));
}

// Test getValuesByPathsAsync functionality
TEST_F(XMLReaderTest, GetValuesByPathsAsync) {
    XMLReader reader(testFilePath);
    std::vector<std::string> paths = {
        "root/config/server", "root/users/user/name", "root/users/user/email",
        "root/non_existent"};

    auto future = reader.getValuesByPathsAsync(paths);
    auto results = future.get();

    EXPECT_EQ(results.size(), 4);

    ASSERT_TRUE(std::holds_alternative<std::string>(results[0]));
    EXPECT_EQ(std::get<std::string>(results[0]), "Production Server");

    ASSERT_TRUE(std::holds_alternative<std::string>(results[1]));
    EXPECT_EQ(std::get<std::string>(results[1]), "John Doe");

    ASSERT_TRUE(std::holds_alternative<std::string>(results[2]));
    EXPECT_EQ(std::get<std::string>(results[2]), "john@example.com");

    ASSERT_TRUE(std::holds_alternative<std::string>(results[3]));
    EXPECT_THAT(std::get<std::string>(results[3]), HasSubstr("not found"));
}

// Test thread safety of XMLReader methods
TEST_F(XMLReaderTest, ThreadSafety) {
    XMLReader reader(testFilePath);

    std::vector<std::thread> threads;
    const int numThreads = 10;
    std::vector<Result<std::string>> results(numThreads);

    // Start multiple threads that access the XMLReader concurrently
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&reader, i, &results]() {
            if (i % 3 == 0) {
                results[i] = reader.getValueByPath("root/config/server");
            } else if (i % 3 == 1) {
                results[i] =
                    reader.getAttributeValueByPath("root/users/user", "id");
            } else {
                results[i] =
                    reader.getChildElementTextByPath("root/users/user", "name");
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify results
    for (int i = 0; i < numThreads; ++i) {
        ASSERT_TRUE(std::holds_alternative<std::string>(results[i]));

        if (i % 3 == 0) {
            EXPECT_EQ(std::get<std::string>(results[i]), "Production Server");
        } else if (i % 3 == 1) {
            EXPECT_EQ(std::get<std::string>(results[i]), "1");
        } else {
            EXPECT_EQ(std::get<std::string>(results[i]), "John Doe");
        }
    }
}

// Test XMLReader with various XML features
TEST_F(XMLReaderTest, ComplexXML) {
    // Create a more complex XML file
    const char* complexXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<complex>"
        "  <!-- This is a comment -->"
        "  <element with-attr=\"value\" empty-attr=\"\">"
        "    <![CDATA[This is CDATA content with <tags> & special chars]]>"
        "  </element>"
        "  <empty/>"
        "  <nested><deep><deeper>Nested content</deeper></deep></nested>"
        "  <with-entities>&lt;tag&gt; with &quot;entities&quot; &amp; special "
        "chars</with-entities>"
        "</complex>";

    std::string complexFilePath = "complex_xml_file.xml";
    std::ofstream out(complexFilePath);
    out << complexXml;
    out.close();

    XMLReader reader(complexFilePath);

    // Test CDATA content
    auto cdataResult = reader.getElementText("element");
    ASSERT_TRUE(std::holds_alternative<std::string>(cdataResult));
    EXPECT_THAT(std::get<std::string>(cdataResult),
                HasSubstr("This is CDATA content"));
    EXPECT_THAT(std::get<std::string>(cdataResult), HasSubstr("<tags>"));

    // Test empty elements
    EXPECT_TRUE(reader.hasChildElement("complex", "empty"));

    // Test deeply nested elements
    auto nestedResult = reader.getValueByPath("complex/nested/deep/deeper");
    ASSERT_TRUE(std::holds_alternative<std::string>(nestedResult));
    EXPECT_EQ(std::get<std::string>(nestedResult), "Nested content");

    // Test entity decoding
    auto entitiesResult = reader.getElementText("with-entities");
    ASSERT_TRUE(std::holds_alternative<std::string>(entitiesResult));
    EXPECT_THAT(std::get<std::string>(entitiesResult), HasSubstr("<tag>"));
    EXPECT_THAT(std::get<std::string>(entitiesResult),
                HasSubstr("\"entities\""));
    EXPECT_THAT(std::get<std::string>(entitiesResult),
                HasSubstr("& special chars"));

    // Clean up
    std::remove(complexFilePath.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}