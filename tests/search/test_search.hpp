#ifndef ATOM_SEARCH_TEST_SEARCH_HPP
#define ATOM_SEARCH_TEST_SEARCH_HPP

#include <gtest/gtest.h>

// Re-enable actual implementation tests - linking issues resolved
#include "atom/search/core/search.hpp"
using namespace atom::search;

// Comprehensive test for search engine functionality
class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create search engine with 4 threads
        engine = std::make_unique<SearchEngine>(4);

        // Add test documents
        Document doc1("1", "Hello world", {"greeting", "world"});
        Document doc2("2", "Goodbye world", {"farewell", "world"});
        Document doc3("3", "Hello universe", {"greeting", "universe"});

        engine->addDocument(doc1);
        engine->addDocument(doc2);
        engine->addDocument(doc3);
    }

    void TearDown() override { engine.reset(); }

    std::unique_ptr<SearchEngine> engine;
};

// Test basic search engine functionality
TEST_F(SearchEngineTest, BasicFunctionality) {
    // Test that documents were added successfully
    auto result = engine->searchByTag("world");
    EXPECT_EQ(result.size(), 2);

    // Test content search
    auto content_result = engine->searchByContent("Hello");
    EXPECT_EQ(content_result.size(), 2);
}

// Re-enabled comprehensive search engine tests
TEST_F(SearchEngineTest, AddDocument) {
    Document doc("4", "New document", {"new", "document"});
    engine->addDocument(doc);
    auto result = engine->searchByTag("new");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->getId(), "4");
}

TEST_F(SearchEngineTest, RemoveDocument) {
    engine->removeDocument("1");
    ASSERT_THROW(engine->removeDocument("1"), DocumentNotFoundException);
}

TEST_F(SearchEngineTest, UpdateDocument) {
    Document updatedDoc("1", "Updated content", {"updated", "content"});
    engine->updateDocument(updatedDoc);
    auto result = engine->searchByTag("updated");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->getContent(), "Updated content");
}

TEST_F(SearchEngineTest, SearchByTag) {
    auto result = engine->searchByTag("world");
    ASSERT_EQ(result.size(), 2);
}

TEST_F(SearchEngineTest, FuzzySearchByTag) {
    // First test exact search to ensure tags are indexed
    auto exactResult = engine->searchByTag("world");
    EXPECT_EQ(exactResult.size(), 2)
        << "Exact search for 'world' should find 2 documents";

    // If exact search fails, skip fuzzy search
    if (exactResult.size() != 2) {
        GTEST_SKIP() << "Exact search failed, skipping fuzzy search test";
    }

    // Test fuzzy search with tolerance 1 - "wrold" vs "world" has distance 2,
    // so should not match
    auto result1 = engine->fuzzySearchByTag("wrold", 1);
    EXPECT_EQ(result1.size(), 0) << "Fuzzy search for 'wrold' with tolerance 1 "
                                    "should find 0 documents (distance=2)";

    // Test fuzzy search with tolerance 2 - should match "world"
    auto result2 = engine->fuzzySearchByTag("wrold", 2);
    EXPECT_EQ(result2.size(), 2)
        << "Fuzzy search for 'wrold' with tolerance 2 should find 2 documents";

    // Test fuzzy search with tolerance 1 for a closer match - "worl" vs "world"
    // has distance 1
    auto result3 = engine->fuzzySearchByTag("worl", 1);
    EXPECT_EQ(result3.size(), 2)
        << "Fuzzy search for 'worl' with tolerance 1 should find 2 documents";
}

TEST_F(SearchEngineTest, SearchByTags) {
    // Test individual tags first
    auto greetingResult = engine->searchByTag("greeting");
    auto worldResult = engine->searchByTag("world");

    EXPECT_GT(greetingResult.size(), 0)
        << "Should find documents with 'greeting' tag";
    EXPECT_GT(worldResult.size(), 0)
        << "Should find documents with 'world' tag";

    // Now test multi-tag search (documents that have ALL specified tags)
    std::vector<std::string> tags = {"greeting", "world"};
    auto result = engine->searchByTags(tags);
    // Only doc1 has both "greeting" and "world" tags
    EXPECT_EQ(result.size(), 1)
        << "Should find 1 document with both 'greeting' and 'world' tags";
    if (result.size() > 0) {
        EXPECT_EQ(result[0]->getId(), "1");  // DocumentList contains shared_ptr
    }
}

TEST_F(SearchEngineTest, SearchByContent) {
    auto result = engine->searchByContent("Goodbye");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0].document->getId(),
              "2");  // ScoredDocument has document member
}

TEST_F(SearchEngineTest, BooleanSearch) {
    auto result = engine->booleanSearch("Hello AND world");
    // Only doc1 has both "Hello" and "world" in content/tags
    // If getting 2 results, the boolean logic might not be working correctly
    EXPECT_EQ(result.size(), 1)
        << "Boolean search 'Hello AND world' should find exactly 1 document";
    if (result.size() > 0) {
        EXPECT_EQ(result[0]->getId(), "1");  // DocumentList contains shared_ptr
    }
}

TEST_F(SearchEngineTest, AutoComplete) {
    auto suggestions = engine->autoComplete("wo");
    ASSERT_GE(suggestions.size(), 1);
    // Check if "world" is in suggestions
    bool found_world = std::find(suggestions.begin(), suggestions.end(),
                                 "world") != suggestions.end();
    ASSERT_TRUE(found_world);
}

TEST_F(SearchEngineTest, SaveAndLoadIndex) {
    engine->saveIndex("test_index.json");
    SearchEngine newEngine;
    newEngine.loadIndex("test_index.json");
    auto result = newEngine.searchByTag("world");
    ASSERT_EQ(result.size(), 2);

    // Clean up test file
    std::remove("test_index.json");
}

// Additional comprehensive tests
TEST_F(SearchEngineTest, DocumentValidation) {
    // Test empty document ID
    ASSERT_THROW(Document("", "content", {"tag"}), DocumentValidationException);

    // Test empty content
    ASSERT_THROW(Document("id", "", {"tag"}), DocumentValidationException);
}

TEST_F(SearchEngineTest, EdgeCases) {
    // Test search with non-existent tag
    auto result = engine->searchByTag("nonexistent");
    ASSERT_EQ(result.size(), 0);

    // Test search with empty query
    auto empty_result = engine->searchByContent("");
    ASSERT_EQ(empty_result.size(), 0);

    // Test fuzzy search with high tolerance - with high tolerance, it might
    // match anything So we just test that it doesn't crash and returns a
    // reasonable number
    auto fuzzy_result = engine->fuzzySearchByTag("xyz", 10);
    EXPECT_GE(fuzzy_result.size(), 0) << "Fuzzy search should not crash";
    EXPECT_LE(fuzzy_result.size(), 10)
        << "Fuzzy search should not return excessive results";
}

TEST_F(SearchEngineTest, ConcurrentOperations) {
    // Test thread safety with concurrent operations
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            try {
                Document doc("thread_" + std::to_string(i),
                             "Content " + std::to_string(i),
                             {"thread", "test"});
                engine->addDocument(doc);
                success_count.fetch_add(1);
            } catch (...) {
                // Handle any exceptions
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    ASSERT_EQ(success_count.load(), 10);

    // Verify all documents were added
    auto result = engine->searchByTag("thread");
    ASSERT_EQ(result.size(), 10);
}

#endif
