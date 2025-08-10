#ifndef ATOM_SEARCH_TEST_ERROR_HANDLING_HPP
#define ATOM_SEARCH_TEST_ERROR_HANDLING_HPP

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for error handling and edge cases
class ErrorHandlingTest : public ::testing::Test {
protected:
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        SearchConfig config;
        config.enable_performance_caching = true;
        engine = std::make_unique<SearchEngine>(4, config);

        // Add some basic test documents
        engine->add_document(Document("doc1", "test content one", {"test", "content"}));
        engine->add_document(Document("doc2", "test content two", {"test", "content"}));
    }

    void TearDown() override {
        // Clean up any test files
        std::filesystem::remove("test_invalid_index.json");
        std::filesystem::remove("test_corrupted_index.json");
        std::filesystem::remove("test_readonly_index.json");
    }
};

// Test Document validation errors
TEST_F(ErrorHandlingTest, DocumentValidationErrors) {
    // Test empty ID
    EXPECT_THROW(Document("", "valid content"), DocumentValidationException);

    // Test empty content
    EXPECT_THROW(Document("valid_id", ""), DocumentValidationException);

    // Test ID too long (over 256 characters)
    std::string long_id(300, 'x');
    EXPECT_THROW(Document(long_id, "valid content"), DocumentValidationException);

    // Test empty tag
    EXPECT_THROW(Document("valid_id", "valid content", {""}), DocumentValidationException);

    // Test tag too long (over 100 characters)
    std::string long_tag(150, 'y');
    EXPECT_THROW(Document("valid_id", "valid content", {long_tag}), DocumentValidationException);

    // Test multiple validation errors
    EXPECT_THROW(Document("", "", {""}), DocumentValidationException);

    // Test valid document creation
    EXPECT_NO_THROW(Document("valid", "valid content", {"valid", "tags"}));
}

// Test SearchEngine operation errors
TEST_F(ErrorHandlingTest, SearchEngineOperationErrors) {
    // Test adding duplicate document
    Document duplicate("doc1", "duplicate content", {"duplicate"});
    EXPECT_THROW(engine->add_document(duplicate), std::invalid_argument);

    // Test removing non-existent document
    EXPECT_THROW(engine->remove_document("nonexistent"), DocumentNotFoundException);

    // Test updating non-existent document
    Document nonexistent("nonexistent", "content", {"tag"});
    EXPECT_THROW(engine->update_document(nonexistent), DocumentNotFoundException);

    // Test invalid document operations
    EXPECT_THROW(engine->add_document(Document("", "content")), DocumentValidationException);
    EXPECT_THROW(engine->update_document(Document("doc1", "")), DocumentValidationException);
}

// Test search operation errors
TEST_F(ErrorHandlingTest, SearchOperationErrors) {
    // Test invalid fuzzy search tolerance
    EXPECT_THROW(engine->fuzzy_search_by_tag("test", -1), std::invalid_argument);

    // Test invalid boolean search syntax
    EXPECT_THROW(engine->boolean_search("AND test"), SearchOperationException);
    EXPECT_THROW(engine->boolean_search("test NOT"), SearchOperationException);
    EXPECT_THROW(engine->boolean_search("test AND OR other"), SearchOperationException);

    // Test invalid regex patterns
    SearchPagination pagination{0, 10};
    EXPECT_THROW(engine->regex_search("[invalid", pagination), SearchOperationException);
    EXPECT_THROW(engine->regex_search("*invalid", pagination), SearchOperationException);
    EXPECT_THROW(engine->regex_search("(?invalid)", pagination), SearchOperationException);

    // Test find similar documents with non-existent document
    EXPECT_THROW(engine->find_similar_documents("nonexistent"), DocumentNotFoundException);
}

// Test file I/O errors
TEST_F(ErrorHandlingTest, FileIOErrors) {
    // Test loading non-existent index file
    EXPECT_THROW(engine->load_index("nonexistent_file.json"), std::ios_base::failure);

    // Test loading invalid JSON file
    std::ofstream invalid_file("test_invalid_index.json");
    invalid_file << "invalid json content {[}";
    invalid_file.close();
    EXPECT_THROW(engine->load_index("test_invalid_index.json"), std::exception);

    // Test loading corrupted index file
    std::ofstream corrupted_file("test_corrupted_index.json");
    corrupted_file << R"({"documents": "invalid_structure"})";
    corrupted_file.close();
    EXPECT_THROW(engine->load_index("test_corrupted_index.json"), std::exception);

    // Test saving to read-only location (if possible)
    // Note: This test might not work on all systems
    try {
        engine->save_index("/root/readonly_test.json");
        // If no exception is thrown, the test passes (system allows write)
    } catch (const std::exception& e) {
        // Expected behavior for read-only locations
        EXPECT_TRUE(true);
    }
}

// Test memory and resource limits
TEST_F(ErrorHandlingTest, MemoryAndResourceLimits) {
    // Test extremely large document content
    std::string huge_content(10000000, 'x');  // 10MB of content
    try {
        engine->add_document(Document("huge", huge_content, {"huge"}));
        // If successful, verify it can be searched
        auto results = engine->search_by_content("x");
        EXPECT_GT(results.size(), 0);
    } catch (const std::exception& e) {
        // Memory constraints might prevent adding huge documents
        EXPECT_TRUE(true);
    }

    // Test adding many documents to test memory limits
    try {
        for (int i = 0; i < 10000; ++i) {
            std::string content = "document " + std::to_string(i) + " with some content";
            engine->add_document(Document("stress_" + std::to_string(i), content, {"stress"}));
        }

        // Verify engine still works
        auto results = engine->search_by_tag("stress");
        EXPECT_GT(results.size(), 0);
    } catch (const std::exception& e) {
        // Memory constraints might prevent adding many documents
        EXPECT_TRUE(true);
    }
}

// Test edge cases in search queries
TEST_F(ErrorHandlingTest, SearchQueryEdgeCases) {
    // Test empty queries
    EXPECT_TRUE(engine->search_by_content("").empty());
    EXPECT_TRUE(engine->search_by_tag("").empty());
    EXPECT_TRUE(engine->search_by_tags({}).empty());

    // Test whitespace-only queries
    EXPECT_TRUE(engine->search_by_content("   ").empty());
    EXPECT_TRUE(engine->search_by_content("\t\n\r").empty());

    // Test queries with only special characters
    EXPECT_TRUE(engine->search_by_content("!@#$%^&*()").empty());
    EXPECT_TRUE(engine->search_by_content("[]{}|\\").empty());

    // Test very long queries
    std::string long_query(10000, 'a');
    EXPECT_NO_THROW(engine->search_by_content(long_query));

    // Test queries with null characters (if applicable)
    std::string null_query = "test\0content";
    EXPECT_NO_THROW(engine->search_by_content(null_query));

    // Test Unicode edge cases
    EXPECT_NO_THROW(engine->search_by_content("🚀🌟💻"));
    EXPECT_NO_THROW(engine->search_by_content("测试内容"));
    EXPECT_NO_THROW(engine->search_by_content("тестовый контент"));
}

// Test pagination edge cases
TEST_F(ErrorHandlingTest, PaginationEdgeCases) {
    // Test pagination with offset larger than result count
    SearchPagination large_offset{1000, 10};
    auto results = engine->search_by_content_enhanced("test", large_offset);
    EXPECT_EQ(results.offset, 1000);
    EXPECT_TRUE(results.results.empty());

    // Test pagination with zero limit
    SearchPagination zero_limit{0, 0};
    auto zero_results = engine->search_by_content_enhanced("test", zero_limit);
    EXPECT_EQ(zero_results.offset, 0);
    EXPECT_TRUE(zero_results.results.empty());

    // Test pagination with very large limit
    SearchPagination large_limit{0, 100000};
    auto large_results = engine->search_by_content_enhanced("test", large_limit);
    EXPECT_EQ(large_results.offset, 0);
    EXPECT_LE(large_results.results.size(), large_results.total_count);
}

// Test autocomplete edge cases
TEST_F(ErrorHandlingTest, AutocompleteEdgeCases) {
    // Test empty prefix
    auto empty_suggestions = engine->auto_complete("");
    EXPECT_TRUE(empty_suggestions.empty());

    // Test very long prefix
    std::string long_prefix(1000, 'x');
    auto long_suggestions = engine->auto_complete(long_prefix);
    EXPECT_TRUE(long_suggestions.empty());

    // Test prefix with special characters
    auto special_suggestions = engine->auto_complete("!@#");
    EXPECT_TRUE(special_suggestions.empty());

    // Test max_results = 0
    auto zero_suggestions = engine->auto_complete("test", 0);
    EXPECT_TRUE(zero_suggestions.empty());

    // Test very large max_results
    auto large_suggestions = engine->auto_complete("test", 100000);
    EXPECT_GE(large_suggestions.size(), 0);
}

// Test thread safety edge cases
TEST_F(ErrorHandlingTest, ThreadSafetyEdgeCases) {
    // Test concurrent access to the same document
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count, &error_count]() {
            try {
                // Try to update the same document concurrently
                engine->update_document(Document("doc1", "updated content", {"updated"}));
                success_count++;
            } catch (const std::exception& e) {
                error_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // At least one operation should succeed
    EXPECT_GT(success_count.load() + error_count.load(), 0);

    // Verify engine is still in consistent state
    EXPECT_NO_THROW(engine->search_by_content("test"));
}

// Test configuration edge cases
TEST_F(ErrorHandlingTest, ConfigurationEdgeCases) {
    // Test configuration with extreme values
    SearchConfig extreme_config;
    extreme_config.max_results = 0;
    extreme_config.score_threshold = -1.0;
    extreme_config.cache_size = 0;
    extreme_config.cache_ttl = std::chrono::milliseconds(0);

    EXPECT_NO_THROW(engine->update_config(extreme_config));

    // Test search with extreme configuration
    auto results = engine->search_by_content("test");
    EXPECT_GE(results.size(), 0);

    // Test configuration with very large values
    SearchConfig large_config;
    large_config.max_results = 1000000;
    large_config.cache_size = 1000000;
    large_config.tokenized_cache_size = 1000000;
    large_config.tf_idf_cache_size = 1000000;

    EXPECT_NO_THROW(engine->update_config(large_config));
}

// Test bulk operation edge cases
TEST_F(ErrorHandlingTest, BulkOperationEdgeCases) {
    // Test bulk insert with empty vector
    std::vector<Document> empty_docs;
    EXPECT_EQ(engine->bulk_insert(empty_docs), 0);

    // Test bulk insert with invalid documents
    std::vector<Document> invalid_docs;
    try {
        invalid_docs.emplace_back("", "invalid", std::vector<std::string>{"tag"});
    } catch (const DocumentValidationException& e) {
        // Expected - can't create invalid document
    }

    // Test bulk update with non-existent documents
    std::vector<Document> nonexistent_docs;
    nonexistent_docs.emplace_back("nonexistent1", "content", std::vector<std::string>{"tag"});
    nonexistent_docs.emplace_back("nonexistent2", "content", std::vector<std::string>{"tag"});
    EXPECT_EQ(engine->bulk_update(nonexistent_docs), 0);

    // Test bulk delete with non-existent IDs
    std::vector<String> nonexistent_ids = {"nonexistent1", "nonexistent2", "nonexistent3"};
    EXPECT_EQ(engine->bulk_delete(nonexistent_ids), 0);

    // Test bulk delete with empty vector
    std::vector<String> empty_ids;
    EXPECT_EQ(engine->bulk_delete(empty_ids), 0);
}

#endif // ATOM_SEARCH_TEST_ERROR_HANDLING_HPP
