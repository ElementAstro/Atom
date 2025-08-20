#ifndef ATOM_SEARCH_SEARCH_HPP
#define ATOM_SEARCH_SEARCH_HPP

#include <atomic>
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

#include "atom/containers/high_performance.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/container/string.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#endif

#include <spdlog/spdlog.h>

namespace atom::search {

using atom::containers::HashMap;
using atom::containers::HashSet;
using atom::containers::String;
using atom::containers::Vector;

#ifdef ATOM_USE_BOOST
namespace threading {
using thread = boost::thread;
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

#ifdef ATOM_HAS_BOOST_LOCKFREE
using atom::containers::hp::lockfree::queue;
#else
template <typename T, size_t Capacity = 1024>
using queue = boost::lockfree::queue<T, boost::lockfree::capacity<Capacity>>;
#endif
template <typename T>
using lockfree_queue = queue<T>;

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

template <typename T>
class lockfree_queue {
private:
    std::mutex mutex_;
    std::queue<T> queue_;

public:
    explicit lockfree_queue(size_t capacity [[maybe_unused]] = 128) {}

    bool push(const T& item) {
        std::lock_guard lock(mutex_);
        queue_.push(item);
        return true;
    }

    bool pop(T& item) {
        std::lock_guard lock(mutex_);
        if (queue_.empty())
            return false;
        item = queue_.front();
        queue_.pop();
        return true;
    }

    bool empty() {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    bool consume(T& item) { return pop(item); }
};
}  // namespace threading
#endif

/**
 * @brief Base exception class for search engine errors.
 */
class SearchEngineException : public std::exception {
public:
    /**
     * @brief Constructs a SearchEngineException with the given message.
     * @param message The error message
     */
    explicit SearchEngineException(std::string message)
        : message_(std::move(message)) {}

    /**
     * @brief Returns the error message.
     * @return The error message as a C-style string
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
     * @brief Constructs a DocumentNotFoundException for the given document ID.
     * @param docId The ID of the document that was not found
     */
    explicit DocumentNotFoundException(const String& docId)
        : SearchEngineException("Document not found: " + std::string(docId)) {}
};

/**
 * @brief Exception thrown when there's an issue with document validation.
 */
class DocumentValidationException : public SearchEngineException {
public:
    /**
     * @brief Constructs a DocumentValidationException with the given message.
     * @param message The validation error message
     */
    explicit DocumentValidationException(const std::string& message)
        : SearchEngineException("Document validation error: " + message) {}
};

/**
 * @brief Exception thrown when there's an issue with search operations.
 */
class SearchOperationException : public SearchEngineException {
public:
    /**
     * @brief Constructs a SearchOperationException with the given message.
     * @param message The search operation error message
     */
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
     * @param id The unique identifier of the document
     * @param content The content of the document
     * @param tags The tags associated with the document
     * @throws DocumentValidationException if validation fails
     */
    explicit Document(String id, String content,
                      std::initializer_list<std::string> tags = {});

    /**
     * @brief Copy constructor.
     * @param other Document to copy from
     */
    Document(const Document& other);

    /**
     * @brief Copy assignment operator.
     * @param other Document to copy from
     * @return Reference to this document
     */
    Document& operator=(const Document& other);

    /**
     * @brief Move constructor.
     * @param other Document to move from
     */
    Document(Document&& other) noexcept;

    /**
     * @brief Move assignment operator.
     * @param other Document to move from
     * @return Reference to this document
     */
    Document& operator=(Document&& other) noexcept;

    /**
     * @brief Default destructor.
     */
    ~Document() = default;

    /**
     * @brief Validates document fields.
     * @throws DocumentValidationException if validation fails
     */
    void validate() const;

    /**
     * @brief Gets the document ID.
     * @return The document ID as a string view
     */
    std::string_view getId() const noexcept { return std::string_view(id_); }

    /**
     * @brief Gets the document content.
     * @return The document content as a string view
     */
    std::string_view getContent() const noexcept {
        return std::string_view(content_);
    }

    /**
     * @brief Gets the document tags.
     * @return A const reference to the set of tags
     */
    const std::set<std::string>& getTags() const noexcept { return tags_; }

    /**
     * @brief Gets the click count.
     * @return The current click count
     */
    int getClickCount() const noexcept {
        return clickCount_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Sets the document content.
     * @param content The new content for the document
     * @throws DocumentValidationException if content is empty
     */
    void setContent(String content);

    /**
     * @brief Adds a tag to the document.
     * @param tag The tag to add
     * @throws DocumentValidationException if tag is invalid
     */
    void addTag(const std::string& tag);

    /**
     * @brief Removes a tag from the document.
     * @param tag The tag to remove
     */
    void removeTag(const std::string& tag);

    /**
     * @brief Increments the click count atomically.
     */
    void incrementClickCount() noexcept {
        clickCount_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Sets the click count.
     * @param count The new click count
     */
    void setClickCount(int count) noexcept {
        clickCount_.store(count, std::memory_order_relaxed);
    }

    /**
     * @brief Resets the click count to zero.
     */
    void resetClickCount() noexcept {
        clickCount_.store(0, std::memory_order_relaxed);
    }

private:
    String id_;
    String content_;
    std::set<std::string> tags_;
    std::atomic<int> clickCount_{0};
};

/**
 * @brief A high-performance search engine for indexing and searching documents.
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
     * @brief Non-copyable.
     */
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;

    /**
     * @brief Non-movable.
     */
    SearchEngine(SearchEngine&&) = delete;
    SearchEngine& operator=(SearchEngine&&) = delete;

    /**
     * @brief Adds a document to the search engine.
     * @param doc The document to add
     * @throws std::invalid_argument if the document ID already exists
     * @throws DocumentValidationException if the document is invalid
     */
    void addDocument(const Document& doc);

    /**
     * @brief Adds a document to the search engine using move semantics.
     * @param doc The document to add
     * @throws std::invalid_argument if the document ID already exists
     * @throws DocumentValidationException if the document is invalid
     */
    void addDocument(Document&& doc);

    /**
     * @brief Removes a document from the search engine.
     * @param docId The ID of the document to remove
     * @throws DocumentNotFoundException if the document does not exist
     */
    void removeDocument(const String& docId);

    /**
     * @brief Updates an existing document in the search engine.
     * @param doc The updated document
     * @throws DocumentNotFoundException if the document does not exist
     * @throws DocumentValidationException if the document is invalid
     */
    void updateDocument(const Document& doc);

    /**
     * @brief Searches for documents by a specific tag.
     * @param tag The tag to search for
     * @return A vector of shared pointers to documents that match the tag
     */
    std::vector<std::shared_ptr<Document>> searchByTag(const std::string& tag);

    /**
     * @brief Performs a fuzzy search for documents by a tag with a specified
     * tolerance.
     * @param tag The tag to search for
     * @param tolerance The tolerance for the fuzzy search
     * @return A vector of shared pointers to documents that match the tag
     * within the tolerance
     * @throws std::invalid_argument if tolerance is negative
     */
    std::vector<std::shared_ptr<Document>> fuzzySearchByTag(
        const std::string& tag, int tolerance);

    /**
     * @brief Searches for documents by multiple tags.
     * @param tags The tags to search for
     * @return A vector of shared pointers to documents that match all the tags
     */
    std::vector<std::shared_ptr<Document>> searchByTags(
        const std::vector<std::string>& tags);

    /**
     * @brief Searches for documents by content.
     * @param query The content query to search for
     * @return A vector of shared pointers to documents that match the content
     * query
     */
    std::vector<std::shared_ptr<Document>> searchByContent(const String& query);

    /**
     * @brief Performs a boolean search for documents by a query.
     * @param query The boolean query to search for
     * @return A vector of shared pointers to documents that match the boolean
     * query
     */
    std::vector<std::shared_ptr<Document>> booleanSearch(const String& query);

    /**
     * @brief Provides autocomplete suggestions for a given prefix.
     * @param prefix The prefix to autocomplete
     * @param maxResults The maximum number of results to return (0 = no limit)
     * @return A vector of autocomplete suggestions
     */
    std::vector<String> autoComplete(const String& prefix,
                                     size_t maxResults = 0);

    /**
     * @brief Saves the current index to a file.
     * @param filename The file to save the index
     * @throws std::ios_base::failure if the file cannot be written
     */
    void saveIndex(const String& filename) const;

    /**
     * @brief Loads the index from a file.
     * @param filename The file to load the index from
     * @throws std::ios_base::failure if the file cannot be read
     */
    void loadIndex(const String& filename);

    /**
     * @brief Gets the total number of documents in the search engine.
     * @return The total document count
     */
    size_t getDocumentCount() const noexcept {
        return totalDocs_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clears all documents and indexes.
     */
    void clear();

    /**
     * @brief Checks if a document exists.
     * @param docId The document ID to check
     * @return True if the document exists, false otherwise
     */
    bool hasDocument(const String& docId) const;

    /**
     * @brief Gets all document IDs.
     * @return A vector of all document IDs
     */
    std::vector<String> getAllDocumentIds() const;

private:
    /**
     * @brief Adds the content of a document to the content index.
     * @param doc The document whose content to index
     */
    void addContentToIndex(const std::shared_ptr<Document>& doc);

    /**
     * @brief Computes the Levenshtein distance between two strings.
     * @param s1 The first string
     * @param s2 The second string
     * @return The Levenshtein distance between the two strings
     */
    int levenshteinDistanceSIMD(std::string_view s1,
                                std::string_view s2) const noexcept;

    /**
     * @brief Computes the TF-IDF score for a term in a document.
     * @param doc The document
     * @param term The term
     * @return The TF-IDF score for the term in the document
     */
    double tfIdf(const Document& doc, std::string_view term) const noexcept;

    /**
     * @brief Finds a document by its ID.
     * @param docId The ID of the document
     * @return A shared pointer to the document with the specified ID
     * @throws DocumentNotFoundException if the document does not exist
     */
    std::shared_ptr<Document> findDocumentById(const String& docId);

    /**
     * @brief Tokenizes the content into words.
     * @param content The content to tokenize
     * @return A vector of tokens
     */
    std::vector<String> tokenizeContent(const String& content) const;

    /**
     * @brief Gets the ranked results for a set of document scores.
     * @param scores The scores of the documents
     * @return A vector of shared pointers to documents ranked by their scores
     */
    std::vector<std::shared_ptr<Document>> getRankedResults(
        const HashMap<String, double>& scores);

    /**
     * @brief Parallel worker function for searching documents by content.
     * @param wordChunk Chunk of words to process
     * @param scoresMap Map to store document scores
     * @param scoresMutex Mutex to protect the scores map
     */
    void searchByContentWorker(const std::vector<String>& wordChunk,
                               HashMap<String, double>& scoresMap,
                               threading::mutex& scoresMutex);

    /**
     * @brief Starts worker threads for processing tasks.
     */
    void startWorkerThreads();

    /**
     * @brief Stops worker threads.
     */
    void stopWorkerThreads();

    /**
     * @brief Worker thread function.
     */
    void workerFunction();

private:
    unsigned maxThreads_;
    HashMap<String, std::shared_ptr<Document>> documents_;
    HashMap<std::string, std::vector<String>> tagIndex_;
    HashMap<String, HashSet<String>> contentIndex_;
    HashMap<String, int> docFrequency_;
    std::atomic<int> totalDocs_{0};
    mutable threading::shared_mutex indexMutex_;

    struct SearchTask {
        std::vector<String> words;
        std::function<void(const std::vector<String>&)> callback;
    };

    std::unique_ptr<threading::lockfree_queue<SearchTask>> taskQueue_;
    std::atomic<bool> shouldStopWorkers_{false};
    std::vector<std::unique_ptr<threading::thread>> workerThreads_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_SEARCH_HPP
