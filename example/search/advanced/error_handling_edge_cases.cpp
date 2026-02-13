/**
 * @file error_handling_edge_cases.cpp
 * @brief Comprehensive example demonstrating error handling and edge cases
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - Comprehensive error handling patterns
 * - Edge case scenarios and their handling
 * - Recovery mechanisms and fallback strategies
 * - Input validation and sanitization
 * - Resource exhaustion scenarios
 * - Concurrent error handling
 * - Logging and debugging techniques
 * - Best practices for robust applications
 */

#include <chrono>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
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

// Custom error loggerclass ErrorLogger {
private:
std::ofstream log_file_;
std::mutex log_mutex_;

public:
ErrorLogger(const std::string& filename = "error_log.txt")
    : log_file_(filename, std::ios::app) {}

void logError(const std::string& operation, const std::string& error,
              const std::string& context = "") {
    std::lock_guard<std::mutex> lock(log_mutex_);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::cout << "[ERROR] " << operation << ": " << error;
    if (!context.empty()) {
        std::cout << " (Context: " << context << ")";
    }
    std::cout << std::endl;

    if (log_file_.is_open()) {
        log_file_ << std::ctime(&time_t) << " [ERROR] " << operation << ": "
                  << error;
        if (!context.empty()) {
            log_file_ << " (Context: " << context << ")";
        }
        log_file_ << std::endl;
        log_file_.flush();
    }
}

void logWarning(const std::string& operation, const std::string& warning) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << "[WARNING] " << operation << ": " << warning << std::endl;

    if (log_file_.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        log_file_ << std::ctime(&time_t) << " [WARNING] " << operation << ": "
                  << warning << std::endl;
        log_file_.flush();
    }
}
}
;

// Robust search engine wrapper with error handlingclass RobustSearchEngine {
private:
std::unique_ptr<atom::search::SearchEngine> engine_;
ErrorLogger & logger_;
std::atomic<size_t> error_count_{0};
std::atomic<size_t> operation_count_{0};

public:
explicit RobustSearchEngine(ErrorLogger& logger, unsigned int threads = 4)
    : engine_(std::make_unique<atom::search::SearchEngine>(threads)),
      logger_(logger) {}

bool addDocumentSafely(const atom::search::Document& doc) {
    operation_count_++;

    try {
        // Input validation
        if (doc.getId().empty()) {
            logger_.logError("addDocument", "Document ID cannot be empty",
                             doc.getId());
            error_count_++;
            return false;
        }

        if (doc.getContent().empty()) {
            logger_.logError("addDocument", "Document content cannot be empty",
                             doc.getId());
            error_count_++;
            return false;
        }

        // Check for extremely large content
        if (doc.getContent().size() > 1000000) {  // 1MB limit
            logger_.logWarning("addDocument", "Document content is very large",
                               doc.getId());
        }

        engine_->addDocument(doc);
        return true;

    } catch (const atom::search::DocumentValidationException& e) {
        logger_.logError("addDocument", e.what(), doc.getId());
        error_count_++;
        return false;
    } catch (const atom::search::SearchEngineException& e) {
        logger_.logError("addDocument", e.what(), doc.getId());
        error_count_++;
        return false;
    } catch (const std::exception& e) {
        logger_.logError("addDocument", e.what(), doc.getId());
        error_count_++;
        return false;
    } catch (...) {
        logger_.logError("addDocument", "Unknown error occurred", doc.getId());
        error_count_++;
        return false;
    }
}

std::vector<std::shared_ptr<atom::search::Document>> searchSafely(
    const std::string& query, const std::string& search_type = "content") {
    operation_count_++;

    try {
        // Input validation
        if (query.empty()) {
            logger_.logError("search", "Query cannot be empty");
            error_count_++;
            return {};
        }

        // Check for extremely long queries
        if (query.size() > 10000) {
            logger_.logWarning("search", "Query is very long: " +
                                             std::to_string(query.size()) +
                                             " characters");
        }

        // Sanitize query (remove potential problematic characters)
        std::string sanitized_query = sanitizeQuery(query);
        if (sanitized_query != query) {
            logger_.logWarning("search", "Query was sanitized");
        }

        if (search_type == "content") {
            return engine_->searchByContent(sanitized_query);
        } else if (search_type == "tag") {
            return engine_->searchByTag(sanitized_query);
        } else if (search_type == "boolean") {
            return engine_->booleanSearch(sanitized_query);
        } else {
            logger_.logError("search", "Unknown search type: " + search_type);
            error_count_++;
            return {};
        }

    } catch (const atom::search::SearchOperationException& e) {
        logger_.logError("search", e.what(), query);
        error_count_++;
        return {};
    } catch (const atom::search::SearchEngineException& e) {
        logger_.logError("search", e.what(), query);
        error_count_++;
        return {};
    } catch (const std::exception& e) {
        logger_.logError("search", e.what(), query);
        error_count_++;
        return {};
    } catch (...) {
        logger_.logError("search", "Unknown error occurred", query);
        error_count_++;
        return {};
    }
}

bool saveIndexSafely(const std::string& filename) {
    operation_count_++;

    try {
        // Validate filename
        if (filename.empty()) {
            logger_.logError("saveIndex", "Filename cannot be empty");
            error_count_++;
            return false;
        }

        // Check if directory exists and is writable
        std::ofstream test_file(filename, std::ios::app);
        if (!test_file.is_open()) {
            logger_.logError("saveIndex", "Cannot write to file", filename);
            error_count_++;
            return false;
        }
        test_file.close();

        engine_->saveIndex(filename);
        return true;

    } catch (const std::exception& e) {
        logger_.logError("saveIndex", e.what(), filename);
        error_count_++;
        return false;
    } catch (...) {
        logger_.logError("saveIndex", "Unknown error occurred", filename);
        error_count_++;
        return false;
    }
}

double getErrorRate() const {
    size_t total_ops = operation_count_.load();
    if (total_ops == 0)
        return 0.0;
    return static_cast<double>(error_count_.load()) / total_ops;
}

void printStatistics() const {
    std::cout << "\n=== Robust Search Engine Statistics ===" << std::endl;
    std::cout << "Total operations: " << operation_count_.load() << std::endl;
    std::cout << "Total errors: " << error_count_.load() << std::endl;
    std::cout << "Error rate: " << (getErrorRate() * 100) << "%" << std::endl;
}

private:
std::string sanitizeQuery(const std::string& query) {
    std::string sanitized = query;

    // Remove null characters
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'),
                    sanitized.end());

    // Replace control characters with spaces
    for (char& c : sanitized) {
        if (std::iscntrl(c) && c != '\t' && c != '\n' && c != '\r') {
            c = ' ';
        }
    }

    return sanitized;
}
}
;

// Test various edge casesvoid testEdgeCases(RobustSearchEngine& engine,
// ErrorLogger& logger) {
std::cout << "Testing edge cases..." << std::endl;

// Test 1: Empty document ID
try {
    atom::search::Document empty_id_doc("", "Some content", {"tag"});
    engine.addDocumentSafely(empty_id_doc);
} catch (...) {
    logger.logError("testEdgeCases", "Exception during empty ID test");
}

// Test 2: Empty content
try {
    atom::search::Document empty_content_doc("doc1", "", {"tag"});
    engine.addDocumentSafely(empty_content_doc);
} catch (...) {
    logger.logError("testEdgeCases", "Exception during empty content test");
}

// Test 3: Very long content
try {
    std::string long_content(2000000, 'a');  // 2MB of 'a' characters
    atom::search::Document long_doc("long_doc", long_content, {"long"});
    engine.addDocumentSafely(long_doc);
} catch (...) {
    logger.logError("testEdgeCases", "Exception during long content test");
}

// Test 4: Special characters in content
try {
    std::string special_content =
        "Content with special chars: \x01\x02\x03\x7F\xFF";
    atom::search::Document special_doc("special_doc", special_content,
                                       {"special"});
    engine.addDocumentSafely(special_doc);
} catch (...) {
    logger.logError("testEdgeCases",
                    "Exception during special characters test");
}

// Test 5: Unicode content
try {
    std::string unicode_content = "Unicode content: 你好世界 🌍 Здравствуй мир";
    atom::search::Document unicode_doc("unicode_doc", unicode_content,
                                       {"unicode"});
    engine.addDocumentSafely(unicode_doc);
} catch (...) {
    logger.logError("testEdgeCases", "Exception during unicode test");
}

// Test 6: Empty search query
auto results = engine.searchSafely("");

// Test 7: Very long search query
std::string long_query(50000, 'x');
results = engine.searchSafely(long_query);

// Test 8: Search with special characters
results = engine.searchSafely("query with \x01\x02 special chars");

// Test 9: Invalid search type
results = engine.searchSafely("test query", "invalid_type");

std::cout << "Edge case testing completed." << std::endl;
}

int main() {
    std::cout << "=== Error Handling and Edge Cases Example ===\n";

    try {
        //----------------------------------------------------------------------
        // 1. Setup Error Logging
        //----------------------------------------------------------------------
        printSection("1. Setup Error Logging");

        ErrorLogger logger("search_errors.log");
        std::cout << "Error logger initialized." << std::endl;

        //----------------------------------------------------------------------
        // 2. Create Robust Search Engine
        //----------------------------------------------------------------------
        printSection("2. Create Robust Search Engine");

        RobustSearchEngine robust_engine(logger, 4);
        std::cout << "Robust search engine created with error handling."
                  << std::endl;

        //----------------------------------------------------------------------
        // 3. Test Edge Cases
        //----------------------------------------------------------------------
        printSection("3. Test Edge Cases");

        testEdgeCases(robust_engine, logger);

        //----------------------------------------------------------------------
        // 4. Test Normal Operations
        //----------------------------------------------------------------------
        printSection("4. Test Normal Operations");

        std::cout << "Adding valid documents..." << std::endl;

        // Add some valid documents
        std::vector<atom::search::Document> valid_docs = {
            atom::search::Document("doc1", "Machine learning algorithms",
                                   {"ml", "ai"}),
            atom::search::Document("doc2", "Database optimization techniques",
                                   {"db", "performance"}),
            atom::search::Document("doc3", "Web development best practices",
                                   {"web", "dev"})};

        for (const auto& doc : valid_docs) {
            if (robust_engine.addDocumentSafely(doc)) {
                std::cout << "Successfully added: " << doc.getId() << std::endl;
            }
        }

        // Test normal searches
        std::cout << "\nTesting normal search operations..." << std::endl;

        auto results = robust_engine.searchSafely("machine learning");
        std::cout << "Search for 'machine learning' returned " << results.size()
                  << " results" << std::endl;

        results = robust_engine.searchSafely("optimization", "content");
        std::cout << "Search for 'optimization' returned " << results.size()
                  << " results" << std::endl;

        //----------------------------------------------------------------------
        // 5. Test Concurrent Error Scenarios
        //----------------------------------------------------------------------
        printSection("5. Test Concurrent Error Scenarios");

        std::cout << "Testing concurrent operations with potential errors..."
                  << std::endl;

        std::vector<std::future<void>> futures;

        // Launch multiple threads that will cause various errors
        for (int i = 0; i < 10; ++i) {
            futures.push_back(std::async(std::launch::async, [&robust_engine,
                                                              i]() {
                // Mix of valid and invalid operations
                if (i % 3 == 0) {
                    // Invalid document
                    atom::search::Document invalid_doc("", "content", {"tag"});
                    robust_engine.addDocumentSafely(invalid_doc);
                } else if (i % 3 == 1) {
                    // Invalid search
                    robust_engine.searchSafely("");
                } else {
                    // Valid operation
                    atom::search::Document valid_doc(
                        "thread_doc_" + std::to_string(i),
                        "Content from thread " + std::to_string(i), {"thread"});
                    robust_engine.addDocumentSafely(valid_doc);
                }
            }));
        }

        // Wait for all threads to complete
        for (auto& future : futures) {
            future.wait();
        }

        std::cout << "Concurrent error testing completed." << std::endl;

        //----------------------------------------------------------------------
        // 6. Test Resource Exhaustion Scenarios
        //----------------------------------------------------------------------
        printSection("6. Test Resource Exhaustion Scenarios");

        std::cout << "Testing resource exhaustion scenarios..." << std::endl;

        // Test with many documents
        for (int i = 0; i < 1000; ++i) {
            std::string content =
                "Document " + std::to_string(i) + " with some content";
            atom::search::Document doc("bulk_doc_" + std::to_string(i), content,
                                       {"bulk"});
            robust_engine.addDocumentSafely(doc);
        }

        std::cout << "Added 1000 documents for resource testing." << std::endl;

        // Test many concurrent searches
        std::vector<std::future<void>> search_futures;
        for (int i = 0; i < 50; ++i) {
            search_futures.push_back(
                std::async(std::launch::async, [&robust_engine, i]() {
                    std::string query = "Document " + std::to_string(i % 100);
                    robust_engine.searchSafely(query);
                }));
        }

        for (auto& future : search_futures) {
            future.wait();
        }

        std::cout << "Completed 50 concurrent searches." << std::endl;

        //----------------------------------------------------------------------
        // 7. Test File I/O Error Scenarios
        //----------------------------------------------------------------------
        printSection("7. Test File I/O Error Scenarios");

        std::cout << "Testing file I/O error scenarios..." << std::endl;

        // Test saving to invalid path
        bool success = robust_engine.saveIndexSafely("/invalid/path/index.dat");
        std::cout << "Save to invalid path: "
                  << (success ? "Success" : "Failed (expected)") << std::endl;

        // Test saving to read-only location (if possible)
        success = robust_engine.saveIndexSafely("/root/index.dat");
        std::cout << "Save to read-only location: "
                  << (success ? "Success" : "Failed (expected)") << std::endl;

        // Test saving with empty filename
        success = robust_engine.saveIndexSafely("");
        std::cout << "Save with empty filename: "
                  << (success ? "Success" : "Failed (expected)") << std::endl;

        // Test saving to valid location
        success = robust_engine.saveIndexSafely("test_index.dat");
        std::cout << "Save to valid location: "
                  << (success ? "Success" : "Failed") << std::endl;

        //----------------------------------------------------------------------
        // 8. Final Statistics and Cleanup
        //----------------------------------------------------------------------
        printSection("8. Final Statistics and Cleanup");

        robust_engine.printStatistics();

        // Clean up test files
        std::remove("test_index.dat");
        std::remove("search_errors.log");

        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "This error handling example demonstrated:" << std::endl;
        std::cout << "  1. Comprehensive error logging and monitoring"
                  << std::endl;
        std::cout << "  2. Input validation and sanitization" << std::endl;
        std::cout << "  3. Edge case handling (empty inputs, special "
                     "characters, etc.)"
                  << std::endl;
        std::cout << "  4. Concurrent error scenarios and thread safety"
                  << std::endl;
        std::cout << "  5. Resource exhaustion testing" << std::endl;
        std::cout << "  6. File I/O error handling" << std::endl;
        std::cout << "  7. Recovery mechanisms and graceful degradation"
                  << std::endl;
        std::cout << "  8. Performance monitoring under error conditions"
                  << std::endl;

        std::cout << "\nError handling example completed successfully!"
                  << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
