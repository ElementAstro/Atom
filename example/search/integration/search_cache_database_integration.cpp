/**
 * @file search_cache_database_integration.cpp
 * @brief Comprehensive integration example combining Search Engine, Cache, and
 * Database
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates a real-world scenario where:
 * - Documents are stored in a database (SQLite)
 * - Search results are cached for performance (LRU Cache)
 * - Full-text search is performed with caching optimization
 * - Statistics and monitoring are implemented
 * - Async operations for better performance
 * - Error handling and recovery mechanisms
 *
 * Use case: A document management system with search capabilities
 */

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/search/lru.hpp"
#include "atom/search/search.hpp"
#include "atom/search/sqlite.hpp"

// Helper function to print section titlesvoid printSection(const std::string&
// title) {
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n";
}

// Document structure for our systemstruct DocumentRecord {
std::string id;
std::string title;
std::string content;
std::string category;
std::vector<std::string> tags;
std::chrono::system_clock::time_point created_at;
int view_count;

std::string toString() const {
    std::string tags_str = "";
    for (const auto& tag : tags) {
        tags_str += tag + " ";
    }
    return "Document[id=" + id + ", title=" + title + ", category=" + category +
           ", tags=" + tags_str + ", views=" + std::to_string(view_count) + "]";
}
}
;

// Search result structure for cachingstruct SearchResult {
std::vector<DocumentRecord> documents;
std::chrono::system_clock::time_point timestamp;
size_t total_count;
std::string query;

SearchResult() : total_count(0) {}

SearchResult(const std::vector<DocumentRecord>& docs, const std::string& q)
    : documents(docs),
      timestamp(std::chrono::system_clock::now()),
      total_count(docs.size()),
      query(q) {}
}
;

/**
 * @brief Integrated Document Management System
 *
 * This class combines search engine, caching, and database functionality
 * to provide a high-performance document management and search system.
 */
class DocumentManagementSystem {
private:
    std::unique_ptr<atom::search::database::SqliteDB> db_;
    std::unique_ptr<atom::search::SearchEngine> search_engine_;
    std::unique_ptr<atom::search::ThreadSafeLRUCache<std::string, SearchResult>>
        cache_;

    // Statistics
    std::atomic<size_t> total_searches_{0};
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
    std::atomic<size_t> db_queries_{0};

public:
    explicit DocumentManagementSystem(const std::string& db_path,
                                      size_t cache_size = 100,
                                      unsigned search_threads = 4)
        : db_(std::make_unique<atom::search::database::SqliteDB>(db_path)),
          search_engine_(
              std::make_unique<atom::search::SearchEngine>(search_threads)),
          cache_(std::make_unique<
                 atom::search::ThreadSafeLRUCache<std::string, SearchResult>>(
              cache_size)) {
        initializeDatabase();
        setupCallbacks();
    }

    /**
     * @brief Initialize database schema
     */
    void initializeDatabase() {
        std::cout << "Initializing database schema..." << std::endl;

        // Create documents table
        db_->executeQuery(R"(
            CREATE TABLE IF NOT EXISTS documents (
                id TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                content TEXT NOT NULL,
                category TEXT,
                tags TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                view_count INTEGER DEFAULT 0
            )
        )");

        // Create full-text search index
        db_->executeQuery(R"(
            CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts USING fts5(
                id,
                title,
                content,
                category,
                tags,
                content='documents',
                content_rowid='rowid'
            )
        )");

        // Create triggers to keep FTS in sync
        db_->executeQuery(R"(
            CREATE TRIGGER IF NOT EXISTS documents_ai AFTER INSERT ON documents BEGIN
                INSERT INTO documents_fts(rowid, id, title, content, category, tags)
                VALUES (new.rowid, new.id, new.title, new.content, new.category, new.tags);
            END
        )");

        db_->executeQuery(R"(
            CREATE TRIGGER IF NOT EXISTS documents_ad AFTER DELETE ON documents BEGIN
                INSERT INTO documents_fts(documents_fts, rowid, id, title, content, category, tags)
                VALUES('delete', old.rowid, old.id, old.title, old.content, old.category, old.tags);
            END
        )");

        std::cout << "Database schema initialized successfully." << std::endl;
    }

    /**
     * @brief Setup cache callbacks for monitoring
     */
    void setupCallbacks() {
        cache_->setInsertCallback(
            [this](const std::string& key, const SearchResult& result) {
                std::cout << "Cache: Stored search result for query '" << key
                          << "' with " << result.documents.size()
                          << " documents" << std::endl;
            });

        cache_->setEraseCallback([this](const std::string& key) {
            std::cout << "Cache: Evicted search result for query '" << key
                      << "'" << std::endl;
        });
    }

    /**
     * @brief Add a document to the system
     */
    bool addDocument(const DocumentRecord& doc) {
        try {
            // Convert tags to string
            std::string tags_str = "";
            for (const auto& tag : doc.tags) {
                tags_str += tag + " ";
            }

            // Insert into database
            bool success = db_->executeParameterizedQuery(
                "INSERT INTO documents (id, title, content, category, tags, "
                "view_count) VALUES (?, ?, ?, ?, ?, ?)",
                doc.id, doc.title, doc.content, doc.category, tags_str,
                doc.view_count);

            if (success) {
                // Add to search engine
                atom::search::Document search_doc(doc.id, doc.content,
                                                  doc.tags);
                search_engine_->addDocument(search_doc);

                // Clear cache since we have new content
                cache_->clear();

                std::cout << "Added document: " << doc.id << std::endl;
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error adding document: " << e.what() << std::endl;
        }
        return false;
    }

    /**
     * @brief Search documents with caching
     */
    SearchResult searchDocuments(const std::string& query,
                                 bool use_cache = true) {
        total_searches_++;

        // Check cache first
        if (use_cache) {
            auto cached_result = cache_->get(query);
            if (cached_result) {
                cache_hits_++;
                std::cout << "Cache HIT for query: '" << query << "'"
                          << std::endl;
                return *cached_result;
            }
        }

        cache_misses_++;
        std::cout << "Cache MISS for query: '" << query
                  << "' - performing database search" << std::endl;

        // Perform database search
        SearchResult result = performDatabaseSearch(query);

        // Cache the result
        if (use_cache && !result.documents.empty()) {
            cache_->put(query, result,
                        std::chrono::minutes(10));  // Cache for 10 minutes
        }

        return result;
    }

    /**
     * @brief Perform actual database search
     */
    SearchResult performDatabaseSearch(const std::string& query) {
        db_queries_++;

        try {
            // Use FTS for full-text search
            auto db_results = db_->selectData(R"(
                SELECT d.id, d.title, d.content, d.category, d.tags, d.view_count
                FROM documents d
                JOIN documents_fts fts ON d.rowid = fts.rowid
                WHERE documents_fts MATCH ?
                ORDER BY rank
            )");

            // For this example, we'll simulate the parameterized query
            // In real implementation, you'd use proper parameterized queries
            std::string fts_query =
                "SELECT d.id, d.title, d.content, d.category, d.tags, "
                "d.view_count "
                "FROM documents d "
                "JOIN documents_fts fts ON d.rowid = fts.rowid "
                "WHERE documents_fts MATCH '" +
                query +
                "' "
                "ORDER BY rank";

            auto db_results = db_->selectData(fts_query);

            std::vector<DocumentRecord> documents;
            for (const auto& row : db_results) {
                if (row.size() >= 6) {
                    DocumentRecord doc;
                    doc.id = row[0];
                    doc.title = row[1];
                    doc.content = row[2];
                    doc.category = row[3];

                    // Parse tags
                    std::string tags_str = row[4];
                    std::istringstream iss(tags_str);
                    std::string tag;
                    while (iss >> tag) {
                        doc.tags.push_back(tag);
                    }

                    doc.view_count = std::stoi(row[5]);
                    documents.push_back(doc);
                }
            }

            return SearchResult(documents, query);

        } catch (const std::exception& e) {
            std::cerr << "Database search error: " << e.what() << std::endl;
            return SearchResult();
        }
    }

    /**
     * @brief Get document by ID with caching
     */
    std::optional<DocumentRecord> getDocument(const std::string& id) {
        try {
            auto result = db_->selectData(
                "SELECT id, title, content, category, tags, view_count FROM "
                "documents WHERE id = '" +
                id + "'");

            if (!result.empty() && !result[0].empty()) {
                const auto& row = result[0];
                DocumentRecord doc;
                doc.id = row[0];
                doc.title = row[1];
                doc.content = row[2];
                doc.category = row[3];

                // Parse tags
                std::string tags_str = row[4];
                std::istringstream iss(tags_str);
                std::string tag;
                while (iss >> tag) {
                    doc.tags.push_back(tag);
                }

                doc.view_count = std::stoi(row[5]);

                // Increment view count
                db_->executeQuery(
                    "UPDATE documents SET view_count = view_count + 1 WHERE id "
                    "= '" +
                    id + "'");
                doc.view_count++;

                return doc;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting document: " << e.what() << std::endl;
        }

        return std::nullopt;
    }

    /**
     * @brief Get system statistics
     */
    void printStatistics() {
        std::cout << "\n=== System Statistics ===" << std::endl;
        std::cout << "Total searches: " << total_searches_ << std::endl;
        std::cout << "Cache hits: " << cache_hits_ << std::endl;
        std::cout << "Cache misses: " << cache_misses_ << std::endl;
        std::cout << "Database queries: " << db_queries_ << std::endl;

        if (total_searches_ > 0) {
            double hit_rate =
                static_cast<double>(cache_hits_) / total_searches_ * 100;
            std::cout << "Cache hit rate: " << std::fixed
                      << std::setprecision(2) << hit_rate << "%" << std::endl;
        }

        auto cache_stats = cache_->getStatistics();
        std::cout << "Cache size: " << cache_stats.size << "/"
                  << cache_stats.maxSize << std::endl;
        std::cout << "Cache load factor: " << cache_stats.loadFactor
                  << std::endl;
    }

    /**
     * @brief Async search operation
     */
    std::future<SearchResult> searchAsync(const std::string& query) {
        return std::async(std::launch::async,
                          [this, query]() { return searchDocuments(query); });
    }
};

int main() {
    std::cout << "=== Integrated Document Management System Example ===\n";

    try {
        //----------------------------------------------------------------------
        // 1. System Initialization
        //----------------------------------------------------------------------
        printSection("1. System Initialization");

        DocumentManagementSystem dms(":memory:", 50, 4);
        std::cout << "Document Management System initialized successfully."
                  << std::endl;

        //----------------------------------------------------------------------
        // 2. Adding Sample Documents
        //----------------------------------------------------------------------
        printSection("2. Adding Sample Documents");

        std::vector<DocumentRecord> sample_docs = {
            {"doc1",
             "Machine Learning Fundamentals",
             "Introduction to machine learning algorithms and neural networks",
             "Technology",
             {"ml", "ai", "algorithms"},
             {},
             0},
            {"doc2",
             "Database Optimization Guide",
             "Advanced techniques for optimizing database performance and "
             "queries",
             "Technology",
             {"database", "performance", "sql"},
             {},
             0},
            {"doc3",
             "Web Development Best Practices",
             "Modern approaches to building scalable web applications",
             "Technology",
             {"web", "development", "javascript"},
             {},
             0},
            {"doc4",
             "Data Science with Python",
             "Complete guide to data analysis and visualization using Python",
             "Technology",
             {"python", "data", "science"},
             {},
             0},
            {"doc5",
             "System Architecture Patterns",
             "Design patterns for building robust and scalable systems",
             "Technology",
             {"architecture", "design", "patterns"},
             {},
             0}};

        for (const auto& doc : sample_docs) {
            dms.addDocument(doc);
        }

        std::cout << "Added " << sample_docs.size() << " sample documents."
                  << std::endl;

        //----------------------------------------------------------------------
        // 3. Search Operations with Caching
        //----------------------------------------------------------------------
        printSection("3. Search Operations with Caching");

        std::vector<std::string> search_queries = {
            "machine learning", "database optimization", "web development",
            "python data", "system architecture"};

        std::cout << "Performing initial searches (cache misses expected):"
                  << std::endl;
        for (const auto& query : search_queries) {
            auto result = dms.searchDocuments(query);
            std::cout << "Query: '" << query << "' -> Found "
                      << result.documents.size() << " documents" << std::endl;

            if (!result.documents.empty()) {
                std::cout << "  First result: " << result.documents[0].title
                          << std::endl;
            }
        }

        std::cout << "\nPerforming same searches again (cache hits expected):"
                  << std::endl;
        for (const auto& query : search_queries) {
            auto result = dms.searchDocuments(query);
            std::cout << "Query: '" << query << "' -> Found "
                      << result.documents.size() << " documents" << std::endl;
        }

        //----------------------------------------------------------------------
        // 4. Document Retrieval and View Tracking
        //----------------------------------------------------------------------
        printSection("4. Document Retrieval and View Tracking");

        std::cout << "Retrieving documents by ID:" << std::endl;
        std::vector<std::string> doc_ids = {"doc1", "doc2", "doc3"};

        for (const auto& id : doc_ids) {
            auto doc = dms.getDocument(id);
            if (doc) {
                std::cout << "Retrieved: " << doc->toString() << std::endl;
            } else {
                std::cout << "Document not found: " << id << std::endl;
            }
        }

        // Retrieve same documents again to show view count increment
        std::cout
            << "\nRetrieving same documents again (view count should increase):"
            << std::endl;
        for (const auto& id : doc_ids) {
            auto doc = dms.getDocument(id);
            if (doc) {
                std::cout << "Retrieved: " << doc->toString() << std::endl;
            }
        }

        //----------------------------------------------------------------------
        // 5. Async Search Operations
        //----------------------------------------------------------------------
        printSection("5. Async Search Operations");

        std::cout << "Performing asynchronous searches:" << std::endl;

        std::vector<std::future<SearchResult>> async_results;
        std::vector<std::string> async_queries = {
            "machine learning algorithms", "database performance optimization",
            "web application development"};

        // Start async searches
        for (const auto& query : async_queries) {
            async_results.push_back(dms.searchAsync(query));
            std::cout << "Started async search for: '" << query << "'"
                      << std::endl;
        }

        // Collect results
        std::cout << "\nCollecting async search results:" << std::endl;
        for (size_t i = 0; i < async_results.size(); ++i) {
            auto result = async_results[i].get();
            std::cout << "Async query '" << async_queries[i]
                      << "' completed -> " << result.documents.size()
                      << " documents found" << std::endl;
        }

        //----------------------------------------------------------------------
        // 6. Performance Testing
        //----------------------------------------------------------------------
        printSection("6. Performance Testing");

        std::cout << "Running performance test with repeated searches:"
                  << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        // Perform many searches to test cache performance
        for (int i = 0; i < 100; ++i) {
            std::string query = search_queries[i % search_queries.size()];
            dms.searchDocuments(query);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        std::cout << "Completed 100 searches in " << duration.count()
                  << " milliseconds" << std::endl;
        std::cout << "Average time per search: " << (duration.count() / 100.0)
                  << " ms" << std::endl;

        //----------------------------------------------------------------------
        // 7. System Statistics and Monitoring
        //----------------------------------------------------------------------
        printSection("7. System Statistics and Monitoring");

        dms.printStatistics();

        //----------------------------------------------------------------------
        // 8. Cache Management
        //----------------------------------------------------------------------
        printSection("8. Cache Management");

        std::cout << "Testing cache eviction with many unique queries:"
                  << std::endl;

        // Generate many unique queries to test cache eviction
        for (int i = 0; i < 60; ++i) {
            std::string unique_query = "test query " + std::to_string(i);
            dms.searchDocuments(unique_query);
        }

        std::cout << "Generated 60 unique queries to test cache eviction."
                  << std::endl;
        dms.printStatistics();

        //----------------------------------------------------------------------
        // Summary
        //----------------------------------------------------------------------
        printSection("Summary");

        std::cout << "This integration example demonstrated:" << std::endl;
        std::cout << "  1. Database-backed document storage with SQLite"
                  << std::endl;
        std::cout << "  2. Full-text search capabilities with FTS5"
                  << std::endl;
        std::cout << "  3. LRU caching for search result optimization"
                  << std::endl;
        std::cout << "  4. Search engine integration for advanced queries"
                  << std::endl;
        std::cout << "  5. Asynchronous search operations" << std::endl;
        std::cout << "  6. Performance monitoring and statistics" << std::endl;
        std::cout << "  7. View tracking and document management" << std::endl;
        std::cout << "  8. Cache eviction and memory management" << std::endl;

        std::cout << "\nIntegration example completed successfully!"
                  << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "System error: " << e.what() << std::endl;
        return 1;
    }
}
