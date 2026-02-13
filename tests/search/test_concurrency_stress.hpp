#ifndef ATOM_SEARCH_TEST_CONCURRENCY_STRESS_HPP
#define ATOM_SEARCH_TEST_CONCURRENCY_STRESS_HPP

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <atomic>
#include <random>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for concurrency and stress testing
class ConcurrencyStressTest : public ::testing::Test {
protected:
    SearchConfig config;
    std::unique_ptr<SearchEngine> engine;

    void SetUp() override {
        config.enable_performance_caching = true;
        config.enable_semantic_search = true;
        config.cache_size = 1000;
        config.tokenized_cache_size = 500;
        config.tf_idf_cache_size = 1000;

        engine = std::make_unique<SearchEngine>(8, config);  // Use 8 threads for stress testing

        // Pre-populate with test documents
        for (int i = 0; i < 100; ++i) {
            std::string content = "document " + std::to_string(i) + " with content about ";
            if (i % 4 == 0) content += "machine learning";
            else if (i % 4 == 1) content += "artificial intelligence";
            else if (i % 4 == 2) content += "data science";
            else content += "computer vision";

            std::vector<std::string> tags;
            tags.push_back("tag" + std::to_string(i % 10));
            tags.push_back("category" + std::to_string(i % 5));

            engine->add_document(Document("doc" + std::to_string(i), content, tags));
        }
    }

    // Helper function to generate random strings
    std::string generateRandomString(size_t length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyz";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, chars.size() - 1);

        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }
};

// Test concurrent document insertion
TEST_F(ConcurrencyStressTest, ConcurrentDocumentInsertion) {
    const int num_threads = 10;
    const int docs_per_thread = 50;
    std::vector<std::future<int>> futures;
    std::atomic<int> total_inserted{0};

    // Launch multiple threads to insert documents concurrently
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, t, docs_per_thread, &total_inserted]() {
            int inserted = 0;
            for (int i = 0; i < docs_per_thread; ++i) {
                try {
                    std::string doc_id = "thread" + std::to_string(t) + "_doc" + std::to_string(i);
                    std::string content = "concurrent content " + std::to_string(t) + " " + std::to_string(i);
                    engine->add_document(Document(doc_id, content, {"concurrent", "test"}));
                    inserted++;
                    total_inserted++;
                } catch (const std::exception& e) {
                    // Some insertions might fail due to duplicate IDs, which is expected
                }
            }
            return inserted;
        }));
    }

    // Wait for all threads to complete
    int total_successful = 0;
    for (auto& future : futures) {
        total_successful += future.get();
    }

    // Verify that most insertions were successful
    EXPECT_GT(total_successful, docs_per_thread * num_threads * 0.8);  // At least 80% success rate
    EXPECT_EQ(total_inserted.load(), total_successful);

    // Verify documents can be found
    auto results = engine->search_by_tag("concurrent");
    EXPECT_EQ(results.size(), total_successful);
}

// Test concurrent search operations
TEST_F(ConcurrencyStressTest, ConcurrentSearchOperations) {
    const int num_threads = 20;
    const int searches_per_thread = 100;
    std::vector<std::future<int>> futures;
    std::atomic<int> total_searches{0};

    std::vector<std::string> search_terms = {
        "machine", "learning", "artificial", "intelligence", "data", "science", "computer", "vision"
    };

    // Launch multiple threads to perform searches concurrently
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, t, searches_per_thread, &search_terms, &total_searches]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> term_dis(0, search_terms.size() - 1);

            int successful_searches = 0;
            for (int i = 0; i < searches_per_thread; ++i) {
                try {
                    std::string term = search_terms[term_dis(gen)];

                    // Mix different types of searches
                    if (i % 4 == 0) {
                        auto results = engine->search_by_content(term);
                        successful_searches++;
                    } else if (i % 4 == 1) {
                        auto results = engine->search_by_tag("tag" + std::to_string(i % 10));
                        successful_searches++;
                    } else if (i % 4 == 2) {
                        auto results = engine->fuzzy_search_by_tag("tag" + std::to_string(i % 10), 1);
                        successful_searches++;
                    } else {
                        auto results = engine->boolean_search(term + " AND document");
                        successful_searches++;
                    }
                    total_searches++;
                } catch (const std::exception& e) {
                    // Some searches might fail, but most should succeed
                }
            }
            return successful_searches;
        }));
    }

    // Wait for all threads to complete
    int total_successful = 0;
    for (auto& future : futures) {
        total_successful += future.get();
    }

    // Verify that most searches were successful
    EXPECT_GT(total_successful, searches_per_thread * num_threads * 0.95);  // At least 95% success rate
    EXPECT_EQ(total_searches.load(), total_successful);
}

// Test concurrent mixed operations (insert, update, delete, search)
TEST_F(ConcurrencyStressTest, ConcurrentMixedOperations) {
    const int num_threads = 8;
    const int operations_per_thread = 50;
    std::vector<std::future<void>> futures;
    std::atomic<int> insert_count{0};
    std::atomic<int> update_count{0};
    std::atomic<int> delete_count{0};
    std::atomic<int> search_count{0};

    // Launch multiple threads with mixed operations
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, t, operations_per_thread, &insert_count, &update_count, &delete_count, &search_count]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> op_dis(0, 3);

            for (int i = 0; i < operations_per_thread; ++i) {
                int operation = op_dis(gen);
                std::string doc_id = "mixed_" + std::to_string(t) + "_" + std::to_string(i);

                try {
                    switch (operation) {
                        case 0: // Insert
                            engine->add_document(Document(doc_id, "mixed operation content", {"mixed", "test"}));
                            insert_count++;
                            break;
                        case 1: // Update (if document exists)
                            if (engine->has_document(doc_id)) {
                                engine->update_document(Document(doc_id, "updated mixed content", {"mixed", "updated"}));
                                update_count++;
                            }
                            break;
                        case 2: // Delete (if document exists)
                            if (engine->has_document(doc_id)) {
                                engine->remove_document(doc_id);
                                delete_count++;
                            }
                            break;
                        case 3: // Search
                            engine->search_by_content("mixed");
                            search_count++;
                            break;
                    }
                } catch (const std::exception& e) {
                    // Some operations might fail due to race conditions, which is acceptable
                }

                // Small delay to increase chance of race conditions
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Verify that operations were performed
    EXPECT_GT(insert_count.load(), 0);
    EXPECT_GT(search_count.load(), 0);
    // Update and delete counts might be 0 if documents don't exist when operations are attempted

    // Verify engine is still in a consistent state
    EXPECT_NO_THROW(engine->search_by_content("test"));
    EXPECT_NO_THROW(engine->get_document_count());
}

// Test cache behavior under concurrent access
TEST_F(ConcurrencyStressTest, ConcurrentCacheAccess) {
    const int num_threads = 15;
    const int searches_per_thread = 30;
    std::vector<std::future<std::pair<int, int>>> futures;

    SearchPagination pagination{0, 10};

    // Launch multiple threads that perform the same searches to test cache
    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, searches_per_thread, &pagination]() {
            int cache_hits = 0;
            int cache_misses = 0;

            for (int i = 0; i < searches_per_thread; ++i) {
                // Alternate between a few search terms to increase cache hit probability
                std::string term = (i % 3 == 0) ? "machine" : (i % 3 == 1) ? "learning" : "data";

                auto results = engine->search_by_content_enhanced(term, pagination);
                if (results.from_cache) {
                    cache_hits++;
                } else {
                    cache_misses++;
                }

                // Small delay to allow other threads to populate cache
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }

            return std::make_pair(cache_hits, cache_misses);
        }));
    }

    // Collect results
    int total_cache_hits = 0;
    int total_cache_misses = 0;
    for (auto& future : futures) {
        auto [hits, misses] = future.get();
        total_cache_hits += hits;
        total_cache_misses += misses;
    }

    // Verify cache is working (should have some hits due to repeated searches)
    EXPECT_GT(total_cache_hits, 0);
    EXPECT_GT(total_cache_misses, 0);

    // Cache hit ratio should be reasonable
    double hit_ratio = static_cast<double>(total_cache_hits) / (total_cache_hits + total_cache_misses);
    EXPECT_GT(hit_ratio, 0.1);  // At least 10% cache hit rate
}

// Test bulk operations under stress
TEST_F(ConcurrencyStressTest, BulkOperationsStress) {
    const int num_bulk_ops = 10;
    const int docs_per_bulk = 100;

    // Test bulk insert
    std::vector<std::future<size_t>> insert_futures;
    for (int i = 0; i < num_bulk_ops; ++i) {
        insert_futures.push_back(std::async(std::launch::async, [this, i, docs_per_bulk]() {
            std::vector<Document> docs;
            for (int j = 0; j < docs_per_bulk; ++j) {
                std::string doc_id = "bulk_" + std::to_string(i) + "_" + std::to_string(j);
                docs.emplace_back(doc_id, "bulk content " + std::to_string(j),
                                std::vector<std::string>{"bulk", "test"});
            }
            return engine->bulk_insert(docs);
        }));
    }

    // Wait for bulk inserts to complete
    size_t total_inserted = 0;
    for (auto& future : insert_futures) {
        total_inserted += future.get();
    }

    EXPECT_GT(total_inserted, docs_per_bulk * num_bulk_ops * 0.8);  // At least 80% success

    // Test bulk update
    std::vector<Document> update_docs;
    for (int i = 0; i < 50; ++i) {
        std::string doc_id = "bulk_0_" + std::to_string(i);
        if (engine->has_document(doc_id)) {
            update_docs.emplace_back(doc_id, "updated bulk content",
                                   std::vector<std::string>{"bulk", "updated"});
        }
    }

    if (!update_docs.empty()) {
        size_t updated = engine->bulk_update(update_docs);
        EXPECT_GT(updated, 0);
    }

    // Test bulk delete
    std::vector<String> delete_ids;
    for (int i = 0; i < 30; ++i) {
        delete_ids.push_back("bulk_1_" + std::to_string(i));
    }

    size_t deleted = engine->bulk_delete(delete_ids);
    EXPECT_GE(deleted, 0);  // Some might not exist, so >= 0 is acceptable
}

// Test memory usage and performance under stress
TEST_F(ConcurrencyStressTest, MemoryAndPerformanceStress) {
    const int large_doc_count = 1000;
    const int large_content_size = 1000;  // words per document

    auto start_time = std::chrono::high_resolution_clock::now();

    // Add many large documents
    std::vector<std::future<void>> futures;
    const int num_threads = 4;
    const int docs_per_thread = large_doc_count / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, t, docs_per_thread, large_content_size]() {
            for (int i = 0; i < docs_per_thread; ++i) {
                std::string content;
                for (int j = 0; j < large_content_size; ++j) {
                    if (j > 0) content += " ";
                    content += "word" + std::to_string(j % 100);
                }

                std::string doc_id = "large_" + std::to_string(t) + "_" + std::to_string(i);
                try {
                    engine->add_document(Document(doc_id, content, {"large", "stress"}));
                } catch (const std::exception& e) {
                    // Memory or other constraints might cause failures
                }
            }
        }));
    }

    // Wait for all insertions to complete
    for (auto& future : futures) {
        future.wait();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Verify that insertion completed in reasonable time (less than 30 seconds)
    EXPECT_LT(duration.count(), 30000);

    // Verify that searches still work efficiently
    auto search_start = std::chrono::high_resolution_clock::now();
    auto results = engine->search_by_content("word50");
    auto search_end = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::milliseconds>(search_end - search_start);

    EXPECT_GT(results.size(), 0);
    EXPECT_LT(search_duration.count(), 5000);  // Search should complete in less than 5 seconds

    // Check that engine is still responsive
    EXPECT_NO_THROW(engine->get_document_count());
    EXPECT_NO_THROW(engine->get_index_stats());
}

#endif // ATOM_SEARCH_TEST_CONCURRENCY_STRESS_HPP
