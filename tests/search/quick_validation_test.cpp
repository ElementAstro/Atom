/**
 * @file quick_validation_test.cpp
 * @brief Quick validation test for core search functionality
 * @date 2025-08-04
 */

#include <gtest/gtest.h>
#include <iostream>
#include <chrono>

#include "atom/search/search.hpp"

using namespace atom::search;

// Quick test for basic search functionality
TEST(QuickSearchTest, BasicFunctionality) {
    // Create search engine with minimal configuration
    SearchConfig config;
    config.enable_performance_caching = false;  // Disable caching for quick tests
    config.max_results = 10;
    
    SearchEngine engine;  // Use default constructor like existing tests
    
    // Add test documents
    ASSERT_NO_THROW(engine.add_document(Document("doc1", "machine learning algorithms", {"ai", "ml"})));
    ASSERT_NO_THROW(engine.add_document(Document("doc2", "deep learning neural networks", {"ai", "deep"})));
    ASSERT_NO_THROW(engine.add_document(Document("doc3", "natural language processing", {"nlp", "language"})));
    
    // Verify document count
    EXPECT_EQ(engine.get_document_count(), 3);
    
    // Test basic content search
    auto content_results = engine.search_by_content("machine");
    EXPECT_GT(content_results.size(), 0);
    EXPECT_LE(content_results.size(), 3);
    
    // Test tag search
    auto tag_results = engine.search_by_tag("ai");
    EXPECT_EQ(tag_results.size(), 2);  // Should find doc1 and doc2
    
    // Test multi-tag search
    std::vector<std::string> tags = {"ai", "ml"};
    auto multi_tag_results = engine.search_by_tags(tags);
    EXPECT_GE(multi_tag_results.size(), 1);  // Should find at least doc1
    
    // Test boolean search
    auto boolean_results = engine.boolean_search("machine AND learning");
    EXPECT_GT(boolean_results.size(), 0);
    
    std::cout << "✅ Basic search functionality working correctly!" << std::endl;
}

// Test enhanced search methods
TEST(QuickSearchTest, EnhancedMethods) {
    SearchConfig config;
    config.enable_performance_caching = false;
    
    SearchEngine engine;
    
    // Add test documents
    engine.add_document(Document("doc1", "machine learning algorithms for AI", {"ai", "ml"}));
    engine.add_document(Document("doc2", "The quick brown fox jumps over lazy dog", {"animals"}));
    
    SearchPagination pagination{0, 10};
    
    // Test phrase search
    auto phrase_results = engine.phrase_search("machine learning", pagination);
    EXPECT_GT(phrase_results.total_count, 0);
    EXPECT_FALSE(phrase_results.results.empty());
    
    // Test wildcard search
    auto wildcard_results = engine.wildcard_search("mach*", pagination);
    EXPECT_GT(wildcard_results.total_count, 0);
    
    // Test regex search
    auto regex_results = engine.regex_search("machine|quick", pagination);
    EXPECT_GT(regex_results.total_count, 0);
    
    // Test enhanced content search
    auto enhanced_results = engine.search_by_content_enhanced("machine", pagination);
    EXPECT_GT(enhanced_results.total_count, 0);
    EXPECT_GT(enhanced_results.search_time_ms, 0.0);
    
    std::cout << "✅ Enhanced search methods working correctly!" << std::endl;
}

// Test error handling
TEST(QuickSearchTest, ErrorHandling) {
    SearchEngine engine;
    
    // Test document validation
    EXPECT_THROW(Document("", "content"), DocumentValidationException);
    EXPECT_THROW(Document("id", ""), DocumentValidationException);
    
    // Test duplicate document
    engine.add_document(Document("test", "content", {"tag"}));
    EXPECT_THROW(engine.add_document(Document("test", "duplicate", {"tag"})), std::invalid_argument);
    
    // Test removing non-existent document
    EXPECT_THROW(engine.remove_document("nonexistent"), DocumentNotFoundException);
    
    // Test invalid regex
    SearchPagination pagination{0, 10};
    EXPECT_THROW(engine.regex_search("[invalid", pagination), SearchOperationException);
    
    std::cout << "✅ Error handling working correctly!" << std::endl;
}

// Test bulk operations
TEST(QuickSearchTest, BulkOperations) {
    SearchEngine engine;
    
    // Test bulk insert
    std::vector<Document> docs;
    for (int i = 0; i < 5; ++i) {
        docs.emplace_back("bulk" + std::to_string(i), 
                         "bulk content " + std::to_string(i), 
                         std::vector<std::string>{"bulk", "test"});
    }
    
    size_t inserted = engine.bulk_insert(docs);
    EXPECT_EQ(inserted, 5);
    EXPECT_EQ(engine.get_document_count(), 5);
    
    // Test bulk update
    std::vector<Document> update_docs;
    for (int i = 0; i < 3; ++i) {
        update_docs.emplace_back("bulk" + std::to_string(i), 
                                "updated content " + std::to_string(i), 
                                std::vector<std::string>{"bulk", "updated"});
    }
    
    size_t updated = engine.bulk_update(update_docs);
    EXPECT_EQ(updated, 3);
    
    // Verify updates
    auto updated_results = engine.search_by_tag("updated");
    EXPECT_EQ(updated_results.size(), 3);
    
    // Test bulk delete
    std::vector<String> delete_ids = {"bulk0", "bulk1"};
    size_t deleted = engine.bulk_delete(delete_ids);
    EXPECT_EQ(deleted, 2);
    EXPECT_EQ(engine.get_document_count(), 3);
    
    std::cout << "✅ Bulk operations working correctly!" << std::endl;
}

// Test configuration and metrics
TEST(QuickSearchTest, ConfigAndMetrics) {
    SearchConfig config;
    config.max_results = 50;
    config.enable_stemming = true;
    
    SearchEngine engine(2, config);
    
    // Test configuration retrieval
    const auto& retrieved_config = engine.get_config();
    EXPECT_EQ(retrieved_config.max_results, 50);
    EXPECT_TRUE(retrieved_config.enable_stemming);
    
    // Test configuration update
    SearchConfig new_config = retrieved_config;
    new_config.max_results = 100;
    engine.update_config(new_config);
    
    const auto& updated_config = engine.get_config();
    EXPECT_EQ(updated_config.max_results, 100);
    
    // Test metrics
    engine.reset_metrics();
    const auto& initial_metrics = engine.get_metrics();
    EXPECT_EQ(initial_metrics.total_searches.load(), 0);
    
    // Perform a search to update metrics
    engine.add_document(Document("test", "test content", {"test"}));
    engine.search_by_content("test");
    
    const auto& updated_metrics = engine.get_metrics();
    EXPECT_GT(updated_metrics.total_searches.load(), 0);
    
    std::cout << "✅ Configuration and metrics working correctly!" << std::endl;
}

// Performance test
TEST(QuickSearchTest, Performance) {
    SearchEngine engine;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add many documents
    for (int i = 0; i < 100; ++i) {
        std::string content = "document " + std::to_string(i) + " with content about ";
        if (i % 4 == 0) content += "machine learning";
        else if (i % 4 == 1) content += "artificial intelligence";
        else if (i % 4 == 2) content += "data science";
        else content += "computer vision";
        
        engine.add_document(Document("perf" + std::to_string(i), content, {"performance"}));
    }
    
    auto insert_end = std::chrono::high_resolution_clock::now();
    auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - start);
    
    // Perform searches
    auto search_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10; ++i) {
        engine.search_by_content("machine");
        engine.search_by_tag("performance");
        engine.boolean_search("machine AND learning");
    }
    
    auto search_end = std::chrono::high_resolution_clock::now();
    auto search_time = std::chrono::duration_cast<std::chrono::milliseconds>(search_end - search_start);
    
    std::cout << "✅ Performance test completed:" << std::endl;
    std::cout << "  - Insert time: " << insert_time.count() << " ms for 100 documents" << std::endl;
    std::cout << "  - Search time: " << search_time.count() << " ms for 30 searches" << std::endl;
    
    // Basic performance expectations
    EXPECT_LT(insert_time.count(), 5000);  // Should insert 100 docs in less than 5 seconds
    EXPECT_LT(search_time.count(), 1000);  // Should perform 30 searches in less than 1 second
}

// Note: main() function is provided by the main test framework
// These tests will be automatically discovered and run by the existing test runner
