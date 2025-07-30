/**
 * @file search.hpp
 * @brief Defines the Document and SearchEngine classes for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_SEARCH_HPP
#define ATOM_SEARCH_SEARCH_HPP

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atom::search {

using String = std::string;
template<typename K, typename V>
using HashMap = std::unordered_map<K, V>;
template<typename T>
using HashSet = std::unordered_set<T>;
template<typename T>
using Vector = std::vector<T>;

/**
 * @brief Hash function for String pairs used in TF-IDF cache
 */
struct StringPairHash {
    std::size_t operator()(const std::pair<String, String>& p) const noexcept {
        std::size_t h1 = std::hash<String>{}(p.first);
        std::size_t h2 = std::hash<String>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

/**
 * @brief Configuration options for the search engine.
 */
struct SearchConfig {
    size_t max_results = 100;           ///< Maximum results per search
    double score_threshold = 0.0;       ///< Minimum score threshold
    bool enable_stemming = false;       ///< Enable word stemming
    bool enable_fuzzy = true;           ///< Enable fuzzy matching
    size_t cache_size = 1000;          ///< Search result cache size
    std::chrono::milliseconds cache_ttl{300000}; ///< Cache TTL (5 minutes)

    // Performance optimization settings
    size_t tokenized_cache_size = 5000;  ///< Maximum tokenized content cache entries per shard
    size_t tf_idf_cache_size = 10000;    ///< Maximum TF-IDF cache entries per shard
    bool enable_performance_caching = true; ///< Enable performance caches

    // Similarity search settings
    double min_similarity_threshold = 0.01; ///< Minimum similarity for semantic search
    bool use_cosine_similarity = true;      ///< Use cosine similarity (vs Jaccard)

    // Enhanced features
    bool enable_semantic_search = true;     ///< Enable semantic search capabilities
    bool enable_ranked_autocomplete = true; ///< Enable frequency-based autocomplete ranking
};

/**
 * @brief Search result pagination parameters.
 */
struct SearchPagination {
    size_t offset = 0;                  ///< Result offset
    size_t limit = 50;                  ///< Maximum results to return
};

/**
 * @brief Search performance metrics.
 */
struct SearchMetrics {
    std::atomic<uint64_t> total_searches{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> total_documents_indexed{0};
    std::atomic<uint64_t> total_search_time_ms{0};

    double get_cache_hit_ratio() const noexcept {
        uint64_t total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
    }

    double get_average_search_time_ms() const noexcept {
        uint64_t searches = total_searches.load();
        return searches > 0 ? static_cast<double>(total_search_time_ms.load()) / searches : 0.0;
    }
};

/**
 * @brief Base exception class for search engine errors.
 */
class SearchEngineException : public std::exception {
public:
    /**
     * @brief Constructs a SearchEngineException with a given message.
     * @param message The error message.
     */
    explicit SearchEngineException(std::string message)
        : message_(std::move(message)) {}

    /**
     * @brief Returns the error message.
     * @return The error message as a C-style string.
     */
    const char* what() const noexcept override { return message_.c_str(); }

protected:
    std::string message_;
};

/**
 * @brief Exception thrown when a document is not found.
 */
class DocumentNotFoundException : public SearchEngineException {
public:
    /**
     * @brief Constructs a DocumentNotFoundException for a given document ID.
     * @param doc_id The ID of the document that was not found.
     */
    explicit DocumentNotFoundException(const String& doc_id)
        : SearchEngineException("Document not found: " + std::string(doc_id)) {}
};

/**
 * @brief Exception for document validation errors.
 */
class DocumentValidationException : public SearchEngineException {
public:
    /**
     * @brief Constructs a DocumentValidationException with a given message.
     * @param message The validation error message.
     */
    explicit DocumentValidationException(const std::string& message)
        : SearchEngineException("Document validation error: " + message) {}
};

/**
 * @brief Exception for errors during a search operation.
 */
class SearchOperationException : public SearchEngineException {
public:
    /**
     * @brief Constructs a SearchOperationException with a given message.
     * @param message The search operation error message.
     */
    explicit SearchOperationException(const std::string& message)
        : SearchEngineException("Search operation error: " + message) {}
};



/**
 * @brief Represents a searchable document.
 *
 * Contains an ID, content, a set of tags, and a click counter for relevance.
 * The class is thread-safe for click count modifications.
 */
class Document {
public:
    /**
     * @brief Constructs a Document.
     * @param id The unique identifier for the document.
     * @param content The main content of the document.
     * @param tags An initializer list of tags.
     * @throws DocumentValidationException if any validation fails.
     */
    explicit Document(String id, String content,
                      std::initializer_list<std::string> tags = {});

    /**
     * @brief Constructs a Document with vector of tags.
     * @param id The unique identifier for the document.
     * @param content The main content of the document.
     * @param tags A vector of tags.
     * @throws DocumentValidationException if any validation fails.
     */
    explicit Document(String id, String content,
                      const std::vector<std::string>& tags);

    /**
     * @brief Default destructor.
     */
    ~Document() = default;

    Document(const Document& other);
    Document& operator=(const Document& other);
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;

    /**
     * @brief Validates the document's fields.
     * @throws DocumentValidationException if validation fails.
     */
    void validate() const;

    /**
     * @brief Gets the document's ID.
     * @return A string view of the document's ID.
     */
    [[nodiscard]] std::string_view get_id() const noexcept {
        return std::string_view(id_);
    }

    /**
     * @brief Gets the document's content.
     * @return A string view of the document's content.
     */
    [[nodiscard]] std::string_view get_content() const noexcept {
        return std::string_view(content_);
    }

    /**
     * @brief Gets the document's tags.
     * @return A const reference to the set of tags.
     */
    [[nodiscard]] const std::set<std::string>& get_tags() const noexcept {
        return tags_;
    }

    /**
     * @brief Gets the document's click count.
     * @return The current click count.
     */
    [[nodiscard]] int get_click_count() const noexcept {
        return click_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Sets the document's content.
     * @param content The new content.
     * @throws DocumentValidationException if content is empty.
     */
    void set_content(String content);

    /**
     * @brief Adds a tag to the document.
     * @param tag The tag to add.
     * @throws DocumentValidationException if the tag is invalid.
     */
    void add_tag(const std::string& tag);

    /**
     * @brief Removes a tag from the document.
     * @param tag The tag to remove.
     */
    void remove_tag(const std::string& tag);

    /**
     * @brief Atomically increments the click count.
     */
    void increment_click_count() noexcept {
        click_count_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Sets the click count to a specific value.
     * @param count The new click count.
     */
    void set_click_count(int count) noexcept {
        click_count_.store(count, std::memory_order_relaxed);
    }

private:
    String id_;
    String content_;
    std::set<std::string> tags_;
    std::atomic<int> click_count_{0};
};

/**
 * @brief Enhanced search result with metadata.
 */
struct SearchResult {
    std::shared_ptr<Document> document;
    double score = 0.0;
    std::vector<std::string> matched_terms;
    std::string snippet;  ///< Content snippet with highlighted terms

    SearchResult() = default;
    SearchResult(std::shared_ptr<Document> doc, double s)
        : document(std::move(doc)), score(s) {}
};

/**
 * @brief Search results with pagination and metadata.
 */
struct SearchResults {
    std::vector<SearchResult> results;
    size_t total_count = 0;
    size_t offset = 0;
    double search_time_ms = 0.0;
    bool from_cache = false;
};

/**
 * @brief A high-performance, thread-safe, sharded search engine.
 *
 * This search engine uses a sharded architecture to provide high-concurrency
 * indexing and searching. Data is partitioned across multiple shards, each
 * with its own lock, to minimize contention and scale on multi-core systems.
 */
class SearchEngine {
public:
    /**
     * @brief Constructs the SearchEngine.
     * @param num_threads The number of worker threads for background tasks. If 0,
     * defaults to hardware concurrency.
     * @param config Search engine configuration options.
     */
    explicit SearchEngine(unsigned num_threads = 0, SearchConfig config = {});

    /**
     * @brief Destructor. Stops worker threads and cleans up resources.
     */
    ~SearchEngine();

    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
    SearchEngine(SearchEngine&&) = delete;
    SearchEngine& operator=(SearchEngine&&) = delete;

    /**
     * @brief Adds a document to the search index.
     * @param doc The document to add (l-value).
     */
    void add_document(const Document& doc);

    /**
     * @brief Adds a document to the search index.
     * @param doc The document to add (r-value).
     */
    void add_document(Document&& doc);

    /**
     * @brief Removes a document from the search index.
     * @param doc_id The ID of the document to remove.
     */
    void remove_document(const String& doc_id);

    /**
     * @brief Updates an existing document.
     * @param doc The document with updated information.
     */
    void update_document(const Document& doc);

    /**
     * @brief Searches for documents matching a single tag.
     * @param tag The tag to search for.
     * @return A vector of documents matching the tag.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> search_by_tag(
        const std::string& tag);

    /**
     * @brief Enhanced search for documents matching a single tag with pagination.
     * @param tag The tag to search for.
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults search_by_tag_enhanced(
        const std::string& tag, const SearchPagination& pagination = {});

    /**
     * @brief Performs a fuzzy search for documents by tag.
     * @param tag The tag to search for.
     * @param tolerance The maximum Levenshtein distance.
     * @return A vector of documents matching the fuzzy search.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> fuzzy_search_by_tag(
        const std::string& tag, int tolerance);

    /**
     * @brief Enhanced fuzzy search with pagination and scoring.
     * @param tag The tag to search for.
     * @param tolerance The maximum Levenshtein distance.
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults fuzzy_search_by_tag_enhanced(
        const std::string& tag, int tolerance, const SearchPagination& pagination = {});

    /**
     * @brief Searches for documents matching a list of tags.
     * @param tags The tags to search for.
     * @return A vector of documents, ranked by relevance.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> search_by_tags(
        const std::vector<std::string>& tags);

    /**
     * @brief Enhanced search for documents matching multiple tags.
     * @param tags The tags to search for.
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults search_by_tags_enhanced(
        const std::vector<std::string>& tags, const SearchPagination& pagination = {});

    /**
     * @brief Searches document content for a query string.
     * @param query The query string.
     * @return A vector of documents, ranked by relevance.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> search_by_content(
        const String& query);

    /**
     * @brief Enhanced content search with pagination and snippets.
     * @param query The query string.
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata and content snippets.
     */
    [[nodiscard]] SearchResults search_by_content_enhanced(
        const String& query, const SearchPagination& pagination = {});

    /**
     * @brief Performs a boolean search (AND, OR, NOT).
     * @param query The boolean query string.
     * @return A vector of documents matching the query.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> boolean_search(
        const String& query);

    /**
     * @brief Enhanced boolean search with full operator support.
     * @param query The boolean query string (supports AND, OR, NOT, parentheses).
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults boolean_search_enhanced(
        const String& query, const SearchPagination& pagination = {});

    /**
     * @brief Performs phrase search for exact phrase matching.
     * @param phrase The phrase to search for.
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults phrase_search(
        const String& phrase, const SearchPagination& pagination = {});

    /**
     * @brief Performs wildcard search with pattern matching.
     * @param pattern The wildcard pattern (* and ? supported).
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults wildcard_search(
        const String& pattern, const SearchPagination& pagination = {});

    /**
     * @brief Performs regex search on document content.
     * @param regex_pattern The regular expression pattern.
     * @param pagination Pagination parameters.
     * @return SearchResults with metadata.
     */
    [[nodiscard]] SearchResults regex_search(
        const String& regex_pattern, const SearchPagination& pagination = {});

    /**
     * @brief Provides autocomplete suggestions for a prefix.
     * @param prefix The prefix to complete.
     * @param max_results The maximum number of suggestions to return.
     * @return A vector of suggestion strings.
     */
    [[nodiscard]] std::vector<String> auto_complete(const String& prefix,
                                                    size_t max_results = 10);

    /**
     * @brief Enhanced autocomplete with frequency-based ranking.
     * @param prefix The prefix to complete.
     * @param max_results The maximum number of suggestions to return.
     * @return A vector of suggestion strings ranked by frequency.
     */
    [[nodiscard]] std::vector<std::pair<String, size_t>> auto_complete_ranked(
        const String& prefix, size_t max_results = 10);

    /**
     * @brief Finds documents similar to a given document.
     * @param doc_id The ID of the reference document.
     * @param max_results The maximum number of similar documents to return.
     * @param min_similarity Minimum similarity threshold (0.0 to 1.0).
     * @return A vector of similar documents with similarity scores.
     */
    [[nodiscard]] std::vector<std::pair<std::shared_ptr<Document>, double>>
    find_similar_documents(const String& doc_id, size_t max_results = 10,
                          double min_similarity = 0.1);

    /**
     * @brief Performs semantic search using document similarity.
     * @param query_text The query text to find similar documents for.
     * @param pagination Pagination parameters.
     * @return SearchResults with semantically similar documents.
     */
    [[nodiscard]] SearchResults semantic_search(
        const String& query_text, const SearchPagination& pagination = {});

    /**
     * @brief Saves the entire search index to a file.
     * @param filename The path to the file.
     */
    void save_index(const String& filename) const;

    /**
     * @brief Loads the search index from a file.
     * @param filename The path to the file.
     */
    void load_index(const String& filename);

    /**
     * @brief Gets the total number of documents in the engine.
     * @return The total number of documents.
     */
    [[nodiscard]] size_t get_document_count() const noexcept {
        return total_docs_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clears all data from the search engine.
     */
    void clear();

    /**
     * @brief Checks if a document with a given ID exists.
     * @param doc_id The document ID to check.
     * @return True if the document exists, false otherwise.
     */
    [[nodiscard]] bool has_document(const String& doc_id) const;

    /**
     * @brief Gets the IDs of all documents in the engine.
     * @return A vector of all document IDs.
     */
    [[nodiscard]] std::vector<String> get_all_document_ids() const;

    /**
     * @brief Bulk insert multiple documents efficiently.
     * @param documents Vector of documents to insert.
     * @return Number of successfully inserted documents.
     */
    size_t bulk_insert(const std::vector<Document>& documents);

    /**
     * @brief Bulk update multiple documents efficiently.
     * @param documents Vector of documents to update.
     * @return Number of successfully updated documents.
     */
    size_t bulk_update(const std::vector<Document>& documents);

    /**
     * @brief Bulk delete multiple documents efficiently.
     * @param doc_ids Vector of document IDs to delete.
     * @return Number of successfully deleted documents.
     */
    size_t bulk_delete(const std::vector<String>& doc_ids);

    /**
     * @brief Gets search performance metrics.
     * @return Current search metrics.
     */
    [[nodiscard]] const SearchMetrics& get_metrics() const noexcept {
        return metrics_;
    }

    /**
     * @brief Resets all performance metrics.
     */
    void reset_metrics() noexcept;

    /**
     * @brief Gets current search engine configuration.
     * @return Current configuration.
     */
    [[nodiscard]] const SearchConfig& get_config() const noexcept {
        return config_;
    }

    /**
     * @brief Updates search engine configuration.
     * @param config New configuration.
     */
    void update_config(const SearchConfig& config);

    /**
     * @brief Optimizes the search index for better performance.
     */
    void optimize_index();

    /**
     * @brief Gets index statistics.
     * @return Map of statistic name to value.
     */
    [[nodiscard]] std::unordered_map<std::string, size_t> get_index_stats() const;

    // Similarity calculation methods (made public for testing)
    [[nodiscard]] double calculate_cosine_similarity(const Document& doc1, const Document& doc2) const;
    [[nodiscard]] double calculate_jaccard_similarity(const Document& doc1, const Document& doc2) const;
    [[nodiscard]] std::unordered_map<String, double> create_tf_vector(const Document& doc) const;

    // Boolean query methods (made public for testing)
    struct BooleanQuery {
        enum class Operator { AND, OR, NOT };
        std::vector<std::string> terms;
        std::vector<Operator> operators;
    };
    [[nodiscard]] BooleanQuery parse_boolean_query(const String& query) const;
    [[nodiscard]] std::vector<std::shared_ptr<Document>> execute_boolean_query(const BooleanQuery& query) const;

private:
    struct Shard {
        HashMap<String, std::shared_ptr<Document>> documents;
        HashMap<std::string, std::vector<String>> tag_index;
        HashMap<String, HashSet<String>> content_index;
        HashMap<String, int> doc_frequency;

        // Performance optimizations
        HashMap<String, std::vector<String>> tokenized_content_cache; ///< Cache tokenized content
        std::unordered_map<std::pair<String, String>, double, StringPairHash> tf_idf_cache; ///< Cache TF-IDF calculations
        mutable std::atomic<size_t> cache_hits{0};
        mutable std::atomic<size_t> cache_misses{0};

        mutable std::shared_mutex mutex;
        mutable std::shared_mutex cache_mutex; ///< Separate mutex for cache operations

        // Constructor
        Shard() : cache_hits(0), cache_misses(0) {}
    };

    /**
     * @brief A thread-safe queue for asynchronous tasks.
     */
    template <typename T>
    class ConcurrentQueue {
    public:
        void push(T item) {
            {
                std::unique_lock lock(mutex_);
                queue_.push(std::move(item));
            }
            cv_.notify_one();
        }

        bool pop(T& item) {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || stop_; });
            if (stop_ && queue_.empty()) {
                return false;
            }
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void stop() {
            {
                std::unique_lock lock(mutex_);
                stop_ = true;
            }
            cv_.notify_all();
        }

    private:
        std::queue<T> queue_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool stop_ = false;
    };

    struct SearchTask {
        std::vector<String> words;
        std::function<void(const std::vector<String>&)> callback;
    };

    Shard& get_shard(const String& key) const;

    void add_content_to_index(Shard& doc_shard,
                              const std::shared_ptr<Document>& doc);
    void remove_content_from_index(Shard& doc_shard,
                                   const std::shared_ptr<Document>& doc);

    [[nodiscard]] std::vector<String> tokenize_content(
        const String& content) const;
    [[nodiscard]] double tf_idf(const Document& doc,
                              std::string_view term) const;
    [[nodiscard]] std::vector<std::shared_ptr<Document>> get_ranked_results(
        const HashMap<String, double>& scores) const;
    [[nodiscard]] int levenshtein_distance(std::string_view s1,
                                           std::string_view s2) const noexcept;

    void start_worker_threads();
    void stop_worker_threads();
    void worker_function();

    // Enhanced helper methods
    [[nodiscard]] std::string generate_cache_key(const std::string& query,
                                                 const SearchPagination& pagination) const;
    [[nodiscard]] bool get_cached_result(const std::string& cache_key, SearchResults& result) const;
    void cache_result(const std::string& cache_key, const SearchResults& result) const;
    void cleanup_expired_cache() const;

    [[nodiscard]] std::string generate_snippet(const Document& doc,
                                              const std::vector<std::string>& terms,
                                              size_t max_length = 200) const;
    [[nodiscard]] std::vector<std::string> extract_matched_terms(const Document& doc,
                                                                const std::vector<std::string>& query_terms) const;

    // Enhanced TF-IDF with caching
    [[nodiscard]] double tf_idf_cached(const Document& doc, std::string_view term) const;
    [[nodiscard]] std::vector<String> get_cached_tokens(const String& doc_id, const String& content) const;
    void cache_tokenized_content(const String& doc_id, const std::vector<String>& tokens) const;
    void invalidate_content_cache(const String& doc_id) const;

    // Stemming support
    [[nodiscard]] std::string stem_word(const std::string& word) const;
    [[nodiscard]] std::vector<String> tokenize_with_stemming(const String& content) const;

    // Performance optimization helpers
    [[nodiscard]] std::vector<String> tokenize_content_optimized(const String& content) const;
    void clear_performance_caches() const;

    // Other private helper methods

    const unsigned int num_threads_;
    std::vector<std::unique_ptr<Shard>> shards_;
    const size_t shard_mask_;
    std::atomic<size_t> total_docs_{0};

    std::unique_ptr<ConcurrentQueue<SearchTask>> task_queue_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> stop_workers_{false};

    // Configuration and metrics
    SearchConfig config_;
    mutable SearchMetrics metrics_;

    // Search result cache
    mutable std::unordered_map<std::string, std::pair<SearchResults, std::chrono::steady_clock::time_point>> result_cache_;
    mutable std::mutex cache_mutex_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_SEARCH_HPP
