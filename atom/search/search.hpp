/**
 * @file search.hpp
 * @brief Defines the Document and SearchEngine classes for Atom Search.
 * @date 2025-07-16
 */

#ifndef ATOM_SEARCH_SEARCH_HPP
#define ATOM_SEARCH_SEARCH_HPP

#include <spdlog/spdlog.h>

#include <atomic>
#include <condition_variable>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "atom/containers/high_performance.hpp"

namespace atom::search {

using atom::containers::HashMap;
using atom::containers::HashSet;
using atom::containers::String;
using atom::containers::Vector;

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
     */
    explicit SearchEngine(unsigned num_threads = 0);

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
     * @brief Performs a fuzzy search for documents by tag.
     * @param tag The tag to search for.
     * @param tolerance The maximum Levenshtein distance.
     * @return A vector of documents matching the fuzzy search.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> fuzzy_search_by_tag(
        const std::string& tag, int tolerance);

    /**
     * @brief Searches for documents matching a list of tags.
     * @param tags The tags to search for.
     * @return A vector of documents, ranked by relevance.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> search_by_tags(
        const std::vector<std::string>& tags);

    /**
     * @brief Searches document content for a query string.
     * @param query The query string.
     * @return A vector of documents, ranked by relevance.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> search_by_content(
        const String& query);

    /**
     * @brief Performs a boolean search (AND, OR, NOT).
     * @param query The boolean query string.
     * @return A vector of documents matching the query.
     */
    [[nodiscard]] std::vector<std::shared_ptr<Document>> boolean_search(
        const String& query);

    /**
     * @brief Provides autocomplete suggestions for a prefix.
     * @param prefix The prefix to complete.
     * @param max_results The maximum number of suggestions to return.
     * @return A vector of suggestion strings.
     */
    [[nodiscard]] std::vector<String> auto_complete(const String& prefix,
                                                    size_t max_results = 10);

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

private:
    struct Shard {
        HashMap<String, std::shared_ptr<Document>> documents;
        HashMap<std::string, std::vector<String>> tag_index;
        HashMap<String, HashSet<String>> content_index;
        HashMap<String, int> doc_frequency;
        mutable std::shared_mutex mutex;
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
    Shard& get_shard(const std::string& key) const;

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

    const unsigned int num_threads_;
    std::vector<std::unique_ptr<Shard>> shards_;
    const size_t shard_mask_;
    std::atomic<size_t> total_docs_{0};

    std::unique_ptr<ConcurrentQueue<SearchTask>> task_queue_;
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> stop_workers_{false};
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_SEARCH_HPP
