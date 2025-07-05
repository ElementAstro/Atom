#pragma once
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "atom/extra/pugixml/xml_document.hpp"

namespace atom::extra::pugixml::test {

class XmlDocumentTest : public ::testing::Test {
protected:
    // Sample XML strings for testing
    const std::string simple_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<root>
    <child>Text content</child>
</root>)";

    const std::string complex_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<data version="1.0">
    <user id="1" active="true">
        <name>John Doe</name>
        <email>john@example.com</email>
        <roles>
            <role>admin</role>
            <role>editor</role>
        </roles>
    </user>
    <user id="2" active="false">
        <name>Jane Smith</name>
        <email>jane@example.com</email>
        <roles>
            <role>user</role>
        </roles>
    </user>
</data>)";

    // Temporary file for file I/O tests
    std::filesystem::path temp_file;

    void SetUp() override {
        // Create a temporary file path
        temp_file =
            std::filesystem::temp_directory_path() / "pugixml_test_temp.xml";
    }

    void TearDown() override {
        // Clean up temporary file
        std::error_code ec;
        std::filesystem::remove(temp_file, ec);
    }
};

// Test LoadOptions struct
TEST_F(XmlDocumentTest, LoadOptionsConfiguration) {
    // Test default options
    LoadOptions default_options;
    EXPECT_EQ(default_options.options, pugi::parse_default);
    EXPECT_EQ(default_options.encoding, pugi::encoding_auto);

    // Test minimal configuration
    LoadOptions minimal = LoadOptions().minimal();
    EXPECT_EQ(minimal.options, pugi::parse_minimal);

    // Test full configuration
    LoadOptions full = LoadOptions().full();
    EXPECT_EQ(full.options, pugi::parse_full);

    // Test modifiers
    LoadOptions custom = LoadOptions().full().no_escapes().trim_whitespace();

    // Verify no_escapes is applied (parse_escapes bit should be cleared)
    EXPECT_EQ(custom.options & pugi::parse_escapes, 0u);

    // Verify trim_whitespace is applied
    EXPECT_NE(custom.options & pugi::parse_trim_pcdata, 0u);

    // Test chained configuration
    LoadOptions chained;
    chained.set_parse_options(pugi::parse_minimal)
        .set_encoding(pugi::encoding_utf8);

    EXPECT_EQ(chained.options, pugi::parse_minimal);
    EXPECT_EQ(chained.encoding, pugi::encoding_utf8);
}

// Test SaveOptions struct
TEST_F(XmlDocumentTest, SaveOptionsConfiguration) {
    // Test default options
    SaveOptions default_options;
    EXPECT_STREQ(default_options.indent, "\t");
    EXPECT_EQ(default_options.flags, pugi::format_default);
    EXPECT_EQ(default_options.encoding, pugi::encoding_auto);

    // Test raw configuration
    SaveOptions raw = SaveOptions().raw();
    EXPECT_EQ(raw.flags, pugi::format_raw);

    // Test no_declaration configuration
    SaveOptions no_decl = SaveOptions().no_declaration();
    EXPECT_NE(no_decl.flags & pugi::format_no_declaration, 0u);

    // Test write_bom configuration
    SaveOptions with_bom = SaveOptions().write_bom();
    EXPECT_NE(with_bom.flags & pugi::format_write_bom, 0u);

    // Test chained configuration
    SaveOptions chained;
    chained.set_indent("  ")
        .set_flags(pugi::format_indent)
        .set_encoding(pugi::encoding_utf8);

    EXPECT_STREQ(chained.indent, "  ");
    EXPECT_EQ(chained.flags, pugi::format_indent);
    EXPECT_EQ(chained.encoding, pugi::encoding_utf8);

    // Test multi-option chaining
    SaveOptions multi = SaveOptions().raw().no_declaration().write_bom();

    EXPECT_EQ(multi.flags, pugi::format_raw | pugi::format_no_declaration |
                               pugi::format_write_bom);
}

// Test Document creation and basic operations
TEST_F(XmlDocumentTest, DocumentCreation) {
    // Test default constructor
    Document doc;
    EXPECT_TRUE(doc.empty());
    EXPECT_FALSE(doc.has_root());

    // Test create_empty factory method
    Document empty_doc = Document::create_empty("1.0", "UTF-8");
    EXPECT_FALSE(empty_doc.empty());     // Has declaration node
    EXPECT_FALSE(empty_doc.has_root());  // But no root element

    // Test explicit standalone parameter
    Document standalone_doc = Document::create_empty("1.0", "UTF-8", "yes");
    std::string xml_str = standalone_doc.to_string();
    EXPECT_THAT(xml_str, ::testing::HasSubstr("standalone=\"yes\""));
}

// Test Document from_string factory method
TEST_F(XmlDocumentTest, DocumentFromString) {
    // Test valid XML
    Document doc = Document::from_string(simple_xml);
    EXPECT_FALSE(doc.empty());
    EXPECT_TRUE(doc.has_root());

    // Verify root element
    Node root = doc.root();
    EXPECT_TRUE(root.valid());
    EXPECT_EQ(root.name(), "root");

    // Test with custom options
    LoadOptions options;
    options.trim_whitespace();
    Document doc_trimmed = Document::from_string(simple_xml, options);
    EXPECT_TRUE(doc_trimmed.has_root());

    // Test with invalid XML
    EXPECT_THROW(
        { Document::from_string("<root>incomplete"); }, ParseException);
}

// Test Document from_file factory method
TEST_F(XmlDocumentTest, DocumentFromFile) {
    // Create a test file
    {
        std::ofstream test_file(temp_file);
        test_file << simple_xml;
    }

    // Test loading from file
    Document doc = Document::from_file(temp_file);
    EXPECT_TRUE(doc.has_root());
    EXPECT_EQ(doc.root().name(), "root");

    // Test with custom options
    LoadOptions options;
    options.trim_whitespace();
    Document doc_trimmed = Document::from_file(temp_file, options);
    EXPECT_TRUE(doc_trimmed.has_root());

    // Test with non-existent file
    std::filesystem::path nonexistent = temp_file;
    nonexistent += ".nonexistent";
    EXPECT_THROW({ Document::from_file(nonexistent); }, ParseException);
}

// Test Document from_stream factory method
TEST_F(XmlDocumentTest, DocumentFromStream) {
    // Create a test stream
    std::stringstream ss(simple_xml);

    // Test loading from stream
    Document doc = Document::from_stream(ss);
    EXPECT_TRUE(doc.has_root());
    EXPECT_EQ(doc.root().name(), "root");

    // Test with custom options
    std::stringstream ss2(simple_xml);
    LoadOptions options;
    options.trim_whitespace();
    Document doc_trimmed = Document::from_stream(ss2, options);
    EXPECT_TRUE(doc_trimmed.has_root());

    // Test with invalid XML stream
    std::stringstream invalid("<root>incomplete");
    EXPECT_THROW({ Document::from_stream(invalid); }, ParseException);
}

// Test Document save operations
TEST_F(XmlDocumentTest, DocumentSave) {
    // Create a document
    Document doc = Document::from_string(simple_xml);

    // Test save_to_file
    doc.save_to_file(temp_file);
    EXPECT_TRUE(std::filesystem::exists(temp_file));

    // Verify file content
    {
        std::ifstream file(temp_file);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        EXPECT_THAT(content, ::testing::HasSubstr("<root>"));
        EXPECT_THAT(content,
                    ::testing::HasSubstr("<child>Text content</child>"));
    }

    // Test save_to_stream
    std::stringstream ss;
    doc.save_to_stream(ss);
    std::string stream_content = ss.str();
    EXPECT_THAT(stream_content, ::testing::HasSubstr("<root>"));
    EXPECT_THAT(stream_content,
                ::testing::HasSubstr("<child>Text content</child>"));

    // Test to_string
    std::string str_content = doc.to_string();
    EXPECT_THAT(str_content, ::testing::HasSubstr("<root>"));
    EXPECT_THAT(str_content,
                ::testing::HasSubstr("<child>Text content</child>"));

    // Test with custom save options
    SaveOptions options;
    options.set_indent("  ").no_declaration();
    std::string custom_str = doc.to_string(options);
    EXPECT_THAT(
        custom_str,
        ::testing::Not(::testing::HasSubstr("<?xml")));  // No declaration
}

// Test Document root manipulation
TEST_F(XmlDocumentTest, DocumentRootManipulation) {
    // Create an empty document
    Document doc = Document::create_empty();
    EXPECT_FALSE(doc.has_root());

    // Create root element
    Node root = doc.create_root("data");
    EXPECT_TRUE(doc.has_root());
    EXPECT_EQ(root.name(), "data");

    // Add children to root
    root.append_child("item").set_text("First item");
    root.append_child("item").set_text("Second item");

    // Verify structure
    auto items = doc.select_nodes("//item");
    EXPECT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].text(), "First item");
    EXPECT_EQ(items[1].text(), "Second item");

    // Clear the document
    doc.clear();
    EXPECT_TRUE(doc.empty());
    EXPECT_FALSE(doc.has_root());
}

// Test Document XPath operations
TEST_F(XmlDocumentTest, DocumentXPath) {
    // Create a document with complex structure
    Document doc = Document::from_string(complex_xml);

    // Test select_nodes
    auto users = doc.select_nodes("//user");
    EXPECT_EQ(users.size(), 2);

    // Test select_node for first match
    auto first_user = doc.select_node("//user");
    ASSERT_TRUE(first_user.has_value());
    EXPECT_EQ(first_user->attribute("id")->value(), "1");

    // Test select_node with predicate
    auto active_user = doc.select_node("//user[@active='true']");
    ASSERT_TRUE(active_user.has_value());
    EXPECT_EQ(active_user->attribute("id")->value(), "1");

    // Test select_node with no match
    auto nonexistent = doc.select_node("//nonexistent");
    EXPECT_FALSE(nonexistent.has_value());

    // Test complex XPath query
    auto admin_roles = doc.select_nodes("//user[.//role='admin']//name");
    EXPECT_EQ(admin_roles.size(), 1);
    EXPECT_EQ(admin_roles[0].text(), "John Doe");
}

// Test Document clone operation
TEST_F(XmlDocumentTest, DocumentClone) {
    // Create a document
    Document original = Document::from_string(simple_xml);

    // Clone the document
    Document cloned = original.clone();

    // Verify the cloned document has the same content
    EXPECT_TRUE(cloned.has_root());
    EXPECT_EQ(cloned.root().name(), "root");

    auto original_child = original.select_node("//child");
    auto cloned_child = cloned.select_node("//child");
    ASSERT_TRUE(original_child.has_value());
    ASSERT_TRUE(cloned_child.has_value());
    EXPECT_EQ(original_child->text(), cloned_child->text());

    // Modify the cloned document and verify the original is unchanged
    cloned.root().append_child("new_child");
    EXPECT_EQ(cloned.select_nodes("//child").size(), 2);  // Original + new
    EXPECT_EQ(original.select_nodes("//child").size(),
              1);  // Still just the original
}

// Test Document move semantics
TEST_F(XmlDocumentTest, DocumentMoveSemantics) {
    // Create a document
    Document doc1 = Document::from_string(simple_xml);

    // Move construct
    Document doc2(std::move(doc1));

    // Verify doc2 now has the content
    EXPECT_TRUE(doc2.has_root());
    EXPECT_EQ(doc2.root().name(), "root");

    // doc1 should be in a valid but unspecified state after move
    // (We don't test its state)

    // Create another document
    Document doc3 = Document::from_string(complex_xml);

    // Move assign
    doc2 = std::move(doc3);

    // Verify doc2 now has doc3's content
    auto users = doc2.select_nodes("//user");
    EXPECT_EQ(users.size(), 2);
}

// Test error cases
TEST_F(XmlDocumentTest, DocumentErrorCases) {
    Document doc = Document::create_empty();

    // Test invalid root creation
    EXPECT_THROW(
        {
            // Creating a second root element should fail
            doc.create_root("root");
            doc.create_root("another_root");
        },
        XmlException);

    // Test saving to invalid path
    EXPECT_THROW(
        { doc.save_to_file("/nonexistent/path/file.xml"); }, XmlException);

    // Test loading invalid XML
    EXPECT_THROW({ Document::from_string("<malformed>"); }, ParseException);
}

// Test node/document relationship
TEST_F(XmlDocumentTest, DocumentNodeRelationship) {
    Document doc = Document::from_string(complex_xml);

    // Test document() access
    Node doc_node = doc.document();
    EXPECT_TRUE(doc_node.valid());

    // document_element() should equal root()
    EXPECT_EQ(doc.document_element().name(), doc.root().name());

    // Verify document node children access
    auto children = doc_node.children();
    int count = 0;
    for ([[maybe_unused]] const auto& child : children) {
        count++;
    }
    EXPECT_GE(count, 2);  // At least declaration and root element

    // Test native access
    pugi::xml_document& native_doc = doc.native();
    EXPECT_FALSE(native_doc.empty());
    EXPECT_STREQ(native_doc.document_element().name(), "data");
}

// Test mixed load/save options
TEST_F(XmlDocumentTest, MixedLoadSaveOptions) {
    // Create test file with CDATA
    const std::string cdata_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<root>
    <![CDATA[<script>alert("Hello");</script>]]>
</root>)";

    {
        std::ofstream test_file(temp_file);
        test_file << cdata_xml;
    }

    // Load with different options and save with different options
    LoadOptions load_opts;
    load_opts.set_parse_options(pugi::parse_cdata | pugi::parse_declaration);

    SaveOptions save_opts;
    save_opts.raw().no_declaration();

    Document doc = Document::from_file(temp_file, load_opts);
    std::string result = doc.to_string(save_opts);

    // Should not have XML declaration
    EXPECT_THAT(result, ::testing::Not(::testing::HasSubstr("<?xml")));

    // Should have CDATA preserved
    EXPECT_THAT(result, ::testing::HasSubstr("<![CDATA["));
    EXPECT_THAT(result, ::testing::HasSubstr("</script>"));
}

}  // namespace atom::extra::pugixml::test
