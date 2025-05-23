#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/extra/pugixml/xml_builder.hpp"
#include "atom/extra/pugixml/xml_document.hpp"

#include <string>
#include <vector>

namespace atom::extra::pugixml::test {

class XmlBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a document for testing NodeBuilder operations
        doc_ = Document::create_empty();
        root_node_ = doc_.create_root("test_root");
        ASSERT_TRUE(root_node_.valid()) << "Failed to create test root node";
    }

    Document doc_;
    Node root_node_;
};

// Test AttributePair construction
TEST_F(XmlBuilderTest, AttributePairConstruction) {
    // Test string construction
    AttributePair str_attr("name", "value");
    EXPECT_EQ(str_attr.name, "name");
    EXPECT_EQ(str_attr.value, "value");

    // Test string_view construction
    std::string_view name_view = "view_name";
    std::string_view value_view = "view_value";
    AttributePair view_attr(name_view, value_view);
    EXPECT_EQ(view_attr.name, "view_name");
    EXPECT_EQ(view_attr.value, "view_value");

    // Test numeric construction
    AttributePair int_attr("int_attr", 42);
    EXPECT_EQ(int_attr.name, "int_attr");
    EXPECT_EQ(int_attr.value, "42");

    AttributePair float_attr("float_attr", 3.14);
    EXPECT_EQ(float_attr.name, "float_attr");
    EXPECT_EQ(float_attr.value, "3.14");

    // Test attr helper function
    auto helper_attr = attr("helper", "value");
    EXPECT_EQ(helper_attr.name, "helper");
    EXPECT_EQ(helper_attr.value, "value");

    auto numeric_helper_attr = attr("numeric", 99);
    EXPECT_EQ(numeric_helper_attr.name, "numeric");
    EXPECT_EQ(numeric_helper_attr.value, "99");
}

// Test NodeBuilder attribute setting
TEST_F(XmlBuilderTest, NodeBuilderAttributes) {
    NodeBuilder builder(root_node_);

    // Test single attribute
    builder.attribute("single", "value");

    // Test multiple attributes at once
    builder.attributes(attr("attr1", "value1"), attr("attr2", 42),
                       attr("attr3", 3.14));

    // Verify all attributes
    auto single_attr = root_node_.attribute("single");
    ASSERT_TRUE(single_attr.has_value());
    EXPECT_EQ(single_attr->value(), "value");

    auto attr1 = root_node_.attribute("attr1");
    ASSERT_TRUE(attr1.has_value());
    EXPECT_EQ(attr1->value(), "value1");

    auto attr2 = root_node_.attribute("attr2");
    ASSERT_TRUE(attr2.has_value());
    EXPECT_EQ(attr2->value(), "42");

    auto attr3 = root_node_.attribute("attr3");
    ASSERT_TRUE(attr3.has_value());
    EXPECT_EQ(attr3->value(), "3.14");
}

// Test NodeBuilder text content
TEST_F(XmlBuilderTest, NodeBuilderText) {
    NodeBuilder builder(root_node_);

    // Set string text
    builder.text("Simple text");
    EXPECT_EQ(root_node_.text(), "Simple text");

    // Set integer text
    builder.text(42);
    EXPECT_EQ(root_node_.text(), "42");

    // Set floating point text
    builder.text(3.14159);
    EXPECT_EQ(root_node_.text(), "3.14159");
}

// Test NodeBuilder child with configurator
TEST_F(XmlBuilderTest, NodeBuilderChildWithConfigurator) {
    NodeBuilder builder(root_node_);

    // Add a child with configurator
    builder.child("child1", [](NodeBuilder& child) {
        child.attribute("id", 1).text("Child content");
    });

    // Verify the child
    auto child1 = root_node_.child("child1");
    ASSERT_TRUE(child1.has_value());

    auto id_attr = child1->attribute("id");
    ASSERT_TRUE(id_attr.has_value());
    EXPECT_EQ(id_attr->value(), "1");

    EXPECT_EQ(child1->text(), "Child content");

    // Test nested children
    builder.child("parent", [](NodeBuilder& parent) {
        parent.attribute("level", 1).child("child", [](NodeBuilder& child) {
            child.attribute("level", 2).text("Nested content");
        });
    });

    // Verify nested structure
    auto parent = root_node_.child("parent");
    ASSERT_TRUE(parent.has_value());
    EXPECT_EQ(parent->attribute("level")->value(), "1");

    auto nested_child = parent->child("child");
    ASSERT_TRUE(nested_child.has_value());
    EXPECT_EQ(nested_child->attribute("level")->value(), "2");
    EXPECT_EQ(nested_child->text(), "Nested content");
}

// Test NodeBuilder simple child with text
TEST_F(XmlBuilderTest, NodeBuilderSimpleChild) {
    NodeBuilder builder(root_node_);

    // Add a simple text child
    builder.child("simple", "Simple text");

    // Add a numeric child
    builder.child("numeric", 42);

    // Add a floating point child
    builder.child("float", 3.14);

    // Verify children
    auto simple_child = root_node_.child("simple");
    ASSERT_TRUE(simple_child.has_value());
    EXPECT_EQ(simple_child->text(), "Simple text");

    auto numeric_child = root_node_.child("numeric");
    ASSERT_TRUE(numeric_child.has_value());
    EXPECT_EQ(numeric_child->text(), "42");

    auto float_child = root_node_.child("float");
    ASSERT_TRUE(float_child.has_value());
    EXPECT_EQ(float_child->text(), "3.14");
}

// Test NodeBuilder's children method with containers
TEST_F(XmlBuilderTest, NodeBuilderChildren) {
    NodeBuilder builder(root_node_);

    // Create a test container
    struct Item {
        std::string name;
        int value;
    };

    std::vector<Item> items = {{"first", 1}, {"second", 2}, {"third", 3}};

    // Add children from container
    builder.children("item", items, [](NodeBuilder& child, const Item& item) {
        child.attribute("name", item.name).attribute("value", item.value);
    });

    // Verify children
    std::vector<Node> child_nodes;
    for (auto node : root_node_.children()) {
        if (node.name() == "item") {
            child_nodes.push_back(node);
        }
    }

    ASSERT_EQ(child_nodes.size(), 3);

    // Verify first item
    EXPECT_EQ(child_nodes[0].attribute("name")->value(), "first");
    EXPECT_EQ(child_nodes[0].attribute("value")->value(), "1");

    // Verify second item
    EXPECT_EQ(child_nodes[1].attribute("name")->value(), "second");
    EXPECT_EQ(child_nodes[1].attribute("value")->value(), "2");

    // Verify third item
    EXPECT_EQ(child_nodes[2].attribute("name")->value(), "third");
    EXPECT_EQ(child_nodes[2].attribute("value")->value(), "3");
}

// Test NodeBuilder if_condition
TEST_F(XmlBuilderTest, NodeBuilderIfCondition) {
    NodeBuilder builder(root_node_);

    // Test true condition
    builder.if_condition(true, [](NodeBuilder& node) {
        node.attribute("condition_true", "yes");
    });

    // Test false condition
    builder.if_condition(false, [](NodeBuilder& node) {
        node.attribute("condition_false", "should_not_exist");
    });

    // Verify results
    auto true_attr = root_node_.attribute("condition_true");
    ASSERT_TRUE(true_attr.has_value());
    EXPECT_EQ(true_attr->value(), "yes");

    auto false_attr = root_node_.attribute("condition_false");
    EXPECT_FALSE(false_attr.has_value());
}

// Test NodeBuilder build/get methods
TEST_F(XmlBuilderTest, NodeBuilderBuildGet) {
    NodeBuilder builder(root_node_);

    // Add some content
    builder.attribute("test", "value").child("test_child", "content");

    // Test build method
    Node built = builder.build();
    EXPECT_EQ(built.name(), "test_root");
    EXPECT_EQ(built.attribute("test")->value(), "value");

    // Test get method
    Node got = builder.get();
    EXPECT_EQ(got.name(), "test_root");
    EXPECT_EQ(got.attribute("test")->value(), "value");

    // Test implicit conversion
    Node implicit = builder;
    EXPECT_EQ(implicit.name(), "test_root");
    EXPECT_EQ(implicit.attribute("test")->value(), "value");
}

// Test DocumentBuilder
TEST_F(XmlBuilderTest, DocumentBuilder) {
    // Create document with declaration
    DocumentBuilder builder;
    builder.declaration("1.1", "UTF-8", "yes");

    // Create root with configurator
    builder.root("root", [](NodeBuilder& root) {
        root.attribute("version", "1.0")
            .child("first", "First child")
            .child("second", [](NodeBuilder& second) {
                second.attribute("id", 2).child("nested", "Nested content");
            });
    });

    // Build the document
    Document doc = builder.build();

    // Verify declaration
    std::string xml_string = doc.to_string();
    EXPECT_THAT(
        xml_string,
        ::testing::HasSubstr(
            "<?xml version=\"1.1\" encoding=\"UTF-8\" standalone=\"yes\"?>"));

    // Verify root and children
    Node root = doc.root();
    EXPECT_EQ(root.name(), "root");
    EXPECT_EQ(root.attribute("version")->value(), "1.0");

    auto first = root.child("first");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->text(), "First child");

    auto second = root.child("second");
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->attribute("id")->value(), "2");

    auto nested = second->child("nested");
    ASSERT_TRUE(nested.has_value());
    EXPECT_EQ(nested->text(), "Nested content");
}

// Test DocumentBuilder with simple root
TEST_F(XmlBuilderTest, DocumentBuilderSimpleRoot) {
    // Create document with simple text root
    DocumentBuilder builder;
    builder.declaration().root("simple_root", "Root text content");

    // Build the document
    Document doc = builder.build();

    // Verify root
    Node root = doc.root();
    EXPECT_EQ(root.name(), "simple_root");
    EXPECT_EQ(root.text(), "Root text content");
}

// Test factory functions
TEST_F(XmlBuilderTest, FactoryFunctions) {
    // Test document() factory
    auto doc_builder = document();
    doc_builder.declaration().root("test", "content");

    Document doc = doc_builder.build();
    EXPECT_EQ(doc.root().name(), "test");

    // Test element() factory
    auto elem_builder = element(root_node_);
    elem_builder.attribute("factory", "test").child("factory_child", "content");

    EXPECT_EQ(root_node_.attribute("factory")->value(), "test");
    EXPECT_EQ(root_node_.child("factory_child")->text(), "content");
}

// Test user-defined literals
TEST_F(XmlBuilderTest, UserDefinedLiterals) {
    using namespace literals;

    auto xml_str = "test"_xml;
    EXPECT_EQ(xml_str, "test");
}

// Test complex XML building
TEST_F(XmlBuilderTest, ComplexXmlBuilding) {
    // Create a complex XML document using the fluent API
    auto doc =
        document()
            .declaration("1.0", "UTF-8")
            .root(
                "catalog",
                [](NodeBuilder& catalog) {
                    // Add books
                    catalog
                        .child("book",
                               [](NodeBuilder& book) {
                                   book.attributes(attr("id", "bk101"),
                                                   attr("category", "Fiction"))
                                       .child("title", "The Catcher in the Rye")
                                       .child("author", "J.D. Salinger")
                                       .child("price", 9.99)
                                       .child("publish_date", "1951-07-16");
                               })
                        .child("book",
                               [](NodeBuilder& book) {
                                   book.attributes(
                                           attr("id", "bk102"),
                                           attr("category", "Science Fiction"))
                                       .child("title", "Dune")
                                       .child("author", "Frank Herbert")
                                       .child("price", 12.99)
                                       .child("publish_date", "1965-08-01");
                               })
                        // Add magazine section
                        .child("magazines", [](NodeBuilder& magazines) {
                            magazines.child(
                                "magazine", [](NodeBuilder& magazine) {
                                    magazine.attribute("id", "mg101")
                                        .child("title", "National Geographic")
                                        .child("issue", "January 2022")
                                        .child("price", 5.99);
                                });
                        });
                })
            .build();

    // Verify the structure
    Node root = doc.root();
    EXPECT_EQ(root.name(), "catalog");

    // Check first book
    auto first_book = root.child("book");
    ASSERT_TRUE(first_book.has_value());
    EXPECT_EQ(first_book->attribute("id")->value(), "bk101");
    EXPECT_EQ(first_book->child("title")->text(), "The Catcher in the Rye");

    // Check second book using XPath
    auto second_book = doc.select_node("/catalog/book[@id='bk102']");
    ASSERT_TRUE(second_book.has_value());
    EXPECT_EQ(second_book->child("title")->text(), "Dune");
    EXPECT_EQ(second_book->child("price")->text(), "12.99");

    // Check magazine
    auto magazine = doc.select_node("//magazine");
    ASSERT_TRUE(magazine.has_value());
    EXPECT_EQ(magazine->child("title")->text(), "National Geographic");
}

// Test building XML with conditional elements
TEST_F(XmlBuilderTest, ConditionalElements) {
    bool include_optional = true;
    bool include_alternative = false;

    auto doc =
        document()
            .declaration()
            .root("configuration",
                  [&](NodeBuilder& config) {
                      config.child("required", "Always present")
                          .if_condition(include_optional,
                                        [](NodeBuilder& node) {
                                            node.child(
                                                "optional",
                                                "Conditionally included");
                                        })
                          .if_condition(include_alternative,
                                        [](NodeBuilder& node) {
                                            node.child("alternative",
                                                       "Should not be present");
                                        });
                  })
            .build();

    Node root = doc.root();

    // Required element should always be present
    auto required = root.child("required");
    ASSERT_TRUE(required.has_value());
    EXPECT_EQ(required->text(), "Always present");

    // Optional element should be present
    auto optional = root.child("optional");
    ASSERT_TRUE(optional.has_value());
    EXPECT_EQ(optional->text(), "Conditionally included");

    // Alternative element should not be present
    auto alternative = root.child("alternative");
    EXPECT_FALSE(alternative.has_value());
}

// Test building XML with container data
TEST_F(XmlBuilderTest, ContainerData) {
    // Data to build XML from
    struct Product {
        int id;
        std::string name;
        double price;
        bool in_stock;
    };

    std::vector<Product> products = {{1, "Laptop", 999.99, true},
                                     {2, "Smartphone", 499.99, true},
                                     {3, "Headphones", 149.99, false}};

    auto doc =
        document()
            .declaration()
            .root("products",
                  [&](NodeBuilder& root) {
                      root.children(
                          "product", products,
                          [](NodeBuilder& product, const Product& item) {
                              product.attribute("id", item.id)
                                  .attribute("in_stock",
                                             item.in_stock ? "yes" : "no")
                                  .child("name", item.name)
                                  .child("price", item.price);
                          });
                  })
            .build();

    // Verify structure
    auto product_nodes = doc.select_nodes("//product");
    ASSERT_EQ(product_nodes.size(), 3);

    // Check first product
    EXPECT_EQ(product_nodes[0].attribute("id")->value(), "1");
    EXPECT_EQ(product_nodes[0].attribute("in_stock")->value(), "yes");
    EXPECT_EQ(product_nodes[0].child("name")->text(), "Laptop");
    EXPECT_EQ(product_nodes[0].child("price")->text(), "999.99");

    // Check third product
    EXPECT_EQ(product_nodes[2].attribute("id")->value(), "3");
    EXPECT_EQ(product_nodes[2].attribute("in_stock")->value(), "no");
    EXPECT_EQ(product_nodes[2].child("name")->text(), "Headphones");
    EXPECT_EQ(product_nodes[2].child("price")->text(), "149.99");
}

// Test chaining multiple operations
TEST_F(XmlBuilderTest, ChainedOperations) {
    NodeBuilder builder(root_node_);

    // Chain multiple operations
    builder.attribute("test", "value")
        .text("Root text")
        .child("first", "First child")
        .child("second", [](NodeBuilder& second) { second.attribute("id", 2); })
        .attribute("another", "attr");

    // Verify all operations were applied
    EXPECT_EQ(root_node_.attribute("test")->value(), "value");
    EXPECT_EQ(root_node_.attribute("another")->value(), "attr");
    EXPECT_EQ(root_node_.text(), "Root text");

    auto first = root_node_.child("first");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->text(), "First child");

    auto second = root_node_.child("second");
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->attribute("id")->value(), "2");
}

}  // namespace atom::extra::pugixml::test