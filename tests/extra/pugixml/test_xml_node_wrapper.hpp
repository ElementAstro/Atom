#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/extra/pugixml/xml_document.hpp"
#include "atom/extra/pugixml/xml_node_wrapper.hpp"

#include <string>

namespace atom::extra::pugixml::test {

class XmlNodeWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Basic XML for testing
        const char* xml_data = R"(
            <?xml version="1.0" encoding="UTF-8"?>
            <root attr1="value1" attr2="42" attr3="3.14">
                <child1>Text content</child1>
                <child2 id="1" active="true">
                    <grandchild>Nested content</grandchild>
                </child2>
                <child3 id="2" />
                <child3 id="3" />
                <empty />
                <numeric>42</numeric>
                <decimal>3.14159</decimal>
                <boolean>true</boolean>
            </root>
        )";

        // Parse the XML
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_string(xml_data);
        ASSERT_TRUE(result)
            << "Failed to parse test XML: " << result.description();

        // Create our wrapped nodes
        doc_ = Document::from_string(xml_data);
        root_ = Node(doc_.native().child("root"));
        ASSERT_TRUE(root_.valid()) << "Root node not found in test XML";
    }

    Document doc_;
    Node root_;
};

// Test Attribute class functionality
TEST_F(XmlNodeWrapperTest, AttributeBasicFunctionality) {
    auto attr1 = root_.attribute("attr1");
    ASSERT_TRUE(attr1.has_value());
    EXPECT_EQ(attr1->name(), "attr1");
    EXPECT_EQ(attr1->value(), "value1");
    EXPECT_FALSE(attr1->empty());
    EXPECT_TRUE(attr1->valid());

    // Test absent attribute
    auto missing_attr = root_.attribute("nonexistent");
    EXPECT_FALSE(missing_attr.has_value());
}

TEST_F(XmlNodeWrapperTest, AttributeTypeConversion) {
    auto attr_int = root_.attribute("attr2");
    ASSERT_TRUE(attr_int.has_value());

    // Test numeric conversion
    auto int_val = attr_int->as<int>();
    ASSERT_TRUE(int_val.has_value());
    EXPECT_EQ(*int_val, 42);

    // Test floating point conversion
    auto attr_float = root_.attribute("attr3");
    ASSERT_TRUE(attr_float.has_value());

    auto float_val = attr_float->as<double>();
    ASSERT_TRUE(float_val.has_value());
    EXPECT_DOUBLE_EQ(*float_val, 3.14);

    // Test boolean conversion (should fail for non-boolean strings)
    auto bool_val = attr_int->as<bool>();
    ASSERT_TRUE(bool_val.has_value());
    EXPECT_TRUE(*bool_val);  // Non-zero = true
}

TEST_F(XmlNodeWrapperTest, AttributeModification) {
    // Get the child2 node
    auto child2 = root_.child("child2");
    ASSERT_TRUE(child2.has_value());

    // Modify an existing attribute
    auto id_attr = child2->attribute("id");
    ASSERT_TRUE(id_attr.has_value());
    id_attr->set_value(99);

    // Verify the change
    auto updated_attr = child2->attribute("id");
    ASSERT_TRUE(updated_attr.has_value());
    auto value = updated_attr->as<int>();
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 99);
}

// Test Node basic properties
TEST_F(XmlNodeWrapperTest, NodeBasicProperties) {
    EXPECT_EQ(root_.name(), "root");
    EXPECT_TRUE(root_.valid());
    EXPECT_FALSE(root_.empty());

    auto child1 = root_.child("child1");
    ASSERT_TRUE(child1.has_value());
    EXPECT_EQ(child1->text(), "Text content");

    auto empty = root_.child("empty");
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->text().empty());
}

TEST_F(XmlNodeWrapperTest, NodeTextConversion) {
    // Test integer text value
    auto numeric = root_.child("numeric");
    ASSERT_TRUE(numeric.has_value());

    auto num_val = numeric->text_as<int>();
    ASSERT_TRUE(num_val.has_value());
    EXPECT_EQ(*num_val, 42);

    // Test floating point text value
    auto decimal = root_.child("decimal");
    ASSERT_TRUE(decimal.has_value());

    auto dec_val = decimal->text_as<double>();
    ASSERT_TRUE(dec_val.has_value());
    EXPECT_DOUBLE_EQ(*dec_val, 3.14159);

    // Test boolean text value
    auto boolean = root_.child("boolean");
    ASSERT_TRUE(boolean.has_value());

    auto bool_val = boolean->text_as<bool>();
    ASSERT_TRUE(bool_val.has_value());
    EXPECT_TRUE(*bool_val);
}

TEST_F(XmlNodeWrapperTest, NodeTextModification) {
    auto child1 = root_.child("child1");
    ASSERT_TRUE(child1.has_value());

    // Modify text content
    child1->set_text("Modified text");
    EXPECT_EQ(child1->text(), "Modified text");

    // Set numeric text
    child1->set_text(123);
    EXPECT_EQ(child1->text(), "123");

    // Set floating point text
    child1->set_text(45.67);
    EXPECT_EQ(child1->text(), "45.67");
}

// Test navigation methods
TEST_F(XmlNodeWrapperTest, NodeNavigation) {
    auto child2 = root_.child("child2");
    ASSERT_TRUE(child2.has_value());

    // Test parent navigation
    auto parent = child2->parent();
    ASSERT_TRUE(parent.has_value());
    EXPECT_EQ(parent->name(), "root");

    // Test child navigation
    auto grandchild = child2->child("grandchild");
    ASSERT_TRUE(grandchild.has_value());
    EXPECT_EQ(grandchild->text(), "Nested content");

    // Test sibling navigation
    auto child3 = child2->next_sibling();
    ASSERT_TRUE(child3.has_value());
    EXPECT_EQ(child3->name(), "child3");

    auto child1 = child2->previous_sibling();
    ASSERT_TRUE(child1.has_value());
    EXPECT_EQ(child1->name(), "child1");
}

TEST_F(XmlNodeWrapperTest, FirstAndLastChild) {
    // Test first_child
    auto first = root_.first_child();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->name(), "child1");

    // Test last_child
    auto last = root_.last_child();
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->name(), "boolean");
}

// Test node creation and modification
TEST_F(XmlNodeWrapperTest, NodeCreation) {
    // Create a new child
    Node new_child = root_.append_child("new_child");
    EXPECT_TRUE(new_child.valid());
    EXPECT_EQ(new_child.name(), "new_child");

    // Set text and attribute
    new_child.set_text("New content");
    new_child.set_attribute("id", 100);

    // Verify
    EXPECT_EQ(new_child.text(), "New content");
    auto attr = new_child.attribute("id");
    ASSERT_TRUE(attr.has_value());
    auto id_val = attr->as<int>();
    ASSERT_TRUE(id_val.has_value());
    EXPECT_EQ(*id_val, 100);
}

TEST_F(XmlNodeWrapperTest, NodePrependsAndRemoval) {
    // Prepend a child
    Node prepended = root_.prepend_child("first_child");
    EXPECT_TRUE(prepended.valid());

    // Verify it's first
    auto first = root_.first_child();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->name(), "first_child");

    // Remove the child
    bool removed = root_.remove_child("first_child");
    EXPECT_TRUE(removed);

    // Verify it's gone
    first = root_.first_child();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->name(), "child1");  // Back to original first
}

TEST_F(XmlNodeWrapperTest, AttributeRemoval) {
    bool removed = root_.remove_attribute("attr1");
    EXPECT_TRUE(removed);

    auto attr1 = root_.attribute("attr1");
    EXPECT_FALSE(attr1.has_value());
}

// Test iterator and ranges
TEST_F(XmlNodeWrapperTest, NodeIteration) {
    // Count children using range-based for
    int count = 0;
    for ([[maybe_unused]] const auto& child : root_.children()) {
        count++;
    }
    EXPECT_EQ(count, 8);  // all children in the test XML

    // Collect child names
    std::vector<std::string> names;
    for (const auto& child : root_.children()) {
        names.push_back(std::string(child.name()));
    }

    // Verify expected children
    ASSERT_EQ(names.size(), 8);
    EXPECT_EQ(names[0], "child1");
    EXPECT_EQ(names[1], "child2");
    EXPECT_EQ(names[2], "child3");
    EXPECT_EQ(names[3], "child3");
}

TEST_F(XmlNodeWrapperTest, AttributeIteration) {
    // Count attributes using range-based for
    int count = 0;
    for ([[maybe_unused]] const auto& attr : root_.attributes()) {
        count++;
    }
    EXPECT_EQ(count, 3);  // attr1, attr2, attr3

    // Collect attribute names and values
    std::vector<std::pair<std::string, std::string>> attrs;
    for (const auto& attr : root_.attributes()) {
        attrs.emplace_back(std::string(attr.name()), std::string(attr.value()));
    }

    // Verify expected attributes
    ASSERT_EQ(attrs.size(), 3);
    EXPECT_EQ(attrs[0].first, "attr1");
    EXPECT_EQ(attrs[0].second, "value1");
    EXPECT_EQ(attrs[1].first, "attr2");
    EXPECT_EQ(attrs[1].second, "42");
}

// Test XPath functionality
TEST_F(XmlNodeWrapperTest, XPathSelectNodes) {
    // Select all child3 nodes
    auto nodes = root_.select_nodes("child3");
    EXPECT_EQ(nodes.size(), 2);

    for (const auto& node : nodes) {
        EXPECT_EQ(node.name(), "child3");
    }

    // Select nodes with attribute id
    auto nodes_with_id = root_.select_nodes("//*[@id]");
    EXPECT_EQ(nodes_with_id.size(), 3);  // child2 and two child3s
}

TEST_F(XmlNodeWrapperTest, XPathSelectSingleNode) {
    // Select first child with id="2"
    auto node = root_.select_node("//*[@id='2']");
    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->name(), "child3");

    // Non-existent node
    auto missing = root_.select_node("//nonexistent");
    EXPECT_FALSE(missing.has_value());
}

// Test functional programming features
TEST_F(XmlNodeWrapperTest, FilterChildren) {
    // Filter to get only child3 nodes
    auto child3_nodes = root_.filter_children([](const Node& node) {
        return node.name() == std::string_view("child3");
    });

    EXPECT_EQ(child3_nodes.size(), 2);
    for (const auto& node : child3_nodes) {
        EXPECT_EQ(node.name(), "child3");
    }
}

TEST_F(XmlNodeWrapperTest, TransformChildren) {
    // Transform to get child names
    auto names = root_.transform_children(
        [](const Node& node) { return std::string(node.name()); });

    ASSERT_EQ(names.size(), 8);
    EXPECT_EQ(names[0], "child1");
    EXPECT_EQ(names[1], "child2");
}

// Test structured binding support
TEST_F(XmlNodeWrapperTest, StructuredBindings) {
    // Get first three children using structured bindings
    auto [first, second, third] = root_.get_children<3>();

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->name(), "child1");

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->name(), "child2");

    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(third->name(), "child3");
}

// Test error handling and exceptions
TEST_F(XmlNodeWrapperTest, ExceptionHandling) {
    // Create an XML document with invalid syntax
    pugi::xml_document invalid_doc;
    pugi::xml_parse_result result = invalid_doc.load_string("<root>incomplete");
    EXPECT_FALSE(result);

    // Test exception handling for XML parsing errors
    try {
        if (!result) {
            throw ParseException(result.description());
        }
        FAIL() << "Expected ParseException was not thrown";
    } catch (const ParseException& e) {
        std::string message = e.what();
        EXPECT_TRUE(message.find("Parse error") != std::string::npos);
    }
}

TEST_F(XmlNodeWrapperTest, NodeCreationErrors) {
    // Create a document node that's read-only
    pugi::xml_document doc;
    auto doc_node = doc.append_child(pugi::node_document);
    Node read_only_node(doc_node);

    // Try to append a child to a read-only node (should throw)
    try {
        read_only_node.append_child("impossible");
        FAIL() << "Expected XmlException was not thrown";
    } catch (const XmlException& e) {
        std::string message = e.what();
        EXPECT_TRUE(message.find("Failed to append child") !=
                    std::string::npos);
    }
}

// Test hash functionality
TEST_F(XmlNodeWrapperTest, NodeHashing) {
    auto child1 = root_.child("child1");
    auto same_child1 = root_.child("child1");
    auto child2 = root_.child("child2");

    ASSERT_TRUE(child1.has_value());
    ASSERT_TRUE(same_child1.has_value());
    ASSERT_TRUE(child2.has_value());

    // Same nodes should have same hash
    EXPECT_EQ(child1->hash(), same_child1->hash());

    // Different nodes should have different hashes
    EXPECT_NE(child1->hash(), child2->hash());

    // Test std::hash specialization
    std::hash<Node> hasher;
    EXPECT_EQ(hasher(*child1), child1->hash());
}

// Test compile-time strings
TEST_F(XmlNodeWrapperTest, CompileTimeStrings) {
    static constexpr CompileTimeString str("test");
    constexpr std::string_view view = str.view();

    EXPECT_EQ(view, "test");
    EXPECT_EQ(view.size(), 4);
}

}  // namespace atom::extra::pugixml::test
