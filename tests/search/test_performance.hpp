#ifndef ATOM_SEARCH_TEST_PERFORMANCE_HPP
#define ATOM_SEARCH_TEST_PERFORMANCE_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <sstream>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for performance testing
class SearchPerformanceTest : public ::testing::Test {
protected:
    std::unique_ptr<SearchEngine> engine;
    std::vector<Document> test_documents;

    void SetUp() override {
        SearchConfig config;
        config.enable_performance_caching = true;
        config.tokenized_cache_size = 1000;
        config.tf_idf_cache_size = 2000;

        engine = std::make_unique<SearchEngine>(8, config);
        generateTestDocuments(1000);  // Generate 1000 test documents
    }

    void generateTestDocuments(size_t count) {
        std::vector<std::string> words = {
            "machine", "learning", "artificial", "intelligence", "neural", "network",
            "deep", "algorithm", "data", "science", "computer", "vision", "natural",
            "language", "processing", "programming", "software", "development",
            "technology", "innovation", "research", "analysis", "optimization",
            "performance", "scalability", "efficiency", "automation", "robotics",
            "classification", "regression", "clustering", "prediction", "model",
            "training", "testing", "validation", "accuracy", "precision", "recall"
        };

        std::vector<std::string> tags = {
            "ai", "ml", "tech", "science", "research", "development", "programming",
            "data", "analysis", "optimization", "performance", "innovation"
        };

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> word_dist(0, words.size() - 1);
        std::uniform_int_distribution<> tag_dist(0, tags.size() - 1);
        std::uniform_int_distribution<> content_length(5, 20);
        std::uniform_int_distribution<> tag_count(1, 5);

        test_documents.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            std::ostringstream content;
            int num_words = content_length(gen);

            for (int j = 0; j < num_words; ++j) {
                if (j > 0) content << " ";
                content << words[word_dist(gen)];
            }

            std::vector<std::string> doc_tags;
            int num_tags = tag_count(gen);
            for (int j = 0; j < num_tags; ++j) {
                doc_tags.push_back(tags[tag_dist(gen)]);
            }

            test_documents.emplace_back(
                "doc" + std::to_string(i),
                content.str(),
                doc_tags
            );
        }
    }

    double measureTime(std::function<void()> func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

// Test bulk insertion performance
TEST_F(SearchPerformanceTest, BulkInsertionPerformance) {
    auto time_ms = measureTime([this]() {
        engine->bulk_insert(test_documents);
    });

    // Should insert 1000 documents in reasonable time (less than 5 seconds)
    EXPECT_LT(time_ms, 5000.0);

    // Verify all documents were inserted
    EXPECT_EQ(engine->get_document_count(), test_documents.size());

    std::cout << "Bulk insertion of " << test_documents.size()
              << " documents took " << time_ms << " ms" << std::endl;
}

// Test individual insertion performance
TEST_F(SearchPerformanceTest, IndividualInsertionPerformance) {
    auto time_ms = measureTime([this]() {
        for (const auto& doc : test_documents) {
            engine->add_document(doc);
        }
    });

    // Individual insertions should be slower than bulk but still reasonable
    EXPECT_LT(time_ms, 10000.0);  // Less than 10 seconds

    std::cout << "Individual insertion of " << test_documents.size()
              << " documents took " << time_ms << " ms" << std::endl;
}

// Test search performance with caching
TEST_F(SearchPerformanceTest, SearchPerformanceWithCaching) {
    // First, insert all documents
    engine->bulk_insert(test_documents);

    // Measure first search (cold cache)
    auto first_search_time = measureTime([this]() {
        auto results = engine->search_by_content("machine learning");
    });

    // Measure second identical search (warm cache)
    auto second_search_time = measureTime([this]() {
        auto results = engine->search_by_content("machine learning");
    });

    // Both searches should complete quickly
    EXPECT_LT(first_search_time, 1000.0);   // Less than 1 second
    EXPECT_LT(second_search_time, 1000.0);  // Less than 1 second

    std::cout << "First search took " << first_search_time << " ms" << std::endl;
    std::cout << "Second search took " << second_search_time << " ms" << std::endl;

    // Second search might be faster due to caching (though not guaranteed in all cases)
    // Just verify both are reasonable
}

// Test concurrent search performance
TEST_F(SearchPerformanceTest, ConcurrentSearchPerformance) {
    // Insert documents first
    engine->bulk_insert(test_documents);

    const int num_threads = 8;
    const int searches_per_thread = 10;

    auto time_ms = measureTime([this, num_threads, searches_per_thread]() {
        std::vector<std::thread> threads;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([this, searches_per_thread, i]() {
                for (int j = 0; j < searches_per_thread; ++j) {
                    std::string query = "machine learning " + std::to_string(i * searches_per_thread + j);
                    auto results = engine->search_by_content(query);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    });

    int total_searches = num_threads * searches_per_thread;
    double avg_time_per_search = time_ms / total_searches;

    EXPECT_LT(time_ms, 10000.0);  // Total time less than 10 seconds
    EXPECT_LT(avg_time_per_search, 100.0);  // Average less than 100ms per search

    std::cout << "Concurrent searches: " << total_searches << " searches in "
              << time_ms << " ms (avg: " << avg_time_per_search << " ms/search)" << std::endl;
}

// Test semantic search performance
TEST_F(SearchPerformanceTest, SemanticSearchPerformance) {
    // Insert documents first
    engine->bulk_insert(test_documents);

    auto time_ms = measureTime([this]() {
        auto results = engine->semantic_search("artificial intelligence machine learning", {});
    });

    // Semantic search is more expensive but should still be reasonable
    EXPECT_LT(time_ms, 5000.0);  // Less than 5 seconds

    std::cout << "Semantic search took " << time_ms << " ms" << std::endl;
}

// Test boolean search performance
TEST_F(SearchPerformanceTest, BooleanSearchPerformance) {
    // Insert documents first
    engine->bulk_insert(test_documents);

    auto time_ms = measureTime([this]() {
        auto results = engine->boolean_search("machine AND learning OR data NOT vision");
    });

    EXPECT_LT(time_ms, 2000.0);  // Less than 2 seconds

    std::cout << "Boolean search took " << time_ms << " ms" << std::endl;
}

// Test similarity search performance
TEST_F(SearchPerformanceTest, SimilaritySearchPerformance) {
    // Insert documents first
    engine->bulk_insert(test_documents);

    auto time_ms = measureTime([this]() {
        auto results = engine->find_similar_documents("doc0", 10, 0.1);
    });

    // Similarity search is expensive but should be reasonable for small datasets
    EXPECT_LT(time_ms, 10000.0);  // Less than 10 seconds

    std::cout << "Similarity search took " << time_ms << " ms" << std::endl;
}

// Test cache performance and hit rates
TEST_F(SearchPerformanceTest, CachePerformanceAnalysis) {
    // Insert documents first
    engine->bulk_insert(test_documents);

    // Perform multiple searches to populate caches
    std::vector<std::string> queries = {
        "machine learning", "artificial intelligence", "data science",
        "neural networks", "computer vision", "natural language"
    };

    // First round - populate caches
    for (const auto& query : queries) {
        engine->search_by_content(query);
    }

    // Second round - should hit caches
    auto cached_search_time = measureTime([this, &queries]() {
        for (const auto& query : queries) {
            engine->search_by_content(query);
        }
    });

    // Get cache statistics
    auto stats = engine->get_index_stats();

    EXPECT_GT(stats["performance_cache_hits"], 0);
    EXPECT_LT(cached_search_time, 1000.0);  // Cached searches should be fast

    std::cout << "Cache hits: " << stats["performance_cache_hits"] << std::endl;
    std::cout << "Cache misses: " << stats["performance_cache_misses"] << std::endl;
    std::cout << "Cached searches took " << cached_search_time << " ms" << std::endl;
}

// Test memory usage and optimization
TEST_F(SearchPerformanceTest, MemoryOptimizationTest) {
    // Insert documents
    engine->bulk_insert(test_documents);

    // Get initial stats
    auto initial_stats = engine->get_index_stats();

    // Perform many searches to populate caches
    for (int i = 0; i < 100; ++i) {
        std::string query = "test query " + std::to_string(i);
        engine->search_by_content(query);
    }

    // Get stats after cache population
    auto populated_stats = engine->get_index_stats();

    // Optimize index (should clean up caches if they're too large)
    engine->optimize_index();

    // Get stats after optimization
    auto optimized_stats = engine->get_index_stats();

    // Verify caches were populated
    EXPECT_GT(populated_stats["tokenized_cache_entries"], 0);
    EXPECT_GT(populated_stats["tf_idf_cache_entries"], 0);

    // After optimization, cache sizes should be reasonable
    EXPECT_LE(optimized_stats["tokenized_cache_entries"], populated_stats["tokenized_cache_entries"]);

    std::cout << "Initial tokenized cache: " << initial_stats["tokenized_cache_entries"] << std::endl;
    std::cout << "Populated tokenized cache: " << populated_stats["tokenized_cache_entries"] << std::endl;
    std::cout << "Optimized tokenized cache: " << optimized_stats["tokenized_cache_entries"] << std::endl;
}

// Test scalability with different document counts
TEST_F(SearchPerformanceTest, ScalabilityTest) {
    std::vector<size_t> document_counts = {100, 500, 1000};

    for (size_t count : document_counts) {
        // Create fresh engine for each test
        SearchEngine test_engine(4);

        // Use subset of documents
        std::vector<Document> subset(test_documents.begin(), test_documents.begin() + count);

        // Measure insertion time
        auto insert_time = measureTime([&test_engine, &subset]() {
            test_engine.bulk_insert(subset);
        });

        // Measure search time
        auto search_time = measureTime([&test_engine]() {
            auto results = test_engine.search_by_content("machine learning");
        });

        std::cout << "Documents: " << count
                  << ", Insert time: " << insert_time << " ms"
                  << ", Search time: " << search_time << " ms" << std::endl;

        // Verify reasonable performance scaling
        EXPECT_LT(insert_time, count * 10.0);  // Should be roughly linear or better
        EXPECT_LT(search_time, 1000.0);       // Search time should remain reasonable
    }
}

#endif // ATOM_SEARCH_TEST_PERFORMANCE_HPP
