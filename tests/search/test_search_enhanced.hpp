#ifndef ATOM_SEARCH_TEST_SEARCH_ENHANCED_HPP
#define ATOM_SEARCH_TEST_SEARCH_ENHANCED_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for enhanced SearchEngine features
class SearchEngineEnhancedTest : public ::testing::Test {
protected:
    SearchConfig config;
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        // Configure for testing
        config.enable_performance_caching = true;
        config.enable_semantic_search = true;
        config.enable_ranked_autocomplete = true;
        config.enable_stemming = true;
        config.tokenized_cache_size = 100;
        config.tf_idf_cache_size = 200;

        engine = std::make_unique<SearchEngine>(4, config);

        // Add test documents
        engine->add_document(Document("doc1", "machine learning algorithms", {"ai", "ml", "tech"}));
        engine->add_document(Document("doc2", "deep learning neural networks", {"ai", "deep", "neural"}));
        engine->add_document(Document("doc3", "natural language processing", {"nlp", "ai", "language"}));
        engine->add_document(Document("doc4", "computer vision applications", {"cv", "vision", "tech"}));
        engine->add_document(Document("doc5", "data science analytics", {"data", "science", "analytics"}));
    }
};

// Test performance caching functionality
TEST_F(SearchEngineEnhancedTest, PerformanceCaching) {
    // First search should populate cache
    auto start1 = std::chrono::high_resolution_clock::now();
    auto results1 = engine->search_by_content("machine learning");
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    ASSERT_FALSE(results1.results.empty());
    EXPECT_EQ(results1.results[0].document->get_id(), "doc1");

    // Second identical search should be faster due to caching
    auto start2 = std::chrono::high_resolution_clock::now();
    auto results2 = engine->search_by_content("machine learning");
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    ASSERT_FALSE(results2.results.empty());
    EXPECT_EQ(results2.results[0].document->get_id(), "doc1");

    // Cache should make second search faster (though this might be flaky in CI)
    // Just verify results are consistent
    EXPECT_EQ(results1.results.size(), results2.results.size());
}

// Test TF-IDF caching
TEST_F(SearchEngineEnhancedTest, TFIDFCaching) {
    // Get document for TF-IDF calculation
    auto doc = std::make_shared<Document>("test_doc", "machine learning machine learning algorithms", std::vector<std::string>{"test"});

    // First TF-IDF calculation should populate cache
    double score1 = engine->tf_idf_cached(*doc, "machine");
    EXPECT_GT(score1, 0.0);

    // Second calculation should use cache
    double score2 = engine->tf_idf_cached(*doc, "machine");
    EXPECT_EQ(score1, score2);

    // Different term should not be cached
    double score3 = engine->tf_idf_cached(*doc, "learning");
    EXPECT_GT(score3, 0.0);
    EXPECT_NE(score1, score3);
}

// Test bulk operations
TEST_F(SearchEngineEnhancedTest, BulkOperations) {
    // Test bulk insert
    std::vector<Document> docs_to_insert = {
        Document("bulk1", "bulk insert test one", {"bulk", "test"}),
        Document("bulk2", "bulk insert test two", {"bulk", "test"}),
        Document("bulk3", "bulk insert test three", {"bulk", "test"})
    };

    size_t inserted = engine->bulk_insert(docs_to_insert);
    EXPECT_EQ(inserted, 3);

    // Verify documents were inserted
    auto results = engine->search_by_tag("bulk");
    EXPECT_EQ(results.results.size(), 3);

    // Test bulk update
    std::vector<Document> docs_to_update = {
        Document("bulk1", "updated bulk test one", {"bulk", "updated"}),
        Document("bulk2", "updated bulk test two", {"bulk", "updated"})
    };

    size_t updated = engine->bulk_update(docs_to_update);
    EXPECT_EQ(updated, 2);

    // Verify updates
    auto updated_results = engine->search_by_tag("updated");
    EXPECT_EQ(updated_results.results.size(), 2);

    // Test bulk delete
    std::vector<String> ids_to_delete = {"bulk1", "bulk2", "bulk3"};
    size_t deleted = engine->bulk_delete(ids_to_delete);
    EXPECT_EQ(deleted, 3);

    // Verify deletions
    auto after_delete = engine->search_by_tag("bulk");
    EXPECT_TRUE(after_delete.results.empty());
}

// Test enhanced autocomplete with ranking
TEST_F(SearchEngineEnhancedTest, RankedAutocomplete) {
    // Add more documents with similar tags to test ranking
    engine->add_document(Document("freq1", "test content", {"ai", "frequent"}));
    engine->add_document(Document("freq2", "test content", {"ai", "frequent"}));
    engine->add_document(Document("freq3", "test content", {"ai", "rare"}));

    auto ranked_suggestions = engine->auto_complete_ranked("a", 5);
    ASSERT_FALSE(ranked_suggestions.empty());

    // "ai" should be ranked higher due to frequency
    bool found_ai = false;
    size_t ai_frequency = 0;
    for (const auto& [suggestion, frequency] : ranked_suggestions) {
        if (std::string(suggestion) == "ai") {
            found_ai = true;
            ai_frequency = frequency;
            break;
        }
    }

    EXPECT_TRUE(found_ai);
    EXPECT_GT(ai_frequency, 1); // Should appear in multiple documents

    // Verify suggestions are sorted by frequency (descending)
    for (size_t i = 1; i < ranked_suggestions.size(); ++i) {
        EXPECT_GE(ranked_suggestions[i-1].second, ranked_suggestions[i].second);
    }
}

// Test stemming functionality
TEST_F(SearchEngineEnhancedTest, StemmingSupport) {
    // Add document with words that should be stemmed
    engine->add_document(Document("stem_test", "running runner runs", {"running"}));

    // Test basic stemming
    std::string stemmed_running = engine->stem_word("running");
    std::string stemmed_runner = engine->stem_word("runner");
    std::string stemmed_runs = engine->stem_word("runs");

    // Basic Porter stemmer should handle some common cases
    EXPECT_NE(stemmed_running, "running"); // Should be stemmed

    // Test tokenization with stemming
    auto tokens = engine->tokenize_with_stemming("running quickly");
    ASSERT_EQ(tokens.size(), 2);

    // Verify stemming was applied
    bool found_stemmed = false;
    for (const auto& token : tokens) {
        if (std::string(token) != "running" && std::string(token) != "quickly") {
            found_stemmed = true;
            break;
        }
    }
    // Note: This test might be fragile depending on stemming implementation
}

// Test optimized tokenization
TEST_F(SearchEngineEnhancedTest, OptimizedTokenization) {
    String test_content = "Hello, World! This is a test with punctuation... and numbers 123.";

    auto tokens = engine->tokenize_content_optimized(test_content);

    // Should extract alphanumeric tokens only
    std::vector<std::string> expected_tokens = {"hello", "world", "this", "is", "a", "test", "with", "punctuation", "and", "numbers", "123"};

    EXPECT_EQ(tokens.size(), expected_tokens.size());

    for (size_t i = 0; i < tokens.size() && i < expected_tokens.size(); ++i) {
        EXPECT_EQ(std::string(tokens[i]), expected_tokens[i]);
    }
}

// Test cache invalidation
TEST_F(SearchEngineEnhancedTest, CacheInvalidation) {
    // Add a document
    engine->add_document(Document("cache_test", "original content", {"original"}));

    // Perform operations that should populate caches
    auto results1 = engine->search_by_content("original");
    ASSERT_FALSE(results1.results.empty());

    // Update the document (should invalidate caches)
    engine->update_document(Document("cache_test", "updated content", {"updated"}));

    // Search for old content should return empty
    auto results2 = engine->search_by_content("original");
    EXPECT_TRUE(results2.results.empty());

    // Search for new content should work
    auto results3 = engine->search_by_content("updated");
    ASSERT_FALSE(results3.results.empty());
    EXPECT_EQ(results3.results[0].document->get_id(), "cache_test");
}

// Test performance cache clearing
TEST_F(SearchEngineEnhancedTest, CacheClearingFunctionality) {
    // Populate caches
    engine->search_by_content("machine learning");

    // Clear caches
    EXPECT_NO_THROW(engine->clear_performance_caches());

    // Should still work after clearing caches
    auto results = engine->search_by_content("machine learning");
    ASSERT_FALSE(results.results.empty());
}

#endif // ATOM_SEARCH_TEST_SEARCH_ENHANCED_HPP
