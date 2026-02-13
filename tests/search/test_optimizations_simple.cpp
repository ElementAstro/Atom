#include <iostream>
#include <chrono>
#include <cassert>
#include <vector>
#include <string>

// Simple test runner for our search engine optimizations
// This bypasses the complex build system and tests core functionality

#include "atom/search/search.hpp"

using namespace atom::search;

class SimpleTestRunner {
public:
    int tests_run = 0;
    int tests_passed = 0;

public:
    void assert_true(bool condition, const std::string& test_name) {
        tests_run++;
        if (condition) {
            tests_passed++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            std::cout << "[FAIL] " << test_name << std::endl;
        }
    }

    void assert_equals(size_t expected, size_t actual, const std::string& test_name) {
        tests_run++;
        if (expected == actual) {
            tests_passed++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            std::cout << "[FAIL] " << test_name << " - Expected: " << expected << ", Actual: " << actual << std::endl;
        }
    }

    template<typename T>
    void assert_not_empty(const std::vector<T>& vec, const std::string& test_name) {
        tests_run++;
        if (!vec.empty()) {
            tests_passed++;
            std::cout << "[PASS] " << test_name << std::endl;
        } else {
            std::cout << "[FAIL] " << test_name << " - Vector is empty" << std::endl;
        }
    }

    void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Tests run: " << tests_run << std::endl;
        std::cout << "Tests passed: " << tests_passed << std::endl;
        std::cout << "Tests failed: " << (tests_run - tests_passed) << std::endl;
        std::cout << "Success rate: " << (tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0) << "%" << std::endl;
    }
};

void test_basic_functionality(SimpleTestRunner& runner) {
    std::cout << "\n=== Testing Basic Functionality ===" << std::endl;

    try {
        SearchConfig config;
        config.enable_performance_caching = true;
        SearchEngine engine(4, config);

        // Test document addition
        Document doc1("doc1", "machine learning algorithms", {"ai", "ml"});
        engine.add_document(doc1);

        runner.assert_equals(1, engine.get_document_count(), "Document addition");

        // Test search functionality
        auto results = engine.search_by_content("machine");
        runner.assert_true(!results.empty(), "Content search returns results");

        if (!results.empty()) {
            runner.assert_true(results[0]->get_id() == "doc1", "Correct document found");
        }

        // Test tag search
        auto tag_results = engine.search_by_tag("ai");
        runner.assert_true(!tag_results.empty(), "Tag search returns results");

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Basic functionality test failed: " << e.what() << std::endl;
    }
}

void test_performance_optimizations(SimpleTestRunner& runner) {
    std::cout << "\n=== Testing Performance Optimizations ===" << std::endl;

    try {
        SearchConfig config;
        config.enable_performance_caching = true;
        config.tokenized_cache_size = 100;
        config.tf_idf_cache_size = 200;
        SearchEngine engine(4, config);

        // Add test documents
        engine.add_document(Document("doc1", "machine learning algorithms", {"ai", "ml"}));
        engine.add_document(Document("doc2", "deep learning neural networks", {"ai", "deep"}));

        // Test caching by performing same search twice
        auto results1 = engine.search_by_content("machine learning");
        auto results2 = engine.search_by_content("machine learning");

        runner.assert_true(!results1.empty(), "First search returns results");
        runner.assert_true(!results2.empty(), "Second search returns results");
        runner.assert_equals(results1.size(), results2.size(), "Consistent results from cache");

        // Test cache statistics
        auto stats = engine.get_index_stats();
        runner.assert_true(stats.find("performance_cache_hits") != stats.end(), "Cache statistics available");

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Performance optimization test failed: " << e.what() << std::endl;
    }
}

void test_bulk_operations(SimpleTestRunner& runner) {
    std::cout << "\n=== Testing Bulk Operations ===" << std::endl;

    try {
        SearchEngine engine(4);

        // Test bulk insert
        std::vector<Document> docs = {
            Document("bulk1", "bulk test one", {"bulk", "test"}),
            Document("bulk2", "bulk test two", {"bulk", "test"}),
            Document("bulk3", "bulk test three", {"bulk", "test"})
        };

        size_t inserted = engine.bulk_insert(docs);
        runner.assert_equals(3, inserted, "Bulk insert count");
        runner.assert_equals(3, engine.get_document_count(), "Document count after bulk insert");

        // Test bulk update
        std::vector<Document> updates = {
            Document("bulk1", "updated bulk test one", {"bulk", "updated"}),
            Document("bulk2", "updated bulk test two", {"bulk", "updated"})
        };

        size_t updated = engine.bulk_update(updates);
        runner.assert_equals(2, updated, "Bulk update count");

        // Test bulk delete
        std::vector<String> ids_to_delete = {"bulk1", "bulk2", "bulk3"};
        size_t deleted = engine.bulk_delete(ids_to_delete);
        runner.assert_equals(3, deleted, "Bulk delete count");
        runner.assert_equals(0, engine.get_document_count(), "Document count after bulk delete");

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Bulk operations test failed: " << e.what() << std::endl;
    }
}

void test_enhanced_features(SimpleTestRunner& runner) {
    std::cout << "\n=== Testing Enhanced Features ===" << std::endl;

    try {
        SearchConfig config;
        config.enable_semantic_search = true;
        config.enable_ranked_autocomplete = true;
        SearchEngine engine(4, config);

        // Add test documents
        engine.add_document(Document("ai1", "machine learning artificial intelligence", {"ai", "ml"}));
        engine.add_document(Document("ai2", "deep learning neural networks", {"ai", "deep"}));
        engine.add_document(Document("sports1", "football soccer basketball", {"sports"}));

        // Test semantic search
        auto semantic_results = engine.semantic_search("artificial intelligence", {});
        runner.assert_true(!semantic_results.results.empty(), "Semantic search returns results");
        runner.assert_true(semantic_results.search_time_ms > 0, "Semantic search time recorded");

        // Test ranked autocomplete
        auto ranked_suggestions = engine.auto_complete_ranked("a", 5);
        runner.assert_true(!ranked_suggestions.empty(), "Ranked autocomplete returns suggestions");

        // Test similarity search
        auto similar_docs = engine.find_similar_documents("ai1", 2, 0.1);
        runner.assert_true(!similar_docs.empty(), "Similarity search returns results");

        // Test boolean search
        auto boolean_results = engine.boolean_search("machine AND learning");
        runner.assert_true(!boolean_results.empty(), "Boolean search returns results");

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Enhanced features test failed: " << e.what() << std::endl;
    }
}

void test_tokenization_optimizations(SimpleTestRunner& runner) {
    std::cout << "\n=== Testing Search Functionality ===" << std::endl;

    try {
        SearchEngine engine(4);

        // Add a document with mixed case and punctuation
        engine.add_document(Document("test_doc", "Hello, World! This is a TEST with punctuation... and numbers 123.", {"test"}));

        // Test that search works with different cases (indicating tokenization normalizes)
        auto results_lower = engine.search_by_content("hello");
        auto results_upper = engine.search_by_content("HELLO");
        auto results_mixed = engine.search_by_content("Hello");

        runner.assert_true(!results_lower.empty(), "Search finds lowercase terms");
        runner.assert_true(!results_upper.empty(), "Search finds uppercase terms");
        runner.assert_true(!results_mixed.empty(), "Search finds mixed case terms");

        // Test that search ignores punctuation
        auto results_punct = engine.search_by_content("world");
        runner.assert_true(!results_punct.empty(), "Search ignores punctuation");

        // Test number search
        auto results_num = engine.search_by_content("123");
        runner.assert_true(!results_num.empty(), "Search finds numbers");

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Search functionality test failed: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== Search Engine Optimization Tests ===" << std::endl;

    SimpleTestRunner runner;

    test_basic_functionality(runner);
    test_performance_optimizations(runner);
    test_bulk_operations(runner);
    test_enhanced_features(runner);
    test_tokenization_optimizations(runner);

    runner.print_summary();

    return (runner.tests_run == runner.tests_passed) ? 0 : 1;
}
