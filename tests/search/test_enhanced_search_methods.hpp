#ifndef ATOM_SEARCH_TEST_ENHANCED_SEARCH_METHODS_HPP
#define ATOM_SEARCH_TEST_ENHANCED_SEARCH_METHODS_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for enhanced search methods
class EnhancedSearchMethodsTest : public ::testing::Test {
protected:
    SearchConfig config;
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        config.enable_performance_caching = true;
        config.enable_semantic_search = true;
        config.enable_stemming = true;
        config.cache_size = 100;
        config.cache_ttl = std::chrono::milliseconds(1000);

        engine = std::make_unique<SearchEngine>(4, config);

        // Add comprehensive test documents
        engine->add_document(Document("doc1", "machine learning algorithms for artificial intelligence", {"ai", "ml", "tech"}));
        engine->add_document(Document("doc2", "deep learning neural networks and machine learning", {"ai", "deep", "neural"}));
        engine->add_document(Document("doc3", "natural language processing with machine learning", {"nlp", "ai", "language"}));
        engine->add_document(Document("doc4", "computer vision applications using deep learning", {"cv", "vision", "tech"}));
        engine->add_document(Document("doc5", "data science and analytics with machine learning", {"data", "science", "analytics"}));
        engine->add_document(Document("doc6", "The quick brown fox jumps over the lazy dog", {"animals", "test"}));
        engine->add_document(Document("doc7", "Programming languages: Python, Java, C++", {"programming", "languages"}));
        engine->add_document(Document("doc8", "Web development with JavaScript and HTML", {"web", "development", "js"}));
    }
};

// Test phrase search functionality
TEST_F(EnhancedSearchMethodsTest, PhraseSearch) {
    SearchPagination pagination{0, 10};

    // Test exact phrase matching
    auto results = engine->phrase_search("machine learning", pagination);
    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());
    EXPECT_EQ(results.offset, 0);
    EXPECT_GT(results.search_time_ms, 0.0);

    // Verify all results contain the exact phrase
    for (const auto& result : results.results) {
        std::string content = std::string(result.document->get_content());
        std::transform(content.begin(), content.end(), content.begin(), ::tolower);
        EXPECT_NE(content.find("machine learning"), std::string::npos);
    }

    // Test phrase not found
    auto no_results = engine->phrase_search("nonexistent phrase", pagination);
    EXPECT_EQ(no_results.total_count, 0);
    EXPECT_TRUE(no_results.results.empty());

    // Test single word phrase
    auto single_word = engine->phrase_search("programming", pagination);
    EXPECT_GT(single_word.total_count, 0);
}

// Test wildcard search functionality
TEST_F(EnhancedSearchMethodsTest, WildcardSearch) {
    SearchPagination pagination{0, 10};

    // Test asterisk wildcard
    auto results = engine->wildcard_search("mach*", pagination);
    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());

    // Test question mark wildcard
    auto single_char = engine->wildcard_search("?eb", pagination);
    EXPECT_GE(single_char.total_count, 0);  // May or may not find matches

    // Test combined wildcards
    auto combined = engine->wildcard_search("*learn*", pagination);
    EXPECT_GT(combined.total_count, 0);

    // Test no wildcard (should work like regular search)
    auto no_wildcard = engine->wildcard_search("programming", pagination);
    EXPECT_GT(no_wildcard.total_count, 0);

    // Test invalid patterns
    auto empty_pattern = engine->wildcard_search("", pagination);
    EXPECT_EQ(empty_pattern.total_count, 0);
}

// Test regex search functionality
TEST_F(EnhancedSearchMethodsTest, RegexSearch) {
    SearchPagination pagination{0, 10};

    // Test basic regex
    auto results = engine->regex_search("machine|deep", pagination);
    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());

    // Test case insensitive regex
    auto case_insensitive = engine->regex_search("MACHINE", pagination);
    EXPECT_GT(case_insensitive.total_count, 0);

    // Test word boundaries
    auto word_boundary = engine->regex_search("\\bweb\\b", pagination);
    EXPECT_GT(word_boundary.total_count, 0);

    // Test character classes
    auto char_class = engine->regex_search("[Pp]ython", pagination);
    EXPECT_GT(char_class.total_count, 0);

    // Test quantifiers
    auto quantifier = engine->regex_search("learn(ing)?", pagination);
    EXPECT_GT(quantifier.total_count, 0);

    // Test invalid regex - should throw exception
    EXPECT_THROW(engine->regex_search("[invalid", pagination), SearchOperationException);
    EXPECT_THROW(engine->regex_search("*invalid", pagination), SearchOperationException);
}

// Test enhanced content search
TEST_F(EnhancedSearchMethodsTest, EnhancedContentSearch) {
    SearchPagination pagination{0, 5};

    auto results = engine->search_by_content_enhanced("machine learning", pagination);
    
    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());
    EXPECT_LE(results.results.size(), 5);  // Respects pagination limit
    EXPECT_EQ(results.offset, 0);
    EXPECT_GT(results.search_time_ms, 0.0);

    // Check that results have scores and snippets
    for (const auto& result : results.results) {
        EXPECT_GT(result.score, 0.0);
        EXPECT_FALSE(result.snippet.empty());
        EXPECT_FALSE(result.matched_terms.empty());
    }

    // Test pagination
    SearchPagination page2{2, 3};
    auto page2_results = engine->search_by_content_enhanced("learning", page2);
    EXPECT_EQ(page2_results.offset, 2);
    EXPECT_LE(page2_results.results.size(), 3);
}

// Test enhanced tag search
TEST_F(EnhancedSearchMethodsTest, EnhancedTagSearch) {
    SearchPagination pagination{0, 10};

    auto results = engine->search_by_tag_enhanced("ai", pagination);
    
    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());
    EXPECT_EQ(results.offset, 0);
    EXPECT_GT(results.search_time_ms, 0.0);

    // Verify all results have the tag
    for (const auto& result : results.results) {
        const auto& tags = result.document->get_tags();
        EXPECT_TRUE(tags.count("ai") > 0);
    }

    // Test non-existent tag
    auto no_results = engine->search_by_tag_enhanced("nonexistent", pagination);
    EXPECT_EQ(no_results.total_count, 0);
    EXPECT_TRUE(no_results.results.empty());
}

// Test enhanced multi-tag search
TEST_F(EnhancedSearchMethodsTest, EnhancedMultiTagSearch) {
    SearchPagination pagination{0, 10};

    std::vector<std::string> tags = {"ai", "ml"};
    auto results = engine->search_by_tags_enhanced(tags, pagination);
    
    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());
    EXPECT_EQ(results.offset, 0);
    EXPECT_GT(results.search_time_ms, 0.0);

    // Results should be ranked by relevance
    for (size_t i = 1; i < results.results.size(); ++i) {
        EXPECT_GE(results.results[i-1].score, results.results[i].score);
    }

    // Test empty tag list
    auto empty_results = engine->search_by_tags_enhanced({}, pagination);
    EXPECT_EQ(empty_results.total_count, 0);
}

// Test caching behavior
TEST_F(EnhancedSearchMethodsTest, CachingBehavior) {
    SearchPagination pagination{0, 10};

    // First search should not be from cache
    auto results1 = engine->phrase_search("machine learning", pagination);
    EXPECT_FALSE(results1.from_cache);

    // Second identical search should be from cache
    auto results2 = engine->phrase_search("machine learning", pagination);
    EXPECT_TRUE(results2.from_cache);

    // Results should be identical
    EXPECT_EQ(results1.total_count, results2.total_count);
    EXPECT_EQ(results1.results.size(), results2.results.size());

    // Different pagination should not use cache
    SearchPagination different_pagination{1, 5};
    auto results3 = engine->phrase_search("machine learning", different_pagination);
    EXPECT_FALSE(results3.from_cache);
}

// Test search result metadata
TEST_F(EnhancedSearchMethodsTest, SearchResultMetadata) {
    SearchPagination pagination{0, 10};

    auto results = engine->search_by_content_enhanced("machine learning algorithms", pagination);
    
    EXPECT_FALSE(results.results.empty());

    for (const auto& result : results.results) {
        // Check document is valid
        EXPECT_NE(result.document, nullptr);
        EXPECT_FALSE(result.document->get_id().empty());
        EXPECT_FALSE(result.document->get_content().empty());

        // Check score is reasonable
        EXPECT_GT(result.score, 0.0);
        EXPECT_LE(result.score, 10.0);  // Reasonable upper bound

        // Check matched terms
        EXPECT_FALSE(result.matched_terms.empty());

        // Check snippet
        EXPECT_FALSE(result.snippet.empty());
        EXPECT_LE(result.snippet.length(), 250);  // Should be reasonably sized
    }
}

#endif // ATOM_SEARCH_TEST_ENHANCED_SEARCH_METHODS_HPP
