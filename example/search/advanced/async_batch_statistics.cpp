/**
 * @file async_batch_statistics.cpp
 * @brief Advanced example demonstrating async operations, batch processing, and
 * statistics
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - Asynchronous search operations with futures
 * - Batch processing for high-throughput scenarios
 * - Real-time statistics monitoring and reporting
 * - Performance optimization techniques
 * - Concurrent cache operations
 * - Load balancing and resource management
 * - Error handling in async environments
 * - Memory usage optimization
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/lru.hpp"
#include "atom/search/search.hpp"
#include "atom/search/ttl.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Performance metrics collector
class PerformanceMetrics {
private:
    std::atomic<size_t> total_operations_{0};
    std::atomic<size_t> successful_operations_{0};
    std::atomic<size_t> failed_operations_{0};
    std::atomic<double> total_duration_ms_{0.0};
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
    mutable std::mutex metrics_mutex_;
    std::chrono::steady_clock::time_point start_time_;

public:
    PerformanceMetrics() : start_time_(std::chrono::steady_clock::now()) {}

    void recordOperation(bool success, double duration_ms) {
        total_operations_++;
        if (success) {
            successful_operations_++;
        } else {
            failed_operations_++;
        }
        total_duration_ms_ += duration_ms;
    }

    void recordCacheHit() { cache_hits_++; }
    void recordCacheMiss() { cache_misses_++; }

    void printStatistics() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        auto now = std::chrono::steady_clock::now();
        auto uptime =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

        size_t total_ops = total_operations_.load();
        size_t successful_ops = successful_operations_.load();
        size_t failed_ops = failed_operations_.load();
        double total_duration = total_duration_ms_.load();
        size_t hits = cache_hits_.load();
        size_t misses = cache_misses_.load();

        std::cout << "\n=== Performance Metrics ===" << std::endl;
        std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
        std::cout << "Total operations: " << total_ops << std::endl;
        std::cout << "Successful operations: " << successful_ops << std::endl;
        std::cout << "Failed operations: " << failed_ops << std::endl;

        if (total_ops > 0) {
            double success_rate =
                static_cast<double>(successful_ops) / total_ops * 100;
            double avg_duration = total_duration / total_ops;
            std::cout << "Success rate: " << std::fixed << std::setprecision(2)
                      << success_rate << "%" << std::endl;
            std::cout << "Average operation time: " << avg_duration << " ms"
                      << std::endl;
            std::cout << "Operations per second: "
                      << (total_ops / static_cast<double>(uptime.count()))
                      << std::endl;
        }

        size_t total_cache_ops = hits + misses;
        if (total_cache_ops > 0) {
            double hit_rate = static_cast<double>(hits) / total_cache_ops * 100;
            std::cout << "Cache hits: " << hits << std::endl;
            std::cout << "Cache misses: " << misses << std::endl;
            std::cout << "Cache hit rate: " << hit_rate << "%" << std::endl;
        }
    }
};

// Async search worker
class AsyncSearchWorker {
private:
    atom::search::SearchEngine& search_engine_;
    atom::search::ThreadSafeLRUCache<std::string, std::vector<std::string>>&
        cache_;
    PerformanceMetrics& metrics_;
    std::queue<std::string> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> should_stop_{false};
    std::vector<std::thread> worker_threads_;

public:
    AsyncSearchWorker(
        atom::search::SearchEngine& engine,
        atom::search::ThreadSafeLRUCache<std::string, std::vector<std::string>>&
            cache,
        PerformanceMetrics& metrics, size_t num_workers = 4)
        : search_engine_(engine), cache_(cache), metrics_(metrics) {
        // Start worker threads
        for (size_t i = 0; i < num_workers; ++i) {
            worker_threads_.emplace_back([this, i]() { workerLoop(i); });
        }
    }

    ~AsyncSearchWorker() { stop(); }

    void addSearchTask(const std::string& query) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(query);
        queue_cv_.notify_one();
    }

    void stop() {
        should_stop_ = true;
        queue_cv_.notify_all();

        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

private:
    void workerLoop(size_t worker_id) {
        std::cout << "Worker " << worker_id << " started" << std::endl;

        while (!should_stop_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !task_queue_.empty() || should_stop_;
            });

            if (should_stop_)
                break;

            std::string query = task_queue_.front();
            task_queue_.pop();
            lock.unlock();

            processSearchQuery(query, worker_id);
        }

        std::cout << "Worker " << worker_id << " stopped" << std::endl;
    }

    void processSearchQuery(const std::string& query, size_t worker_id) {
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = false;

        try {
            // Check cache first
            auto cached_result = cache_.get(query);
            if (cached_result) {
                metrics_.recordCacheHit();
                success = true;
                std::cout << "Worker " << worker_id << ": Cache HIT for '"
                          << query << "'" << std::endl;
            } else {
                metrics_.recordCacheMiss();

                // Perform search
                auto results = search_engine_.searchByContent(query);

                // Convert results to strings for caching
                std::vector<std::string> result_ids;
                for (const auto& doc : results) {
                    result_ids.push_back(doc->getId());
                }

                // Cache the results
                cache_.put(query, result_ids, std::chrono::minutes(5));

                success = true;
                std::cout << "Worker " << worker_id << ": Processed '" << query
                          << "' -> " << results.size() << " results"
                          << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Worker " << worker_id << ": Error processing '"
                      << query << "': " << e.what() << std::endl;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        double duration_ms = duration.count() / 1000.0;

        metrics_.recordOperation(success, duration_ms);
    }
};

// Batch processor for high-throughput scenarios
class BatchProcessor {
private:
    atom::search::SearchEngine& search_engine_;
    size_t batch_size_;

public:
    BatchProcessor(atom::search::SearchEngine& engine, size_t batch_size = 100)
        : search_engine_(engine), batch_size_(batch_size) {}

    void processBatchDocuments(
        const std::vector<atom::search::Document>& documents) {
        std::cout << "Processing batch of " << documents.size()
                  << " documents..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        // Process documents in batches
        for (size_t i = 0; i < documents.size(); i += batch_size_) {
            size_t end = std::min(i + batch_size_, documents.size());

            std::cout << "Processing batch " << (i / batch_size_ + 1)
                      << " (documents " << i << "-" << (end - 1) << ")"
                      << std::endl;

            // Add documents to search engine
            for (size_t j = i; j < end; ++j) {
                try {
                    search_engine_.addDocument(documents[j]);
                } catch (const std::exception& e) {
                    std::cerr << "Error adding document "
                              << documents[j].getId() << ": " << e.what()
                              << std::endl;
                }
            }

            // Small delay to prevent overwhelming the system
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        std::cout << "Batch processing completed in " << duration.count()
                  << " ms (" << (documents.size() / (duration.count() / 1000.0))
                  << " docs/sec)" << std::endl;
    }

    std::vector<
        std::future<std::vector<std::shared_ptr<atom::search::Document>>>>
    processBatchQueries(const std::vector<std::string>& queries) {
        std::cout << "Processing batch of " << queries.size()
                  << " queries asynchronously..." << std::endl;

        std::vector<
            std::future<std::vector<std::shared_ptr<atom::search::Document>>>>
            futures;

        for (const auto& query : queries) {
            futures.push_back(std::async(std::launch::async, [this, query]() {
                return search_engine_.searchByContent(query);
            }));
        }

        return futures;
    }
};

int main() {
    std::cout << "=== Advanced Features: Async, Batch, and Statistics ===\n";

    try {
        //----------------------------------------------------------------------
        // 1. System Setup
        //----------------------------------------------------------------------
        printSection("1. System Setup");

        // Create search engine with multiple threads
        atom::search::SearchEngine search_engine(8);
        std::cout << "Created search engine with 8 worker threads" << std::endl;

        // Create cache for search results
        atom::search::ThreadSafeLRUCache<std::string, std::vector<std::string>>
            result_cache(1000);
        std::cout << "Created LRU cache with capacity 1000" << std::endl;

        // Create performance metrics collector
        PerformanceMetrics metrics;
        std::cout << "Initialized performance metrics collector" << std::endl;

        //----------------------------------------------------------------------
        // 2. Batch Document Processing
        //----------------------------------------------------------------------
        printSection("2. Batch Document Processing");

        BatchProcessor batch_processor(search_engine, 50);

        // Generate sample documents
        std::vector<atom::search::Document> sample_documents;
        std::vector<std::string> categories = {
            "Technology", "Science", "Business", "Health", "Education"};
        std::vector<std::string> tech_terms = {
            "algorithm", "database", "machine learning",
            "artificial intelligence", "cloud computing"};

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> cat_dist(0, categories.size() - 1);
        std::uniform_int_distribution<> term_dist(0, tech_terms.size() - 1);

        for (int i = 0; i < 500; ++i) {
            std::string doc_id = "doc_" + std::to_string(i);
            std::string category = categories[cat_dist(gen)];
            std::string term = tech_terms[term_dist(gen)];

            std::string content =
                "This is a document about " + term + " in the " + category +
                " domain. "
                "It contains detailed information and examples related to " +
                term + ".";

            std::vector<std::string> tags = {category.substr(0, 3),
                                             term.substr(0, 5)};

            sample_documents.emplace_back(doc_id, content, tags);
        }

        // Process documents in batches
        batch_processor.processBatchDocuments(sample_documents);

        //----------------------------------------------------------------------
        // 3. Async Search Operations
        //----------------------------------------------------------------------
        printSection("3. Async Search Operations");

        // Create async search worker
        AsyncSearchWorker async_worker(search_engine, result_cache, metrics, 6);
        std::cout << "Created async search worker with 6 worker threads"
                  << std::endl;

        // Generate search queries
        std::vector<std::string> search_queries = {
            "machine learning",        "database optimization",
            "artificial intelligence", "cloud computing",
            "algorithm design",        "data science",
            "web development",         "system architecture",
            "performance tuning",      "security protocols"};

        // Submit async search tasks
        std::cout << "Submitting " << search_queries.size()
                  << " async search tasks..." << std::endl;
        for (const auto& query : search_queries) {
            async_worker.addSearchTask(query);
        }

        // Submit more tasks to test cache performance
        for (int i = 0; i < 50; ++i) {
            std::string query = search_queries[i % search_queries.size()];
            async_worker.addSearchTask(query);
        }

        // Wait for processing
        std::this_thread::sleep_for(std::chrono::seconds(3));

        //----------------------------------------------------------------------
        // 4. Batch Query Processing
        //----------------------------------------------------------------------
        printSection("4. Batch Query Processing");

        std::vector<std::string> batch_queries = {
            "technology trends",    "scientific research",
            "business strategy",    "health innovations",
            "educational methods",  "data analysis",
            "software engineering", "network security",
            "mobile development"};

        auto query_futures = batch_processor.processBatchQueries(batch_queries);

        std::cout << "Collecting results from " << query_futures.size()
                  << " async queries..." << std::endl;

        size_t total_results = 0;
        for (size_t i = 0; i < query_futures.size(); ++i) {
            try {
                auto results = query_futures[i].get();
                total_results += results.size();
                std::cout << "Query '" << batch_queries[i] << "' returned "
                          << results.size() << " results" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error in batch query " << i << ": " << e.what()
                          << std::endl;
            }
        }

        std::cout << "Total results from batch queries: " << total_results
                  << std::endl;

        //----------------------------------------------------------------------
        // 5. Performance Monitoring
        //----------------------------------------------------------------------
        printSection("5. Performance Monitoring");

        // Let async workers finish
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Print performance statistics
        metrics.printStatistics();

        // Print cache statistics
        auto cache_stats = result_cache.getStatistics();
        std::cout << "\n=== Cache Statistics ===" << std::endl;
        std::cout << "Cache size: " << cache_stats.size << "/"
                  << cache_stats.maxSize << std::endl;
        std::cout << "Cache hit count: " << cache_stats.hitCount << std::endl;
        std::cout << "Cache miss count: " << cache_stats.missCount << std::endl;
        std::cout << "Cache hit rate: " << (cache_stats.hitRate * 100) << "%"
                  << std::endl;
        std::cout << "Cache load factor: " << cache_stats.loadFactor
                  << std::endl;

        //----------------------------------------------------------------------
        // 6. Stress Testing
        //----------------------------------------------------------------------
        printSection("6. Stress Testing");

        std::cout << "Running stress test with concurrent operations..."
                  << std::endl;

        auto stress_start = std::chrono::high_resolution_clock::now();

        // Submit many concurrent search tasks
        for (int i = 0; i < 200; ++i) {
            std::string query = "stress test query " + std::to_string(i % 20);
            async_worker.addSearchTask(query);
        }

        // Wait for stress test completion
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto stress_end = std::chrono::high_resolution_clock::now();
        auto stress_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(stress_end -
                                                                  stress_start);

        std::cout << "Stress test completed in " << stress_duration.count()
                  << " ms" << std::endl;

        // Stop async worker
        async_worker.stop();

        //----------------------------------------------------------------------
        // 7. Final Statistics
        //----------------------------------------------------------------------
        printSection("7. Final Statistics");

        metrics.printStatistics();

        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "This advanced example demonstrated:" << std::endl;
        std::cout << "  1. Batch processing of documents for high throughput"
                  << std::endl;
        std::cout << "  2. Asynchronous search operations with worker threads"
                  << std::endl;
        std::cout << "  3. Real-time performance metrics collection"
                  << std::endl;
        std::cout << "  4. Concurrent cache operations and optimization"
                  << std::endl;
        std::cout << "  5. Stress testing under high load conditions"
                  << std::endl;
        std::cout << "  6. Resource management and error handling" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
