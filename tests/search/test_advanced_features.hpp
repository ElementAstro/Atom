#ifndef ATOM_SEARCH_TEST_ADVANCED_FEATURES_HPP
#define ATOM_SEARCH_TEST_ADVANCED_FEATURES_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>
#include <random>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for advanced features
class AdvancedFeaturesTest : public ::testing::Test {
protected:
    SearchConfig config;
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        config.enable_performance_caching = true;
        config.enable_stemming = true;
        config.enable_semantic_search = true;
        config.enable_ranked_autocomplete = true;
        config.cache_size = 50;
        config.cache_ttl = std::chrono::milliseconds(500);
        config.tokenized_cache_size = 100;
        config.tf_idf_cache_size = 200;

        engine = std::make_unique<SearchEngine>(4, config);

        // Add test documents with various content
        engine->add_document(Document("stem1", "running runner runs", {"sports", "action"}));
        engine->add_document(Document("stem2", "programming programmer programs", {"tech", "coding"}));
        engine->add_document(Document("stem3", "learning learner learns", {"education", "study"}));
        engine->add_document(Document("unicode1", "café naïve résumé", {"unicode", "text"}));
        engine->add_document(Document("unicode2", "北京 東京 москва", {"unicode", "cities"}));
        engine->add_document(Document("special1", "test@email.com http://example.com", {"contact", "web"}));
        engine->add_document(Document("large1", generateLargeContent(1000), {"large", "content"}));
    }

    std::string generateLargeContent(size_t word_count) {
        std::vector<std::string> words = {
            "machine", "learning", "artificial", "intelligence", "neural", "network",
            "deep", "algorithm", "data", "science", "computer", "vision", "natural",
            "language", "processing", "programming", "software", "development"
        };
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, words.size() - 1);
        
        std::string content;
        for (size_t i = 0; i < word_count; ++i) {
            if (i > 0) content += " ";
            content += words[dis(gen)];
        }
        return content;
    }
};

// Test stemming functionality
TEST_F(AdvancedFeaturesTest, StemmingFunctionality) {
    // Test that stemming works in search
    auto results1 = engine->search_by_content("run");
    auto results2 = engine->search_by_content("running");
    auto results3 = engine->search_by_content("runs");

    // With stemming enabled, all should find the same document
    EXPECT_GT(results1.size(), 0);
    EXPECT_EQ(results1.size(), results2.size());
    EXPECT_EQ(results1.size(), results3.size());

    // Test programming variations
    auto prog_results1 = engine->search_by_content("program");
    auto prog_results2 = engine->search_by_content("programming");
    auto prog_results3 = engine->search_by_content("programs");

    EXPECT_GT(prog_results1.size(), 0);
    EXPECT_EQ(prog_results1.size(), prog_results2.size());
    EXPECT_EQ(prog_results1.size(), prog_results3.size());
}

// Test configuration updates
TEST_F(AdvancedFeaturesTest, ConfigurationUpdates) {
    // Get initial config
    const auto& initial_config = engine->get_config();
    EXPECT_TRUE(initial_config.enable_stemming);
    EXPECT_TRUE(initial_config.enable_performance_caching);

    // Update configuration
    SearchConfig new_config = initial_config;
    new_config.enable_stemming = false;
    new_config.max_results = 50;
    new_config.score_threshold = 0.5;

    engine->update_config(new_config);

    // Verify configuration was updated
    const auto& updated_config = engine->get_config();
    EXPECT_FALSE(updated_config.enable_stemming);
    EXPECT_EQ(updated_config.max_results, 50);
    EXPECT_EQ(updated_config.score_threshold, 0.5);
    EXPECT_TRUE(updated_config.enable_performance_caching);  // Should remain unchanged
}

// Test index optimization
TEST_F(AdvancedFeaturesTest, IndexOptimization) {
    // Add more documents
    for (int i = 0; i < 20; ++i) {
        engine->add_document(Document("opt" + std::to_string(i), 
                                    "optimization test document " + std::to_string(i),
                                    {"optimization", "test"}));
    }

    // Get initial stats
    auto initial_stats = engine->get_index_stats();
    EXPECT_GT(initial_stats["total_documents"], 0);

    // Optimize index
    EXPECT_NO_THROW(engine->optimize_index());

    // Get stats after optimization
    auto optimized_stats = engine->get_index_stats();
    EXPECT_EQ(optimized_stats["total_documents"], initial_stats["total_documents"]);

    // Search should still work after optimization
    auto results = engine->search_by_tag("optimization");
    EXPECT_EQ(results.size(), 20);
}

// Test index statistics
TEST_F(AdvancedFeaturesTest, IndexStatistics) {
    auto stats = engine->get_index_stats();

    // Check that basic statistics are present
    EXPECT_TRUE(stats.count("total_documents") > 0);
    EXPECT_TRUE(stats.count("total_shards") > 0);
    EXPECT_TRUE(stats.count("total_terms") > 0);

    // Verify document count matches
    EXPECT_EQ(stats["total_documents"], engine->get_document_count());

    // Add a document and verify stats update
    size_t initial_count = stats["total_documents"];
    engine->add_document(Document("stats_test", "statistics test document", {"stats"}));

    auto updated_stats = engine->get_index_stats();
    EXPECT_EQ(updated_stats["total_documents"], initial_count + 1);
}

// Test metrics functionality
TEST_F(AdvancedFeaturesTest, MetricsFunctionality) {
    // Reset metrics to start fresh
    engine->reset_metrics();

    const auto& initial_metrics = engine->get_metrics();
    EXPECT_EQ(initial_metrics.total_searches.load(), 0);
    EXPECT_EQ(initial_metrics.cache_hits.load(), 0);
    EXPECT_EQ(initial_metrics.cache_misses.load(), 0);

    // Perform some searches
    engine->search_by_content("machine");
    engine->search_by_content("learning");
    engine->search_by_content("machine");  // Should hit cache

    const auto& updated_metrics = engine->get_metrics();
    EXPECT_GT(updated_metrics.total_searches.load(), 0);
    EXPECT_GT(updated_metrics.cache_hits.load(), 0);
    EXPECT_GT(updated_metrics.cache_misses.load(), 0);

    // Test metric calculations
    EXPECT_GE(updated_metrics.get_cache_hit_ratio(), 0.0);
    EXPECT_LE(updated_metrics.get_cache_hit_ratio(), 1.0);
    EXPECT_GT(updated_metrics.get_average_search_time_ms(), 0.0);
}

// Test Unicode and special character handling
TEST_F(AdvancedFeaturesTest, UnicodeAndSpecialCharacters) {
    // Test Unicode content search
    auto unicode_results1 = engine->search_by_content("café");
    EXPECT_GT(unicode_results1.size(), 0);

    auto unicode_results2 = engine->search_by_content("北京");
    EXPECT_GT(unicode_results2.size(), 0);

    auto unicode_results3 = engine->search_by_content("москва");
    EXPECT_GT(unicode_results3.size(), 0);

    // Test special characters
    auto email_results = engine->search_by_content("test@email.com");
    EXPECT_GT(email_results.size(), 0);

    auto url_results = engine->search_by_content("http://example.com");
    EXPECT_GT(url_results.size(), 0);

    // Test that Unicode tags work
    engine->add_document(Document("unicode_tag", "test content", {"测试", "тест"}));
    auto unicode_tag_results = engine->search_by_tag("测试");
    EXPECT_GT(unicode_tag_results.size(), 0);
}

// Test large document handling
TEST_F(AdvancedFeaturesTest, LargeDocumentHandling) {
    // Search in large document
    auto results = engine->search_by_content("machine");
    EXPECT_GT(results.size(), 0);

    // Find the large document
    bool found_large = false;
    for (const auto& doc : results) {
        if (doc->get_id() == "large1") {
            found_large = true;
            EXPECT_GT(doc->get_content().length(), 500);  // Should be large
            break;
        }
    }
    EXPECT_TRUE(found_large);

    // Test that large documents can be updated
    Document large_update("large1", generateLargeContent(2000), {"large", "updated"});
    EXPECT_NO_THROW(engine->update_document(large_update));

    // Verify update worked
    auto updated_results = engine->search_by_tag("updated");
    EXPECT_GT(updated_results.size(), 0);
}

// Test cache TTL and expiration
TEST_F(AdvancedFeaturesTest, CacheTTLAndExpiration) {
    SearchPagination pagination{0, 10};

    // First search - should not be from cache
    auto results1 = engine->search_by_content_enhanced("machine", pagination);
    EXPECT_FALSE(results1.from_cache);

    // Immediate second search - should be from cache
    auto results2 = engine->search_by_content_enhanced("machine", pagination);
    EXPECT_TRUE(results2.from_cache);

    // Wait for cache to expire (TTL is 500ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Search again - should not be from cache due to expiration
    auto results3 = engine->search_by_content_enhanced("machine", pagination);
    EXPECT_FALSE(results3.from_cache);
}

// Test ranked autocomplete
TEST_F(AdvancedFeaturesTest, RankedAutocomplete) {
    // Add documents with varying frequencies of terms
    for (int i = 0; i < 5; ++i) {
        engine->add_document(Document("freq" + std::to_string(i), 
                                    "machine learning artificial intelligence",
                                    {"frequent"}));
    }
    
    engine->add_document(Document("rare1", "machine vision", {"rare"}));

    // Test ranked autocomplete
    auto suggestions = engine->auto_complete_ranked("mach", 10);
    EXPECT_GT(suggestions.size(), 0);

    // Results should be ranked by frequency
    for (size_t i = 1; i < suggestions.size(); ++i) {
        EXPECT_GE(suggestions[i-1].second, suggestions[i].second);
    }

    // "machine" should appear with high frequency
    bool found_machine = false;
    for (const auto& [term, freq] : suggestions) {
        if (term == "machine") {
            found_machine = true;
            EXPECT_GT(freq, 1);  // Should appear multiple times
            break;
        }
    }
    EXPECT_TRUE(found_machine);
}

// Test error handling and edge cases
TEST_F(AdvancedFeaturesTest, ErrorHandlingAndEdgeCases) {
    // Test empty queries
    EXPECT_TRUE(engine->search_by_content("").empty());
    EXPECT_TRUE(engine->search_by_tag("").empty());

    // Test very long queries
    std::string long_query(10000, 'a');
    EXPECT_NO_THROW(engine->search_by_content(long_query));

    // Test queries with only special characters
    auto special_results = engine->search_by_content("!@#$%^&*()");
    EXPECT_TRUE(special_results.empty());

    // Test null and whitespace queries
    EXPECT_TRUE(engine->search_by_content("   ").empty());
    EXPECT_TRUE(engine->search_by_content("\t\n\r").empty());

    // Test document with empty content (should throw during creation)
    EXPECT_THROW(Document("empty", "", {"tag"}), DocumentValidationException);

    // Test document with very long ID
    std::string long_id(300, 'x');
    EXPECT_THROW(Document(long_id, "content", {"tag"}), DocumentValidationException);
}

#endif // ATOM_SEARCH_TEST_ADVANCED_FEATURES_HPP
