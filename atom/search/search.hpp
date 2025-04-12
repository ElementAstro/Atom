#ifndef ATOM_SEARCH_SEARCH_HPP
#define ATOM_SEARCH_SEARCH_HPP

#include <atomic>
#include <cmath>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

// Include the high-performance container aliases
#include "atom/containers/high_performance.hpp"

// Add Boost support with conditional compilation
#ifdef ATOM_USE_BOOST
#include <boost/container/string.hpp>  // Include boost::container::string if used
#include <boost/lockfree/queue.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#endif

namespace atom::search {

// Use container aliases from atom::containers
using atom::containers::HashMap;
using atom::containers::HashSet;
using atom::containers::String;  // Use the String alias
using atom::containers::Vector;
// using atom::containers::Set; // Assuming std::set is intended for tags, or
// define Set alias

// Define threading and synchronization types based on configuration
#ifdef ATOM_USE_BOOST
namespace threading {
using thread = boost::thread;
// using thread_group = boost::thread_group; // Not used in the current code
using mutex = boost::mutex;
using shared_mutex = boost::shared_mutex;
using unique_lock = boost::unique_lock<mutex>;
using shared_lock = boost::shared_lock<shared_mutex>;

template <typename T>
using future = boost::future<T>;
template <typename T>
using shared_future = boost::shared_future<T>;
template <typename T>
using promise = boost::promise<T>;

// Use the lockfree queue definition from high_performance.hpp if available and
// configured
#ifdef ATOM_HAS_BOOST_LOCKFREE
using atom::containers::hp::lockfree::queue;
#else
// Fallback if ATOM_HAS_BOOST_LOCKFREE is not defined but ATOM_USE_BOOST is
template <typename T, size_t Capacity = 1024>
using queue = boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>>;
#endif
template <typename T>
using lockfree_queue = queue<T>;  // Alias for consistency within this file

}  // namespace threading
#else
namespace threading {
using thread = std::thread;
using mutex = std::mutex;
using shared_mutex = std::shared_mutex;
using unique_lock = std::unique_lock<mutex>;
using shared_lock = std::shared_lock<shared_mutex>;

template <typename T>
using future = std::future<T>;
template <typename T>
using shared_future = std::shared_future<T>;
template <typename T>
using promise = std::promise<T>;

// Use the placeholder lockfree_queue (mutex-based) when not using Boost
template <typename T>
class lockfree_queue {
private:
    std::mutex mutex_;
    std::queue<T> queue_;
    // Consider adding capacity if needed, though the std::queue doesn't enforce
    // it size_t capacity_;

public:
    lockfree_queue(size_t capacity
                   [[maybe_unused]] = 128) /*: capacity_(capacity)*/ {}
    bool push(const T& item) {
        std::lock_guard lock(mutex_);
        // if (queue_.size() >= capacity_) return false; // Optional capacity
        // check
        queue_.push(item);
        return true;
    }
    bool pop(T& item) {
        std::lock_guard lock(mutex_);
        if (queue_.empty())
            return false;
        item = queue_.front();  // Consider std::move if T is movable
        queue_.pop();
        return true;
    }
    bool empty() {
        // Use std::scoped_lock or std::lock_guard for RAII
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }
    // Add a non-blocking pop for potential future use, similar to Boost's API
    bool consume(T& item) {
        return pop(item);  // Simple alias for now
    }
};
}  // namespace threading
#endif

/**
 * @brief Base exception class for search engine errors.
 */
class SearchEngineException : public std::exception {
public:
    // Use std::string for exception messages for simplicity
    explicit SearchEngineException(std::string message)
        : message_(std::move(message)) {}
    const char* what() const noexcept override { return message_.c_str(); }

protected:
    std::string message_;
};

/**
 * @brief Exception thrown when a document is not found.
 */
class DocumentNotFoundException : public SearchEngineException {
public:
    explicit DocumentNotFoundException(const String& docId)
        : SearchEngineException("Document not found: " + std::string(docId)) {
    }  // Convert String to std::string if needed
};

/**
 * @brief Exception thrown when there's an issue with document validation.
 */
class DocumentValidationException : public SearchEngineException {
public:
    explicit DocumentValidationException(const std::string& message)
        : SearchEngineException("Document validation error: " + message) {}
};

/**
 * @brief Exception thrown when there's an issue with search operations.
 */
class SearchOperationException : public SearchEngineException {
public:
    explicit SearchOperationException(const std::string& message)
        : SearchEngineException("Search operation error: " + message) {}
};

/**
 * @brief Represents a document with an ID, content, tags, and click count.
 */
class Document {
public:
    /**
     * @brief Constructs a Document object.
     * @param id The unique identifier of the document.
     * @param content The content of the document.
     * @param tags The tags associated with the document.
     * @throws DocumentValidationException if validation fails
     */
    // Use String for id and content, std::string for tags in initializer list
    explicit Document(String id, String content,
                      std::initializer_list<std::string> tags);

    /**
     * @brief Copy constructor
     * @param other Document to copy from
     */
    Document(const Document& other)
        : id_(other.id_),
          content_(other.content_),
          tags_(other.tags_),  // std::set copy constructor
          clickCount_(other.clickCount_.load(std::memory_order_relaxed)) {}

    /**
     * @brief Copy assignment operator
     * @param other Document to copy from
     * @return Reference to this document
     */
    Document& operator=(const Document& other) {
        if (this != &other) {
            id_ = other.id_;
            content_ = other.content_;
            tags_ = other.tags_;  // std::set copy assignment
            clickCount_.store(other.clickCount_.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
        }
        return *this;
    }

    /**
     * @brief Move constructor
     * @param other Document to move from
     */
    Document(Document&& other) noexcept
        : id_(std::move(other.id_)),
          content_(std::move(other.content_)),
          tags_(std::move(other.tags_)),  // std::set move constructor
          clickCount_(other.clickCount_.load(std::memory_order_relaxed)) {}

    /**
     * @brief Move assignment operator
     * @param other Document to move from
     * @return Reference to this document
     */
    Document& operator=(Document&& other) noexcept {
        if (this != &other) {
            id_ = std::move(other.id_);
            content_ = std::move(other.content_);
            tags_ = std::move(other.tags_);  // std::set move assignment
            clickCount_.store(other.clickCount_.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
        }
        return *this;
    }

    /**
     * @brief Validates document fields
     * @throws DocumentValidationException if validation fails
     */
    void validate() const;

    // Getters
    // Return string_view for efficiency, assuming String is compatible
    std::string_view getId() const noexcept { return std::string_view(id_); }
    std::string_view getContent() const noexcept {
        return std::string_view(content_);
    }
    // Keep std::set for tags for now
    const std::set<std::string>& getTags() const noexcept { return tags_; }
    int getClickCount() const noexcept {
        return clickCount_.load(std::memory_order_relaxed);
    }

    // Setters with validation
    void setContent(String content);
    // Use std::string for tag parameters for now
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    void incrementClickCount() noexcept {
        clickCount_.fetch_add(1, std::memory_order_relaxed);
    }

private:
    String id_;  ///< The unique identifier of the document. Use String alias.
    String content_;  ///< The content of the document. Use String alias.
    std::set<std::string> tags_;  ///< The tags associated with the document.
                                  ///< Keep std::set or use containers::Set.
    std::atomic<int> clickCount_{
        0};  ///< The click count used to adjust the document's weight.
};

/**
 * @brief A search engine for indexing and searching documents.
 */
class SearchEngine {
public:
    /**
     * @brief Constructs a SearchEngine with optional parallelism settings.
     * @param maxThreads Maximum number of threads to use (0 = use hardware
     * concurrency)
     */
    explicit SearchEngine(unsigned maxThreads = 0);

    /**
     * @brief Destructor - cleans up thread resources.
     */
    ~SearchEngine();

    /**
     * @brief Adds a document to the search engine.
     * @param doc The document to add.
     * @throws std::invalid_argument if the document ID already exists.
     * @throws DocumentValidationException if the document is invalid
     */
    void addDocument(const Document& doc);

    /**
     * @brief Adds a document to the search engine using move semantics.
     * @param doc The document to add.
     * @throws std::invalid_argument if the document ID already exists.
     * @throws DocumentValidationException if the document is invalid
     */
    void addDocument(Document&& doc);

    /**
     * @brief Removes a document from the search engine.
     * @param docId The ID of the document to remove. Use String.
     * @throws DocumentNotFoundException if the document does not exist.
     */
    void removeDocument(const String& docId);

    /**
     * @brief Updates an existing document in the search engine.
     * @param doc The updated document.
     * @throws DocumentNotFoundException if the document does not exist.
     * @throws DocumentValidationException if the document is invalid
     */
    void updateDocument(const Document& doc);

    /**
     * @brief Searches for documents by a specific tag.
     * @param tag The tag to search for. Use std::string for parameter.
     * @return A vector of shared pointers to documents that match the tag.
     */
    // Keep std::vector for return type unless Vector alias is intended
    std::vector<std::shared_ptr<Document>> searchByTag(const std::string& tag);

    /**
     * @brief Performs a fuzzy search for documents by a tag with a specified
     * tolerance.
     * @param tag The tag to search for. Use std::string for parameter.
     * @param tolerance The tolerance for the fuzzy search.
     * @return A vector of shared pointers to documents that match the tag
     * within the tolerance.
     * @throws std::invalid_argument if tolerance is negative
     */
    std::vector<std::shared_ptr<Document>> fuzzySearchByTag(
        const std::string& tag, int tolerance);

    /**
     * @brief Searches for documents by multiple tags.
     * @param tags The tags to search for. Use std::vector<std::string>.
     * @return A vector of shared pointers to documents that match all the tags.
     */
    std::vector<std::shared_ptr<Document>> searchByTags(
        const std::vector<std::string>& tags);

    /**
     * @brief Searches for documents by content.
     * @param query The content query to search for. Use String.
     * @return A vector of shared pointers to documents that match the content
     * query.
     */
    std::vector<std::shared_ptr<Document>> searchByContent(const String& query);

    /**
     * @brief Performs a boolean search for documents by a query.
     * @param query The boolean query to search for. Use String.
     * @return A vector of shared pointers to documents that match the boolean
     * query.
     */
    std::vector<std::shared_ptr<Document>> booleanSearch(const String& query);

    /**
     * @brief Provides autocomplete suggestions for a given prefix.
     * @param prefix The prefix to autocomplete. Use String.
     * @param maxResults The maximum number of results to return (0 = no limit)
     * @return A vector of autocomplete suggestions (use std::vector<String> or
     * Vector<String>).
     */
    // Return std::vector<String> or Vector<String>
    std::vector<String> autoComplete(const String& prefix,
                                     size_t maxResults = 0);

    /**
     * @brief Saves the current index to a file.
     * @param filename The file to save the index. Use String.
     * @throws std::ios_base::failure if the file cannot be written.
     */
    void saveIndex(const String& filename) const;

    /**
     * @brief Loads the index from a file.
     * @param filename The file to load the index from. Use String.
     * @throws std::ios_base::failure if the file cannot be read.
     */
    void loadIndex(const String& filename);

private:
    /**
     * @brief Adds the content of a document to the content index.
     * @param doc The document whose content to index.
     */
    void addContentToIndex(const std::shared_ptr<Document>& doc);

    /**
     * @brief Computes the Levenshtein distance between two strings using SIMD.
     * @param s1 The first string. Use std::string_view.
     * @param s2 The second string. Use std::string_view.
     * @return The Levenshtein distance between the two strings.
     */
    int levenshteinDistanceSIMD(std::string_view s1,
                                std::string_view s2) const noexcept;

    /**
     * @brief Computes the TF-IDF score for a term in a document.
     * @param doc The document.
     * @param term The term. Use std::string_view.
     * @return The TF-IDF score for the term in the document.
     */
    double tfIdf(const Document& doc, std::string_view term) const noexcept;

    /**
     * @brief Finds a document by its ID.
     * @param docId The ID of the document. Use String.
     * @return A shared pointer to the document with the specified ID.
     * @throws DocumentNotFoundException if the document does not exist.
     */
    std::shared_ptr<Document> findDocumentById(const String& docId);

    /**
     * @brief Tokenizes the content into words.
     * @param content The content to tokenize. Use String.
     * @return A vector of tokens (use std::vector<String> or Vector<String>).
     */
    // Return std::vector<String> or Vector<String>
    std::vector<String> tokenizeContent(const String& content) const;

    /**
     * @brief Gets the ranked results for a set of document scores.
     * @param scores The scores of the documents (use HashMap<String, double>).
     * @return A vector of shared pointers to documents ranked by their scores.
     */
    std::vector<std::shared_ptr<Document>> getRankedResults(
        const HashMap<String, double>& scores);

    /**
     * @brief Parallel worker function for searching documents by content.
     * @param wordChunk Chunk of words to process (use std::vector<String> or
     * Vector<String>).
     * @param scoresMap Map to store document scores (use HashMap<String,
     * double>).
     * @param scoresMutex Mutex to protect the scores map (use
     * threading::mutex).
     */
    void searchByContentWorker(
        const std::vector<String>& wordChunk,  // Or Vector<String>
        HashMap<String, double>& scoresMap,
        threading::mutex& scoresMutex);  // Use threading::mutex

private:
    // Thread pool for parallel processing
    unsigned maxThreads_;

    // Document storage - Use HashMap<String, ...>
    HashMap<String, std::shared_ptr<Document>> documents_;

    // Indexes - Use HashMap<String, ...> and HashSet<String>
    // Keep std::vector for tagIndex value for now, or use Vector<String>
    HashMap<std::string, std::vector<String>>
        tagIndex_;  ///< Index of document ids by tags (key std::string, value
                    ///< std::vector<String>)
    HashMap<String, HashSet<String>>
        contentIndex_;  ///< Index of document ids by content words (key String,
                        ///< value HashSet<String>)

    // Statistics for ranking - Use HashMap<String, int>
    HashMap<String, int> docFrequency_;  ///< Document frequency for terms
    std::atomic<int> totalDocs_{
        0};  ///< Total number of documents in the search engine

    // Thread safety - Use threading::shared_mutex
    mutable threading::shared_mutex
        indexMutex_;  ///< RW mutex for thread safety

#if defined(ATOM_USE_BOOST) || \
    !defined(ATOM_USE_BOOST)  // Apply task queue logic regardless of Boost for
                              // std::thread version too
    // Task queue for parallel processing (using threading::lockfree_queue)
    struct SearchTask {
        std::vector<String> words;  // Or Vector<String>
        // std::function is fine here
        std::function<void(const std::vector<String>&)> callback;
    };

    // Use the threading::lockfree_queue alias
    std::unique_ptr<threading::lockfree_queue<SearchTask>> taskQueue_;
    std::atomic<bool> shouldStopWorkers_{false};
    // Use std::vector or Vector for threads
    std::vector<std::unique_ptr<threading::thread>> workerThreads_;

    // Start worker threads for processing tasks
    void startWorkerThreads();
    // Stop worker threads
    void stopWorkerThreads();
    // Worker thread function
    void workerFunction();
#endif
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_SEARCH_HPP
