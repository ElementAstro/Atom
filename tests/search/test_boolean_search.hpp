#ifndef ATOM_SEARCH_TEST_BOOLEAN_SEARCH_HPP
#define ATOM_SEARCH_TEST_BOOLEAN_SEARCH_HPP

#include <gtest/gtest.h>
#include <algorithm>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for enhanced boolean search functionality
class BooleanSearchTest : public ::testing::Test {
protected:
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        engine = std::make_unique<SearchEngine>(4);
        
        // Add test documents for boolean search
        engine->add_document(Document("doc1", "machine learning algorithms", {"ai", "ml", "tech"}));
        engine->add_document(Document("doc2", "deep learning neural networks", {"ai", "deep", "neural"}));
        engine->add_document(Document("doc3", "natural language processing", {"nlp", "ai", "language"}));
        engine->add_document(Document("doc4", "computer vision applications", {"cv", "vision", "tech"}));
        engine->add_document(Document("doc5", "data science analytics", {"data", "science", "analytics"}));
        engine->add_document(Document("doc6", "machine learning data science", {"ml", "data", "science"}));
    }
    
    // Helper function to get document IDs from search results
    std::vector<std::string> getDocumentIds(const std::vector<std::shared_ptr<Document>>& docs) {
        std::vector<std::string> ids;
        for (const auto& doc : docs) {
            ids.push_back(std::string(doc->get_id()));
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }
};

// Test boolean query parsing
TEST_F(BooleanSearchTest, BooleanQueryParsing) {
    // Test simple AND query
    auto query1 = engine->parse_boolean_query("machine AND learning");
    EXPECT_EQ(query1.terms.size(), 2);
    EXPECT_EQ(query1.operators.size(), 1);
    EXPECT_EQ(query1.terms[0], "machine");
    EXPECT_EQ(query1.terms[1], "learning");
    EXPECT_EQ(query1.operators[0], SearchEngine::BooleanQuery::Operator::AND);
    
    // Test simple OR query
    auto query2 = engine->parse_boolean_query("machine OR vision");
    EXPECT_EQ(query2.terms.size(), 2);
    EXPECT_EQ(query2.operators.size(), 1);
    EXPECT_EQ(query2.terms[0], "machine");
    EXPECT_EQ(query2.terms[1], "vision");
    EXPECT_EQ(query2.operators[0], SearchEngine::BooleanQuery::Operator::OR);
    
    // Test NOT query
    auto query3 = engine->parse_boolean_query("machine NOT vision");
    EXPECT_EQ(query3.terms.size(), 2);
    EXPECT_EQ(query3.operators.size(), 1);
    EXPECT_EQ(query3.terms[0], "machine");
    EXPECT_EQ(query3.terms[1], "vision");
    EXPECT_EQ(query3.operators[0], SearchEngine::BooleanQuery::Operator::NOT);
    
    // Test complex query
    auto query4 = engine->parse_boolean_query("machine AND learning OR data");
    EXPECT_EQ(query4.terms.size(), 3);
    EXPECT_EQ(query4.operators.size(), 2);
    EXPECT_EQ(query4.operators[0], SearchEngine::BooleanQuery::Operator::AND);
    EXPECT_EQ(query4.operators[1], SearchEngine::BooleanQuery::Operator::OR);
}

// Test boolean query parsing with quotes
TEST_F(BooleanSearchTest, BooleanQueryParsingWithQuotes) {
    auto query = engine->parse_boolean_query("\"machine learning\" AND data");
    EXPECT_EQ(query.terms.size(), 2);
    EXPECT_EQ(query.terms[0], "machine learning");  // Quotes should be removed
    EXPECT_EQ(query.terms[1], "data");
}

// Test boolean query parsing case insensitivity
TEST_F(BooleanSearchTest, BooleanQueryParsingCaseInsensitive) {
    auto query1 = engine->parse_boolean_query("machine and learning");
    auto query2 = engine->parse_boolean_query("machine AND learning");
    
    EXPECT_EQ(query1.operators.size(), query2.operators.size());
    EXPECT_EQ(query1.operators[0], query2.operators[0]);
    EXPECT_EQ(query1.operators[0], SearchEngine::BooleanQuery::Operator::AND);
}

// Test boolean query execution - AND operation
TEST_F(BooleanSearchTest, BooleanQueryExecutionAND) {
    auto query = engine->parse_boolean_query("machine AND learning");
    auto results = engine->execute_boolean_query(query);
    
    auto ids = getDocumentIds(results);
    
    // Should find documents containing both "machine" and "learning"
    // doc1: "machine learning algorithms" - contains both
    // doc6: "machine learning data science" - contains both
    std::vector<std::string> expected = {"doc1", "doc6"};
    std::sort(expected.begin(), expected.end());
    
    EXPECT_EQ(ids, expected);
}

// Test boolean query execution - OR operation
TEST_F(BooleanSearchTest, BooleanQueryExecutionOR) {
    auto query = engine->parse_boolean_query("machine OR vision");
    auto results = engine->execute_boolean_query(query);
    
    auto ids = getDocumentIds(results);
    
    // Should find documents containing either "machine" or "vision"
    // doc1: contains "machine"
    // doc4: contains "vision"  
    // doc6: contains "machine"
    std::vector<std::string> expected = {"doc1", "doc4", "doc6"};
    std::sort(expected.begin(), expected.end());
    
    EXPECT_EQ(ids, expected);
}

// Test boolean query execution - NOT operation
TEST_F(BooleanSearchTest, BooleanQueryExecutionNOT) {
    auto query = engine->parse_boolean_query("learning NOT deep");
    auto results = engine->execute_boolean_query(query);
    
    auto ids = getDocumentIds(results);
    
    // Should find documents containing "learning" but not "deep"
    // doc1: contains "learning" but not "deep"
    // doc6: contains "learning" but not "deep"
    // doc2: contains both "learning" and "deep" - should be excluded
    std::vector<std::string> expected = {"doc1", "doc6"};
    std::sort(expected.begin(), expected.end());
    
    EXPECT_EQ(ids, expected);
}

// Test complex boolean query
TEST_F(BooleanSearchTest, ComplexBooleanQuery) {
    auto query = engine->parse_boolean_query("machine AND learning OR data");
    auto results = engine->execute_boolean_query(query);
    
    auto ids = getDocumentIds(results);
    
    // Should find: (machine AND learning) OR data
    // doc1: machine AND learning - included
    // doc5: contains "data" - included
    // doc6: machine AND learning AND data - included
    std::vector<std::string> expected = {"doc1", "doc5", "doc6"};
    std::sort(expected.begin(), expected.end());
    
    EXPECT_EQ(ids, expected);
}

// Test boolean query with no results
TEST_F(BooleanSearchTest, BooleanQueryNoResults) {
    auto query = engine->parse_boolean_query("nonexistent AND term");
    auto results = engine->execute_boolean_query(query);
    
    EXPECT_TRUE(results.empty());
}

// Test boolean query with empty terms
TEST_F(BooleanSearchTest, BooleanQueryEmptyTerms) {
    SearchEngine::BooleanQuery empty_query;
    auto results = engine->execute_boolean_query(empty_query);
    
    EXPECT_TRUE(results.empty());
}

// Test boolean query with single term
TEST_F(BooleanSearchTest, BooleanQuerySingleTerm) {
    auto query = engine->parse_boolean_query("machine");
    auto results = engine->execute_boolean_query(query);
    
    auto ids = getDocumentIds(results);
    
    // Should find all documents containing "machine"
    std::vector<std::string> expected = {"doc1", "doc6"};
    std::sort(expected.begin(), expected.end());
    
    EXPECT_EQ(ids, expected);
}

// Test boolean search integration (using the main boolean_search method)
TEST_F(BooleanSearchTest, BooleanSearchIntegration) {
    auto results = engine->boolean_search("machine AND learning");
    
    EXPECT_FALSE(results.results.empty());
    EXPECT_GT(results.total_count, 0);
    EXPECT_GE(results.search_time_ms, 0.0);
    
    // Verify results contain expected documents
    bool found_doc1 = false;
    bool found_doc6 = false;
    
    for (const auto& result : results.results) {
        std::string doc_id = std::string(result.document->get_id());
        if (doc_id == "doc1") found_doc1 = true;
        if (doc_id == "doc6") found_doc6 = true;
    }
    
    EXPECT_TRUE(found_doc1);
    EXPECT_TRUE(found_doc6);
}

// Test boolean search with pagination
TEST_F(BooleanSearchTest, BooleanSearchWithPagination) {
    SearchPagination pagination{0, 1};  // Get only first result
    auto results = engine->boolean_search("machine OR learning OR data", pagination);
    
    EXPECT_LE(results.results.size(), 1);
    EXPECT_EQ(results.offset, 0);
    
    if (!results.results.empty()) {
        EXPECT_NE(results.results[0].document, nullptr);
        EXPECT_GE(results.results[0].score, 0.0);
    }
}

// Test boolean search performance
TEST_F(BooleanSearchTest, BooleanSearchPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    auto results = engine->boolean_search("machine AND learning OR data NOT vision");
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 1000);
    EXPECT_GT(results.search_time_ms, 0.0);
    EXPECT_LT(results.search_time_ms, 1000.0);
}

// Test boolean query operator precedence
TEST_F(BooleanSearchTest, BooleanQueryOperatorPrecedence) {
    // Test that operators are applied left-to-right as parsed
    auto query1 = engine->parse_boolean_query("a AND b OR c");
    auto results1 = engine->execute_boolean_query(query1);
    
    auto query2 = engine->parse_boolean_query("a OR b AND c");
    auto results2 = engine->execute_boolean_query(query2);
    
    // Results might be different due to different operator precedence
    // This test mainly ensures the parsing and execution don't crash
    EXPECT_NO_THROW(engine->execute_boolean_query(query1));
    EXPECT_NO_THROW(engine->execute_boolean_query(query2));
}

// Test boolean search with special characters in terms
TEST_F(BooleanSearchTest, BooleanSearchSpecialCharacters) {
    // Add document with special characters
    engine->add_document(Document("special", "C++ programming language", {"cpp", "programming"}));
    
    // Search should handle terms with special characters
    auto results = engine->boolean_search("programming AND language");
    
    bool found_special = false;
    for (const auto& result : results.results) {
        if (std::string(result.document->get_id()) == "special") {
            found_special = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_special);
}

#endif // ATOM_SEARCH_TEST_BOOLEAN_SEARCH_HPP
