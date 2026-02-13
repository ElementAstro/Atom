/**
 * @file interface_validation_test.hpp
 * @brief Header-only interface validation for search module
 * @date 2025-08-04
 */

#ifndef ATOM_SEARCH_INTERFACE_VALIDATION_TEST_HPP
#define ATOM_SEARCH_INTERFACE_VALIDATION_TEST_HPP

#include <iostream>
#include <type_traits>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

// Include search headers to validate interfaces
#include "atom/search/search.hpp"

namespace atom::search::test {

/**
 * @brief Compile-time interface validation tests
 * These tests validate that the search module interfaces are correctly defined
 * and can be used as expected without requiring runtime execution.
 */
class InterfaceValidator {
public:
    // Test Document interface
    static void validate_document_interface() {
        // Test that Document can be constructed with different parameter types
        static_assert(std::is_constructible_v<Document, std::string, std::string, std::initializer_list<std::string>>,
                     "Document should be constructible with initializer_list");

        static_assert(std::is_constructible_v<Document, std::string, std::string, std::vector<std::string>>,
                     "Document should be constructible with vector");

        // Test that Document has required methods
        static_assert(std::is_same_v<decltype(std::declval<Document>().get_id()), std::string_view>,
                     "get_id() should return string_view");

        static_assert(std::is_same_v<decltype(std::declval<Document>().get_content()), std::string_view>,
                     "get_content() should return string_view");

        static_assert(std::is_same_v<decltype(std::declval<Document>().get_click_count()), int>,
                     "get_click_count() should return int");

        // Test that Document is copyable and movable
        static_assert(std::is_copy_constructible_v<Document>, "Document should be copy constructible");
        static_assert(std::is_move_constructible_v<Document>, "Document should be move constructible");
        static_assert(std::is_copy_assignable_v<Document>, "Document should be copy assignable");
        static_assert(std::is_move_assignable_v<Document>, "Document should be move assignable");
    }

    // Test SearchEngine interface
    static void validate_search_engine_interface() {
        // Test SearchEngine construction
        static_assert(std::is_constructible_v<SearchEngine, unsigned>,
                     "SearchEngine should be constructible with thread count");

        static_assert(std::is_constructible_v<SearchEngine, unsigned, SearchConfig>,
                     "SearchEngine should be constructible with config");

        // Test that SearchEngine is not copyable (should be move-only or neither)
        static_assert(!std::is_copy_constructible_v<SearchEngine>, "SearchEngine should not be copy constructible");
        static_assert(!std::is_copy_assignable_v<SearchEngine>, "SearchEngine should not be copy assignable");

        // Test return types of search methods
        using DocVector = std::vector<std::shared_ptr<Document>>;

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().search_by_tag(std::declval<std::string>())), DocVector>,
                     "search_by_tag should return vector of shared_ptr<Document>");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().search_by_content(std::declval<std::string>())), DocVector>,
                     "search_by_content should return vector of shared_ptr<Document>");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().get_document_count()), size_t>,
                     "get_document_count should return size_t");
    }

    // Test SearchConfig interface
    static void validate_search_config_interface() {
        // Test that SearchConfig has expected members
        SearchConfig config;

        // These should compile without errors
        config.max_results = 100;
        config.score_threshold = 0.5;
        config.enable_stemming = true;
        config.enable_fuzzy = true;
        config.cache_size = 1000;
        config.enable_performance_caching = true;
        config.enable_semantic_search = true;

        // Test that SearchConfig is copyable
        static_assert(std::is_copy_constructible_v<SearchConfig>, "SearchConfig should be copy constructible");
        static_assert(std::is_copy_assignable_v<SearchConfig>, "SearchConfig should be copy assignable");
    }

    // Test SearchResults interface
    static void validate_search_results_interface() {
        // Test SearchResults structure
        SearchResults results;

        // These should compile without errors
        results.total_count = 10;
        results.offset = 0;
        results.search_time_ms = 1.5;
        results.from_cache = false;

        // Test SearchResult structure
        SearchResult result;
        result.score = 0.8;
        result.snippet = "test snippet";

        static_assert(std::is_same_v<decltype(results.results), std::vector<SearchResult>>,
                     "SearchResults.results should be vector<SearchResult>");
    }

    // Test exception interfaces
    static void validate_exception_interfaces() {
        // Test that exceptions inherit from std::exception
        static_assert(std::is_base_of_v<std::exception, SearchEngineException>,
                     "SearchEngineException should inherit from std::exception");

        static_assert(std::is_base_of_v<SearchEngineException, DocumentNotFoundException>,
                     "DocumentNotFoundException should inherit from SearchEngineException");

        static_assert(std::is_base_of_v<SearchEngineException, DocumentValidationException>,
                     "DocumentValidationException should inherit from SearchEngineException");

        static_assert(std::is_base_of_v<SearchEngineException, SearchOperationException>,
                     "SearchOperationException should inherit from SearchEngineException");
    }

    // Test enhanced search method interfaces
    static void validate_enhanced_search_interfaces() {
        SearchPagination pagination;
        pagination.offset = 0;
        pagination.limit = 10;

        // Test that enhanced search methods return SearchResults
        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().search_by_content_enhanced(
                         std::declval<std::string>(), std::declval<SearchPagination>())), SearchResults>,
                     "search_by_content_enhanced should return SearchResults");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().phrase_search(
                         std::declval<std::string>(), std::declval<SearchPagination>())), SearchResults>,
                     "phrase_search should return SearchResults");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().wildcard_search(
                         std::declval<std::string>(), std::declval<SearchPagination>())), SearchResults>,
                     "wildcard_search should return SearchResults");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().regex_search(
                         std::declval<std::string>(), std::declval<SearchPagination>())), SearchResults>,
                     "regex_search should return SearchResults");
    }

    // Test bulk operation interfaces
    static void validate_bulk_operation_interfaces() {
        using DocVector = std::vector<Document>;
        using StringVector = std::vector<std::string>;

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().bulk_insert(std::declval<DocVector>())), size_t>,
                     "bulk_insert should return size_t");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().bulk_update(std::declval<DocVector>())), size_t>,
                     "bulk_update should return size_t");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().bulk_delete(std::declval<StringVector>())), size_t>,
                     "bulk_delete should return size_t");
    }

    // Test similarity search interfaces
    static void validate_similarity_interfaces() {
        using SimilarityResult = std::vector<std::pair<std::shared_ptr<Document>, double>>;

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().find_similar_documents(
                         std::declval<std::string>(), size_t{10}, double{0.1})), SimilarityResult>,
                     "find_similar_documents should return vector of document-similarity pairs");

        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().semantic_search(
                         std::declval<std::string>(), std::declval<SearchPagination>())), SearchResults>,
                     "semantic_search should return SearchResults");
    }

    // Test metrics interface
    static void validate_metrics_interface() {
        static_assert(std::is_same_v<decltype(std::declval<SearchEngine>().get_metrics()), const SearchMetrics&>,
                     "get_metrics should return const SearchMetrics&");

        // Test SearchMetrics structure
        SearchMetrics metrics;

        // These should compile and be atomic
        static_assert(std::is_same_v<decltype(metrics.total_searches), std::atomic<uint64_t>>,
                     "total_searches should be atomic<uint64_t>");

        static_assert(std::is_same_v<decltype(metrics.cache_hits), std::atomic<uint64_t>>,
                     "cache_hits should be atomic<uint64_t>");

        // Test metric calculation methods
        static_assert(std::is_same_v<decltype(metrics.get_cache_hit_ratio()), double>,
                     "get_cache_hit_ratio should return double");

        static_assert(std::is_same_v<decltype(metrics.get_average_search_time_ms()), double>,
                     "get_average_search_time_ms should return double");
    }

    // Run all validation tests
    static void run_all_validations() {
        std::cout << "Running interface validation tests..." << std::endl;

        validate_document_interface();
        std::cout << "✅ Document interface validation passed" << std::endl;

        validate_search_engine_interface();
        std::cout << "✅ SearchEngine interface validation passed" << std::endl;

        validate_search_config_interface();
        std::cout << "✅ SearchConfig interface validation passed" << std::endl;

        validate_search_results_interface();
        std::cout << "✅ SearchResults interface validation passed" << std::endl;

        validate_exception_interfaces();
        std::cout << "✅ Exception interface validation passed" << std::endl;

        validate_enhanced_search_interfaces();
        std::cout << "✅ Enhanced search interface validation passed" << std::endl;

        validate_bulk_operation_interfaces();
        std::cout << "✅ Bulk operation interface validation passed" << std::endl;

        validate_similarity_interfaces();
        std::cout << "✅ Similarity search interface validation passed" << std::endl;

        validate_metrics_interface();
        std::cout << "✅ Metrics interface validation passed" << std::endl;

        std::cout << "\n🎉 All interface validations passed!" << std::endl;
        std::cout << "The search module interfaces are correctly defined and type-safe." << std::endl;
    }
};

// Test that we can instantiate basic types without linking
class BasicInstantiationTest {
public:
    static void test_basic_instantiation() {
        std::cout << "\nTesting basic type instantiation..." << std::endl;

        // Test SearchConfig instantiation
        SearchConfig config;
        config.max_results = 50;
        config.enable_stemming = true;
        std::cout << "✅ SearchConfig instantiation successful" << std::endl;

        // Test SearchPagination instantiation
        SearchPagination pagination{0, 10};
        std::cout << "✅ SearchPagination instantiation successful" << std::endl;

        // Test SearchResults instantiation
        SearchResults results;
        results.total_count = 5;
        results.search_time_ms = 1.23;
        std::cout << "✅ SearchResults instantiation successful" << std::endl;

        // Test SearchResult instantiation
        SearchResult result;
        result.score = 0.85;
        result.snippet = "test snippet";
        std::cout << "✅ SearchResult instantiation successful" << std::endl;

        // Test SearchMetrics instantiation
        SearchMetrics metrics;
        metrics.total_searches = 10;
        double hit_ratio = metrics.get_cache_hit_ratio();
        std::cout << "✅ SearchMetrics instantiation successful (hit ratio: " << hit_ratio << ")" << std::endl;

        std::cout << "\n🎉 All basic instantiation tests passed!" << std::endl;
    }
};

} // namespace atom::search::test

// Main validation function that can be called from other test files
inline void run_interface_validation() {
    std::cout << "=== SEARCH MODULE INTERFACE VALIDATION ===" << std::endl;

    atom::search::test::InterfaceValidator::run_all_validations();
    atom::search::test::BasicInstantiationTest::test_basic_instantiation();

    std::cout << "\n=== VALIDATION COMPLETE ===" << std::endl;
    std::cout << "The search module interfaces are well-defined and ready for use." << std::endl;
}

#endif // ATOM_SEARCH_INTERFACE_VALIDATION_TEST_HPP
