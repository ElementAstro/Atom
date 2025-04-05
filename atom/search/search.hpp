#ifndef ATOM_SEARCH_SEARCH_HPP
#define ATOM_SEARCH_SEARCH_HPP

#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace atom::search {

/**
 * @brief Base exception class for search engine errors.
 */
class SearchEngineException : public std::exception {
public:
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
    explicit DocumentNotFoundException(const std::string& docId)
        : SearchEngineException("Document not found: " + docId) {}
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
    explicit Document(std::string id, std::string content,
                      std::initializer_list<std::string> tags);

    /**
     * @brief Copy constructor
     * @param other Document to copy from
     */
    Document(const Document& other)
        : id_(other.id_),
          content_(other.content_),
          tags_(other.tags_),
          clickCount_(other.clickCount_.load()) {}

    /**
     * @brief Copy assignment operator
     * @param other Document to copy from
     * @return Reference to this document
     */
    Document& operator=(const Document& other) {
        if (this != &other) {
            id_ = other.id_;
            content_ = other.content_;
            tags_ = other.tags_;
            clickCount_.store(other.clickCount_.load());
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
          tags_(std::move(other.tags_)),
          clickCount_(other.clickCount_.load()) {}

    /**
     * @brief Move assignment operator
     * @param other Document to move from
     * @return Reference to this document
     */
    Document& operator=(Document&& other) noexcept {
        if (this != &other) {
            id_ = std::move(other.id_);
            content_ = std::move(other.content_);
            tags_ = std::move(other.tags_);
            clickCount_.store(other.clickCount_.load());
        }
        return *this;
    }

    /**
     * @brief Validates document fields
     * @throws DocumentValidationException if validation fails
     */
    void validate() const;

    // Getters
    std::string_view getId() const noexcept { return id_; }
    std::string_view getContent() const noexcept { return content_; }
    const std::set<std::string>& getTags() const noexcept { return tags_; }
    int getClickCount() const noexcept { return clickCount_.load(); }

    // Setters with validation
    void setContent(std::string content);
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    void incrementClickCount() noexcept { ++clickCount_; }

private:
    std::string id_;              ///< The unique identifier of the document.
    std::string content_;         ///< The content of the document.
    std::set<std::string> tags_;  ///< The tags associated with the document.
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
     * @param docId The ID of the document to remove.
     * @throws DocumentNotFoundException if the document does not exist.
     */
    void removeDocument(const std::string& docId);

    /**
     * @brief Updates an existing document in the search engine.
     * @param doc The updated document.
     * @throws DocumentNotFoundException if the document does not exist.
     * @throws DocumentValidationException if the document is invalid
     */
    void updateDocument(const Document& doc);

    /**
     * @brief Searches for documents by a specific tag.
     * @param tag The tag to search for.
     * @return A vector of shared pointers to documents that match the tag.
     */
    std::vector<std::shared_ptr<Document>> searchByTag(const std::string& tag);

    /**
     * @brief Performs a fuzzy search for documents by a tag with a specified
     * tolerance.
     * @param tag The tag to search for.
     * @param tolerance The tolerance for the fuzzy search.
     * @return A vector of shared pointers to documents that match the tag
     * within the tolerance.
     * @throws std::invalid_argument if tolerance is negative
     */
    std::vector<std::shared_ptr<Document>> fuzzySearchByTag(
        const std::string& tag, int tolerance);

    /**
     * @brief Searches for documents by multiple tags.
     * @param tags The tags to search for.
     * @return A vector of shared pointers to documents that match all the tags.
     */
    std::vector<std::shared_ptr<Document>> searchByTags(
        const std::vector<std::string>& tags);

    /**
     * @brief Searches for documents by content.
     * @param query The content query to search for.
     * @return A vector of shared pointers to documents that match the content
     * query.
     */
    std::vector<std::shared_ptr<Document>> searchByContent(
        const std::string& query);

    /**
     * @brief Performs a boolean search for documents by a query.
     * @param query The boolean query to search for.
     * @return A vector of shared pointers to documents that match the boolean
     * query.
     */
    std::vector<std::shared_ptr<Document>> booleanSearch(
        const std::string& query);

    /**
     * @brief Provides autocomplete suggestions for a given prefix.
     * @param prefix The prefix to autocomplete.
     * @param maxResults The maximum number of results to return (0 = no limit)
     * @return A vector of autocomplete suggestions.
     */
    std::vector<std::string> autoComplete(const std::string& prefix,
                                          size_t maxResults = 0);

    /**
     * @brief Saves the current index to a file.
     * @param filename The file to save the index.
     * @throws std::ios_base::failure if the file cannot be written.
     */
    void saveIndex(const std::string& filename) const;

    /**
     * @brief Loads the index from a file.
     * @param filename The file to load the index from.
     * @throws std::ios_base::failure if the file cannot be read.
     */
    void loadIndex(const std::string& filename);

private:
    /**
     * @brief Adds the content of a document to the content index.
     * @param doc The document whose content to index.
     */
    void addContentToIndex(const std::shared_ptr<Document>& doc);

    /**
     * @brief Computes the Levenshtein distance between two strings using SIMD.
     * @param s1 The first string.
     * @param s2 The second string.
     * @return The Levenshtein distance between the two strings.
     */
    int levenshteinDistanceSIMD(std::string_view s1,
                                std::string_view s2) const noexcept;

    /**
     * @brief Computes the TF-IDF score for a term in a document.
     * @param doc The document.
     * @param term The term.
     * @return The TF-IDF score for the term in the document.
     */
    double tfIdf(const Document& doc, std::string_view term) const noexcept;

    /**
     * @brief Finds a document by its ID.
     * @param docId The ID of the document.
     * @return A shared pointer to the document with the specified ID.
     * @throws DocumentNotFoundException if the document does not exist.
     */
    std::shared_ptr<Document> findDocumentById(const std::string& docId);

    /**
     * @brief Tokenizes the content into words.
     * @param content The content to tokenize.
     * @return A vector of tokens.
     */
    std::vector<std::string> tokenizeContent(const std::string& content) const;

    /**
     * @brief Gets the ranked results for a set of document scores.
     * @param scores The scores of the documents.
     * @return A vector of shared pointers to documents ranked by their scores.
     */
    std::vector<std::shared_ptr<Document>> getRankedResults(
        const std::unordered_map<std::string, double>& scores);

    /**
     * @brief Parallel worker function for searching documents by content.
     * @param wordChunk Chunk of words to process
     * @param scoresMap Map to store document scores
     * @param scoresMutex Mutex to protect the scores map
     */
    void searchByContentWorker(
        const std::vector<std::string>& wordChunk,
        std::unordered_map<std::string, double>& scoresMap,
        std::mutex& scoresMutex);

private:
    // Thread pool for parallel processing
    unsigned maxThreads_;

    // Document storage
    std::unordered_map<std::string, std::shared_ptr<Document>> documents_;

    // Indexes
    std::unordered_map<std::string, std::vector<std::string>>
        tagIndex_;  ///< Index of document ids by tags
    std::unordered_map<std::string, std::unordered_set<std::string>>
        contentIndex_;  ///< Index of document ids by content words

    // Statistics for ranking
    std::unordered_map<std::string, int>
        docFrequency_;  ///< Document frequency for terms
    std::atomic<int> totalDocs_{
        0};  ///< Total number of documents in the search engine

    // Thread safety
    mutable std::shared_mutex indexMutex_;  ///< RW mutex for thread safety
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_SEARCH_HPP
