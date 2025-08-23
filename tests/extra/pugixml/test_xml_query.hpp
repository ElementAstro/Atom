#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/extra/pugixml/xml_document.hpp"
#include "atom/extra/pugixml/xml_query.hpp"

#include <algorithm>
#include <functional>
#include <string>

namespace atom::extra::pugixml::test {

class XmlQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a rich test XML structure for query tests
        const char* xml_data = R"(
            <?xml version="1.0" encoding="UTF-8"?>
            <catalog>
                <book id="bk101" category="fiction">
                    <author>Gambardella, Matthew</author>
                    <title>XML Developer's Guide</title>
                    <genre>Computer</genre>
                    <price>44.95</price>
                    <publish_date>2000-10-01</publish_date>
                    <description>An in-depth look at creating applications with XML.</description>
                </book>
                <book id="bk102" category="fiction">
                    <author>Ralls, Kim</author>
                    <title>Midnight Rain</title>
                    <genre>Fantasy</genre>
                    <price>5.95</price>
                    <publish_date>2000-12-16</publish_date>
                    <description>A former architect battles corporate zombies.</description>
                </book>
                <book id="bk103" category="non-fiction">
                    <author>Corets, Eva</author>
                    <title>Maeve Ascendant</title>
                    <genre>Fantasy</genre>
                    <price>5.95</price>
                    <publish_date>2000-11-17</publish_date>
                </book>
                <book id="bk104" category="non-fiction">
                    <author>Corets, Eva</author>
                    <title>Oberon's Legacy</title>
                    <genre>Fantasy</genre>
                    <price>5.95</price>
                    <publish_date>2001-03-10</publish_date>
                    <description>In post-apocalypse England, the mysterious agent Oberon helps to create a new life for the inhabitants.</description>
                </book>
                <book id="bk105" category="fiction">
                    <author>Tolkien, J.R.R.</author>
                    <title>The Lord of the Rings</title>
                    <genre>Fantasy</genre>
                    <price>29.99</price>
                    <publish_date>1954-07-29</publish_date>
                    <description>Epic high fantasy novel.</description>
                </book>
                <magazine id="mg101">
                    <title>PC Magazine</title>
                    <issue>January 2022</issue>
                    <price>4.99</price>
                </magazine>
                <magazine id="mg102">
                    <title>National Geographic</title>
                    <issue>February 2022</issue>
                    <price>6.99</price>
                </magazine>
                <empty_element />
            </catalog>
        )";

        // Parse the XML
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_string(xml_data);
        ASSERT_TRUE(result)
            << "Failed to parse test XML: " << result.description();

        // Create our wrapped document and catalog root
        doc_ = Document::from_string(xml_data);
        catalog_ = doc_.root();
        ASSERT_TRUE(catalog_.valid()) << "Catalog node not found in test XML";
    }

    Document doc_;
    Node catalog_;
};

// Test query::filter function
TEST_F(XmlQueryTest, Filter) {
    // Filter only book elements
    auto books = query::filter(catalog_, query::predicates::has_name("book"));

    // Convert to vector for easier testing
    std::vector<Node> book_nodes;
    for (const auto& book : books) {
        book_nodes.push_back(book);
    }

    EXPECT_EQ(book_nodes.size(), 5);

    // Verify all are books
    for (const auto& book : book_nodes) {
        EXPECT_EQ(book.name(), "book");
    }

    // Filter fiction books
    auto fiction_books = query::filter(catalog_, [](const Node& node) {
        return node.name() == "book" &&
               node.attribute("category").has_value() &&
               node.attribute("category")->value() == "fiction";
    });

    // Count fiction books
    size_t fiction_count = 0;
    for ([[maybe_unused]] const auto& book : fiction_books) {
        fiction_count++;
    }

    EXPECT_EQ(fiction_count, 3);
}

// Test query::transform function
TEST_F(XmlQueryTest, Transform) {
    // Transform books to their titles
    auto titles = query::transform(catalog_, [](const Node& node) {
        if (node.name() == "book") {
            auto title_node = node.child("title");
            if (title_node) {
                return std::string(title_node->text());
            }
        }
        return std::string("");
    });

    // Collect titles
    std::vector<std::string> title_list;
    for (const auto& title : titles) {
        if (!title.empty()) {
            title_list.push_back(title);
        }
    }

    ASSERT_EQ(title_list.size(), 5);
    EXPECT_EQ(title_list[0], "XML Developer's Guide");
    EXPECT_EQ(title_list[4], "The Lord of the Rings");
}

// Test query::find_first function
TEST_F(XmlQueryTest, FindFirst) {
    // Find first book by Eva Corets
    auto eva_book = query::find_first(catalog_, [](const Node& node) {
        if (node.name() == "book") {
            auto author_node = node.child("author");
            return author_node && author_node->text() == "Corets, Eva";
        }
        return false;
    });

    ASSERT_TRUE(eva_book.has_value());

    // Verify it's the first Eva Corets book (bk103)
    auto id_attr = eva_book->attribute("id");
    ASSERT_TRUE(id_attr.has_value());
    EXPECT_EQ(id_attr->value(), "bk103");

    // Find non-existent node
    auto nonexistent = query::find_first(catalog_, [](const Node& node) {
        return node.name() == "nonexistent";
    });

    EXPECT_FALSE(nonexistent.has_value());
}

// Test query::find_all_recursive function
TEST_F(XmlQueryTest, FindAllRecursive) {
    // Find all title elements recursively
    auto all_titles = query::find_all_recursive(
        catalog_, [](const Node& node) { return node.name() == "title"; });

    // Should find 7 titles: 5 books + 2 magazines
    EXPECT_EQ(all_titles.size(), 7);

    // Test for specific titles
    std::vector<std::string> title_texts;
    for (const auto& title : all_titles) {
        title_texts.push_back(std::string(title.text()));
    }

    EXPECT_TRUE(std::find(title_texts.begin(), title_texts.end(),
                          "XML Developer's Guide") != title_texts.end());
    EXPECT_TRUE(std::find(title_texts.begin(), title_texts.end(),
                          "PC Magazine") != title_texts.end());

    // Find all elements with price > 10
    auto expensive_items =
        query::find_all_recursive(catalog_, [](const Node& node) {
            if (node.name() == "price") {
                auto price = node.text_as<double>();
                return price.has_value() && *price > 10.0;
            }
            return false;
        });

    EXPECT_EQ(expensive_items.size(), 2);  // XML Developer's Guide and LOTR
}

// Test query::count_if function
TEST_F(XmlQueryTest, CountIf) {
    // Count books
    size_t book_count =
        query::count_if(catalog_, query::predicates::has_name("book"));
    EXPECT_EQ(book_count, 5);

    // Count magazines
    size_t magazine_count =
        query::count_if(catalog_, query::predicates::has_name("magazine"));
    EXPECT_EQ(magazine_count, 2);

    // Count elements with description
    size_t with_description = query::count_if(catalog_, [](const Node& node) {
        return node.child("description").has_value();
    });

    EXPECT_EQ(with_description, 4);  // 4 books have descriptions
}

// Test query::accumulate function
TEST_F(XmlQueryTest, Accumulate) {
    // Sum all book prices
    double total_price =
        query::accumulate(catalog_, 0.0, std::plus<>(), [](const Node& node) {
            if (node.name() == "book") {
                auto price_node = node.child("price");
                if (price_node) {
                    auto price = price_node->text_as<double>();
                    if (price) {
                        return *price;
                    }
                }
            }
            return 0.0;
        });

    // Total should be 44.95 + 5.95 + 5.95 + 5.95 + 29.99 = 92.79
    EXPECT_NEAR(total_price, 92.79, 0.001);

    // Count total child elements across all books
    int total_elements =
        query::accumulate(catalog_, 0, std::plus<>(), [](const Node& node) {
            if (node.name() == "book") {
                int count = 0;
                for ([[maybe_unused]] const auto& child : node.children()) {
                    count++;
                }
                return count;
            }
            return 0;
        });

    // Books have different numbers of children (with/without description)
    EXPECT_GT(total_elements, 25);  // Exact count depends on XML structure
}

// Test query::any_of function
TEST_F(XmlQueryTest, AnyOf) {
    // Check if any book is in Computer genre
    bool has_computer = query::any_of(catalog_, [](const Node& node) {
        if (node.name() == "book") {
            auto genre = node.child("genre");
            return genre && genre->text() == "Computer";
        }
        return false;
    });

    EXPECT_TRUE(has_computer);

    // Check for non-existent genre
    bool has_horror = query::any_of(catalog_, [](const Node& node) {
        if (node.name() == "book") {
            auto genre = node.child("genre");
            return genre && genre->text() == "Horror";
        }
        return false;
    });

    EXPECT_FALSE(has_horror);
}

// Test query::all_of function
TEST_F(XmlQueryTest, AllOf) {
    // Check if all books have a title
    bool all_have_title = query::all_of(catalog_, [](const Node& node) {
        return node.name() != "book" || node.child("title").has_value();
    });

    EXPECT_TRUE(all_have_title);

    // Check if all books have a description (they don't)
    bool all_have_description = query::all_of(catalog_, [](const Node& node) {
        return node.name() != "book" || node.child("description").has_value();
    });

    EXPECT_FALSE(all_have_description);
}

// Test query::predicates namespace
TEST_F(XmlQueryTest, Predicates) {
    using namespace query::predicates;

    // Test has_name predicate
    auto is_book = has_name("book");
    auto first_child = catalog_.first_child();
    ASSERT_TRUE(first_child.has_value());
    EXPECT_TRUE(is_book(*first_child));

    // Test has_attribute predicate
    auto has_id = has_attribute("id");
    EXPECT_TRUE(has_id(*first_child));

    // Test has_attribute_value predicate
    auto is_fiction = has_attribute_value("category", "fiction");
    EXPECT_TRUE(is_fiction(*first_child));

    // Test has_text predicate
    auto first_title = first_child->child("title");
    ASSERT_TRUE(first_title.has_value());
    EXPECT_TRUE(has_text()(*first_title));

    // Test has_text_value predicate
    auto is_xml_guide = has_text_value("XML Developer's Guide");
    EXPECT_TRUE(is_xml_guide(*first_title));

    // Test is_element predicate
    EXPECT_TRUE(is_element()(*first_child));

    // Test has_children predicate
    EXPECT_TRUE(has_children()(*first_child));

    // Test composition of predicates
    auto fiction_with_id = [&](const Node& node) {
        return is_book(node) && has_id(node) && is_fiction(node);
    };

    EXPECT_TRUE(fiction_with_id(*first_child));
}

// Test transform::transform_matching function
TEST_F(XmlQueryTest, TransformMatching) {
    // Create a copy of the document for modification tests
    Document doc_copy = doc_.clone();
    Node catalog_copy = doc_copy.root();

    // Transform all fiction books to add a "bestseller" attribute
    transform::transform_matching(
        catalog_copy,
        query::predicates::has_attribute_value("category", "fiction"),
        [](Node& node) { node.set_attribute("bestseller", "true"); });

    // Count books with bestseller attribute
    int bestseller_count = 0;
    for (auto book : catalog_copy.children()) {
        if (book.name() == "book" && book.attribute("bestseller").has_value()) {
            bestseller_count++;
        }
    }

    EXPECT_EQ(bestseller_count, 3);  // Three fiction books

    // Verify non-fiction books were not modified
    for (auto book : catalog_copy.children()) {
        if (book.name() == "book" && book.attribute("category").has_value() &&
            book.attribute("category")->value() == "non-fiction") {
            EXPECT_FALSE(book.attribute("bestseller").has_value());
        }
    }
}

// Test transform::transform_recursive function
TEST_F(XmlQueryTest, TransformRecursive) {
    // Create a copy of the document for modification tests
    Document doc_copy = doc_.clone();
    Node catalog_copy = doc_copy.root();

    // Add a "processed" attribute to all nodes recursively
    transform::transform_recursive(catalog_copy, [](Node& node) {
        if (node.type() == pugi::node_element) {
            node.set_attribute("processed", "true");
        }
    });

    // Count all processed elements (recursive)
    std::function<int(const Node&)> count_processed = [&](const Node& node) {
        int count = 0;

        // Count this node if processed
        if (node.attribute("processed").has_value()) {
            count++;
        }

        // Count children recursively
        for (auto child : node.children()) {
            count += count_processed(child);
        }

        return count;
    };

    int processed_count = count_processed(catalog_copy);

    // Every element node should be processed
    // Count all element nodes in the XML
    std::function<int(const Node&)> count_elements = [&](const Node& node) {
        int count = 0;
        if (node.type() == pugi::node_element) {
            count = 1;
        }

        for (auto child : node.children()) {
            count += count_elements(child);
        }

        return count;
    };

    int element_count = count_elements(catalog_);

    EXPECT_EQ(processed_count, element_count);
}

// Test transform::sort_children function
TEST_F(XmlQueryTest, SortChildren) {
    // Create a copy of the document for modification tests
    Document doc_copy = doc_.clone();
    Node catalog_copy = doc_copy.root();

    // Sort books by price (highest first)
    transform::sort_children(catalog_copy, [](const Node& a, const Node& b) {
        if (a.name() == "book" && b.name() == "book") {
            auto price_a = a.child("price");
            auto price_b = b.child("price");

            if (price_a && price_b) {
                auto val_a = price_a->text_as<double>().value_or(0.0);
                auto val_b = price_b->text_as<double>().value_or(0.0);
                return val_a > val_b;
            }
        }
        return false;
    });

    // Check if sorting worked by examining the order
    std::vector<std::string> expected_ids = {"bk101", "bk105", "bk102", "bk103",
                                             "bk104"};
    std::vector<std::string> actual_ids;

    for (auto node : catalog_copy.children()) {
        if (node.name() == "book") {
            auto id = node.attribute("id");
            if (id) {
                actual_ids.push_back(std::string(id->value()));
            }
        }
    }

    // Only check the first few to account for undefined sort stability
    ASSERT_GE(actual_ids.size(), 2);
    EXPECT_EQ(actual_ids[0], "bk101");  // Most expensive (44.95)
    EXPECT_EQ(actual_ids[1], "bk105");  // Second most expensive (29.99)

    // Check that the magazine nodes still exist after sorting
    int magazine_count = 0;
    for (auto node : catalog_copy.children()) {
        if (node.name() == "magazine") {
            magazine_count++;
        }
    }

    EXPECT_EQ(magazine_count, 2);
}

// Test combined query and transform operations
TEST_F(XmlQueryTest, CombinedOperations) {
    // Create a copy of the document for modification tests
    Document doc_copy = doc_.clone();
    Node catalog_copy = doc_copy.root();

    // Find all Fantasy books
    auto fantasy_books =
        query::find_all_recursive(catalog_copy, [](const Node& node) {
            if (node.name() == "book") {
                auto genre = node.child("genre");
                return genre && genre->text() == "Fantasy";
            }
            return false;
        });

    EXPECT_EQ(fantasy_books.size(), 4);

    // Transform these books
    for (auto& book : fantasy_books) {
        book.set_attribute("fantasy_verified", "true");
    }

    // Count books with fantasy_verified attribute
    int verified_count = query::count_if(
        catalog_copy, query::predicates::has_attribute("fantasy_verified"));

    EXPECT_EQ(verified_count, 4);

    // Find most expensive fantasy book
    auto expensive_fantasy =
        query::find_first(catalog_copy, [](const Node& node) {
            if (node.name() == "book") {
                auto genre = node.child("genre");
                auto price = node.child("price");

                if (genre && genre->text() == "Fantasy" && price) {
                    auto price_val = price->text_as<double>();
                    return price_val && *price_val > 20.0;
                }
            }
            return false;
        });

    ASSERT_TRUE(expensive_fantasy.has_value());

    // Should be Lord of the Rings
    auto title = expensive_fantasy->child("title");
    ASSERT_TRUE(title.has_value());
    EXPECT_EQ(title->text(), "The Lord of the Rings");
}

}  // namespace atom::extra::pugixml::test
