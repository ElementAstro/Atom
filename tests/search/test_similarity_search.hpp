#ifndef ATOM_SEARCH_TEST_SIMILARITY_SEARCH_HPP
#define ATOM_SEARCH_TEST_SIMILARITY_SEARCH_HPP

#include <gtest/gtest.h>
#include <cmath>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for similarity and semantic search features
class SimilaritySearchTest : public ::testing::Test {
protected:
    SearchConfig config;
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        config.enable_semantic_search = true;
        config.use_cosine_similarity = true;
        config.min_similarity_threshold = 0.01;

        engine = std::make_unique<SearchEngine>(4, config);

        // Add documents with varying similarity
        engine->add_document(Document("ai1", "machine learning artificial intelligence", {"ai", "ml"}));
        engine->add_document(Document("ai2", "deep learning neural networks artificial intelligence", {"ai", "deep"}));
        engine->add_document(Document("ai3", "natural language processing artificial intelligence", {"ai", "nlp"}));
        engine->add_document(Document("sports1", "football soccer basketball sports", {"sports", "ball"}));
        engine->add_document(Document("sports2", "tennis golf swimming sports activities", {"sports", "individual"}));
        engine->add_document(Document("tech1", "software development programming coding", {"tech", "dev"}));
        engine->add_document(Document("tech2", "web development javascript python programming", {"tech", "web"}));
    }
};

// Test cosine similarity calculation
TEST_F(SimilaritySearchTest, CosineSimilarityCalculation) {
    // Get documents for similarity testing
    auto doc1 = std::make_shared<Document>("test1", "machine learning algorithms", std::initializer_list<std::string>{"test"});
    auto doc2 = std::make_shared<Document>("test2", "machine learning models", std::initializer_list<std::string>{"test"});
    auto doc3 = std::make_shared<Document>("test3", "football soccer games", std::initializer_list<std::string>{"test"});

    // Calculate similarities
    double sim_similar = engine->calculate_cosine_similarity(*doc1, *doc2);
    double sim_different = engine->calculate_cosine_similarity(*doc1, *doc3);
    double sim_self = engine->calculate_cosine_similarity(*doc1, *doc1);

    // Similar documents should have higher similarity
    EXPECT_GT(sim_similar, sim_different);

    // Self-similarity should be 1.0 (or very close due to floating point)
    EXPECT_NEAR(sim_self, 1.0, 0.001);

    // All similarities should be between 0 and 1
    EXPECT_GE(sim_similar, 0.0);
    EXPECT_LE(sim_similar, 1.0);
    EXPECT_GE(sim_different, 0.0);
    EXPECT_LE(sim_different, 1.0);
}

// Test Jaccard similarity calculation
TEST_F(SimilaritySearchTest, JaccardSimilarityCalculation) {
    auto doc1 = std::make_shared<Document>("test1", "machine learning algorithms", std::initializer_list<std::string>{"test"});
    auto doc2 = std::make_shared<Document>("test2", "machine learning models", std::initializer_list<std::string>{"test"});
    auto doc3 = std::make_shared<Document>("test3", "football soccer games", std::initializer_list<std::string>{"test"});

    double jaccard_similar = engine->calculate_jaccard_similarity(*doc1, *doc2);
    double jaccard_different = engine->calculate_jaccard_similarity(*doc1, *doc3);
    double jaccard_self = engine->calculate_jaccard_similarity(*doc1, *doc1);

    // Similar documents should have higher Jaccard similarity
    EXPECT_GT(jaccard_similar, jaccard_different);

    // Self-similarity should be 1.0
    EXPECT_NEAR(jaccard_self, 1.0, 0.001);

    // All similarities should be between 0 and 1
    EXPECT_GE(jaccard_similar, 0.0);
    EXPECT_LE(jaccard_similar, 1.0);
    EXPECT_GE(jaccard_different, 0.0);
    EXPECT_LE(jaccard_different, 1.0);
}

// Test TF vector creation
TEST_F(SimilaritySearchTest, TFVectorCreation) {
    auto doc = std::make_shared<Document>("test", "machine learning machine algorithms", std::initializer_list<std::string>{"test"});

    auto tf_vector = engine->create_tf_vector(*doc);

    // Should contain all unique terms
    EXPECT_TRUE(tf_vector.find(String("machine")) != tf_vector.end());
    EXPECT_TRUE(tf_vector.find(String("learning")) != tf_vector.end());
    EXPECT_TRUE(tf_vector.find(String("algorithms")) != tf_vector.end());

    // "machine" appears twice, so should have higher TF
    double tf_machine = tf_vector[String("machine")];
    double tf_learning = tf_vector[String("learning")];
    double tf_algorithms = tf_vector[String("algorithms")];

    EXPECT_GT(tf_machine, tf_learning);  // "machine" appears more frequently
    EXPECT_EQ(tf_learning, tf_algorithms);  // Both appear once

    // All TF values should be positive
    EXPECT_GT(tf_machine, 0.0);
    EXPECT_GT(tf_learning, 0.0);
    EXPECT_GT(tf_algorithms, 0.0);
}

// Test finding similar documents
TEST_F(SimilaritySearchTest, FindSimilarDocuments) {
    // Find documents similar to an AI document
    auto similar_docs = engine->find_similar_documents("ai1", 3, 0.1);

    // Should find other AI documents as similar
    EXPECT_GT(similar_docs.size(), 0);
    EXPECT_LE(similar_docs.size(), 3);  // Respects max_results

    // Results should be sorted by similarity (descending)
    for (size_t i = 1; i < similar_docs.size(); ++i) {
        EXPECT_GE(similar_docs[i-1].second, similar_docs[i].second);
    }

    // All similarities should be above threshold
    for (const auto& [doc, similarity] : similar_docs) {
        EXPECT_GE(similarity, 0.1);
        EXPECT_LE(similarity, 1.0);
    }

    // Should not include the reference document itself
    for (const auto& [doc, similarity] : similar_docs) {
        EXPECT_NE(doc->get_id(), "ai1");
    }
}

// Test finding similar documents with high threshold
TEST_F(SimilaritySearchTest, FindSimilarDocumentsHighThreshold) {
    // Use very high threshold - should return fewer or no results
    auto similar_docs = engine->find_similar_documents("ai1", 10, 0.9);

    // With high threshold, might not find any similar documents
    // All returned documents should meet the threshold
    for (const auto& [doc, similarity] : similar_docs) {
        EXPECT_GE(similarity, 0.9);
    }
}

// Test finding similar documents for non-existent document
TEST_F(SimilaritySearchTest, FindSimilarDocumentsNonExistent) {
    EXPECT_THROW(engine->find_similar_documents("nonexistent", 5, 0.1),
                 DocumentNotFoundException);
}

// Test semantic search
TEST_F(SimilaritySearchTest, SemanticSearch) {
    SearchPagination pagination{0, 5};

    // Search for AI-related content
    auto results = engine->semantic_search("artificial intelligence machine learning", pagination);

    EXPECT_GT(results.total_count, 0);
    EXPECT_FALSE(results.results.empty());
    EXPECT_EQ(results.offset, 0);
    EXPECT_GT(results.search_time_ms, 0.0);
    EXPECT_FALSE(results.from_cache);

    // Results should be sorted by similarity score (descending)
    for (size_t i = 1; i < results.results.size(); ++i) {
        EXPECT_GE(results.results[i-1].score, results.results[i].score);
    }

    // All scores should be positive
    for (const auto& result : results.results) {
        EXPECT_GT(result.score, 0.0);
    }

    // AI documents should rank higher for AI query
    bool found_ai_doc = false;
    for (const auto& result : results.results) {
        std::string doc_id = std::string(result.document->get_id());
        if (doc_id.find("ai") == 0) {  // Starts with "ai"
            found_ai_doc = true;
            break;
        }
    }
    EXPECT_TRUE(found_ai_doc);
}

// Test semantic search with pagination
TEST_F(SimilaritySearchTest, SemanticSearchPagination) {
    SearchPagination pagination{1, 2};  // Skip first result, get 2 results

    auto results = engine->semantic_search("programming development", pagination);

    EXPECT_EQ(results.offset, 1);
    EXPECT_LE(results.results.size(), 2);  // Should respect limit

    // If there are results, they should still be valid
    for (const auto& result : results.results) {
        EXPECT_GT(result.score, 0.0);
        EXPECT_NE(result.document, nullptr);
    }
}

// Test semantic search with empty query
TEST_F(SimilaritySearchTest, SemanticSearchEmptyQuery) {
    auto results = engine->semantic_search("", {});

    // Empty query should return no results or very low scores
    if (!results.results.empty()) {
        for (const auto& result : results.results) {
            EXPECT_GE(result.score, 0.0);
        }
    }
}

// Test semantic search performance
TEST_F(SimilaritySearchTest, SemanticSearchPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    auto results = engine->semantic_search("machine learning algorithms", {});
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Search should complete in reasonable time (less than 1 second for small dataset)
    EXPECT_LT(duration.count(), 1000);

    // Reported search time should be reasonable
    EXPECT_GT(results.search_time_ms, 0.0);
    EXPECT_LT(results.search_time_ms, 1000.0);
}

// Test similarity calculations with edge cases
TEST_F(SimilaritySearchTest, SimilarityEdgeCases) {
    // Empty documents
    auto empty_doc1 = std::make_shared<Document>("empty1", "   ", std::initializer_list<std::string>{"test"});
    auto empty_doc2 = std::make_shared<Document>("empty2", "   ", std::initializer_list<std::string>{"test"});
    auto normal_doc = std::make_shared<Document>("normal", "machine learning", std::initializer_list<std::string>{"test"});

    // Similarity between empty documents should be 0
    double empty_similarity = engine->calculate_cosine_similarity(*empty_doc1, *empty_doc2);
    EXPECT_EQ(empty_similarity, 0.0);

    // Similarity between empty and normal document should be 0
    double empty_normal_similarity = engine->calculate_cosine_similarity(*empty_doc1, *normal_doc);
    EXPECT_EQ(empty_normal_similarity, 0.0);

    // Single word documents
    auto single1 = std::make_shared<Document>("single1", "machine", std::initializer_list<std::string>{"test"});
    auto single2 = std::make_shared<Document>("single2", "machine", std::initializer_list<std::string>{"test"});
    auto single3 = std::make_shared<Document>("single3", "learning", std::initializer_list<std::string>{"test"});

    // Identical single words should have similarity 1.0
    double identical_single = engine->calculate_cosine_similarity(*single1, *single2);
    EXPECT_NEAR(identical_single, 1.0, 0.001);

    // Different single words should have similarity 0.0
    double different_single = engine->calculate_cosine_similarity(*single1, *single3);
    EXPECT_EQ(different_single, 0.0);
}

#endif // ATOM_SEARCH_TEST_SIMILARITY_SEARCH_HPP
