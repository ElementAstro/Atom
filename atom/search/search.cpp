#include "search.hpp"

#include <immintrin.h>  // For SIMD
#include <algorithm>
#include <cctype>
#include <cstring>  // Keep for standard C functions if needed
#include <fstream>
#include <functional>  // Keep for std::function
#include <queue>       // Keep for std::priority_queue
#include <regex>
#include <sstream>

// Conditional includes based on threading choice
#ifdef ATOM_USE_BOOST
// Boost includes are handled in search.hpp
#include <boost/algorithm/string.hpp>  // Keep if used for specific algorithms not covered by std::
// #include <boost/bind/bind.hpp> // Not used, remove
#include <boost/chrono.hpp>         // For boost::chrono::milliseconds
#include <boost/thread/thread.hpp>  // For boost::this_thread::sleep_for
#else
#include <chrono>  // For std::chrono::milliseconds
#include <future>  // Keep for std::async
#include <thread>  // For std::this_thread::sleep_for
#endif

// Assuming loguru supports std::string or requires conversion
#include "atom/log/loguru.hpp"

namespace atom::search {

// Document implementation
// Use String alias for id and content parameters
Document::Document(String id, String content,
                   std::initializer_list<std::string> tags)
    : id_(std::move(id)), content_(std::move(content)), tags_(tags) {
    validate();
    // Loguru might need std::string, convert String if necessary
    LOG_F(INFO, "Document created with id: {}", std::string(id_));
}

void Document::validate() const {
    if (id_.empty()) {
        throw DocumentValidationException("Document ID cannot be empty");
    }

    // Use size() method for String
    if (id_.size() > 256) {
        throw DocumentValidationException(
            "Document ID too long (max 256 chars)");
    }

    if (content_.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }

    // Check for any invalid tags (tags_ is std::set<std::string>)
    for (const auto& tag : tags_) {
        if (tag.empty()) {
            throw DocumentValidationException("Tags cannot be empty");
        }
        if (tag.length() > 100) {
            throw DocumentValidationException("Tag too long (max 100 chars): " +
                                              tag);
        }
    }
}

// Use String alias for content parameter
void Document::setContent(String content) {
    if (content.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }
    content_ = std::move(content);
}

// Parameter is std::string as per header
void Document::addTag(const std::string& tag) {
    if (tag.empty()) {
        throw DocumentValidationException("Tag cannot be empty");
    }
    if (tag.length() > 100) {
        throw DocumentValidationException("Tag too long (max 100 chars): " +
                                          tag);
    }
    tags_.insert(tag);
}

// Parameter is std::string as per header
void Document::removeTag(const std::string& tag) { tags_.erase(tag); }

// SearchEngine implementation
SearchEngine::SearchEngine(unsigned maxThreads)
    : maxThreads_(maxThreads ? maxThreads
#ifdef ATOM_USE_BOOST
                             : boost::thread::hardware_concurrency())
#else
                             : std::thread::hardware_concurrency())
#endif
{
    LOG_F(INFO, "SearchEngine initialized with max threads: {}", maxThreads_);

// Apply task queue logic regardless of Boost
#if defined(ATOM_USE_BOOST) || !defined(ATOM_USE_BOOST)
    // Initialize task queue and worker threads using threading aliases
    taskQueue_ = std::make_unique<threading::lockfree_queue<SearchTask>>(
        1024);  // Capacity example
    startWorkerThreads();
    LOG_F(INFO, "Task queue initialized with {} worker threads", maxThreads_);
#endif
}

SearchEngine::~SearchEngine() {
    LOG_F(INFO, "SearchEngine being destroyed");

// Apply task queue logic regardless of Boost
#if defined(ATOM_USE_BOOST) || !defined(ATOM_USE_BOOST)
    // Clean up thread pool
    stopWorkerThreads();
    LOG_F(INFO, "Worker threads stopped and cleaned up");
#endif
}

// Apply task queue logic regardless of Boost
#if defined(ATOM_USE_BOOST) || !defined(ATOM_USE_BOOST)
void SearchEngine::startWorkerThreads() {
    // Create worker threads using threading::thread
    shouldStopWorkers_.store(false);
    workerThreads_.reserve(maxThreads_);

    for (unsigned i = 0; i < maxThreads_; ++i) {
        // Use threading::thread
        workerThreads_.push_back(std::make_unique<threading::thread>(
            [this]() { workerFunction(); }));
    }
    LOG_F(INFO, "Started {} worker threads", maxThreads_);
}

void SearchEngine::stopWorkerThreads() {
    LOG_F(INFO, "Stopping worker threads");
    shouldStopWorkers_.store(true);

    // Wait for all threads to finish
    for (auto& thread : workerThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    workerThreads_.clear();
    LOG_F(INFO, "All worker threads stopped");
}

void SearchEngine::workerFunction() {
    SearchTask task;

    while (!shouldStopWorkers_.load()) {
        // Use taskQueue_->pop or taskQueue_->consume depending on the
        // lockfree_queue implementation
        if (taskQueue_->pop(task)) {  // Assuming pop is the blocking/waiting
                                      // call or equivalent
            try {
                // Execute the task
                task.callback(task.words);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error in worker thread: {}", e.what());
            }
        } else {
            // Sleep briefly to avoid busy waiting
            // Use appropriate sleep for the threading model
#ifdef ATOM_USE_BOOST
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
        }
    }
}
#endif

void SearchEngine::addDocument(const Document& doc) {
    try {
        // Create a shared pointer from the document
        // Use String(doc.getId()) for logging if needed
        LOG_F(INFO, "Adding document copy with id: {}",
              std::string(doc.getId()));
        // Create a temporary movable document to pass to the move overload
        Document tempDoc = doc;           // Copy constructor
        addDocument(std::move(tempDoc));  // Call move overload
    } catch (const DocumentValidationException& e) {
        LOG_F(ERROR, "Failed to add document copy: {}", e.what());
        throw;
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Failed to add document copy: {}", e.what());
        throw;
    }
}

void SearchEngine::addDocument(Document&& doc) {
    // Use String(doc.getId()) for logging
    LOG_F(INFO, "Adding document move with id: {}", std::string(doc.getId()));

    // Validation
    try {
        doc.validate();
    } catch (const DocumentValidationException& e) {
        LOG_F(ERROR, "Document validation failed: {}", e.what());
        throw;
    }

    // Use unique_lock with the appropriate mutex type
    std::unique_lock<threading::shared_mutex> lock(indexMutex_);
    // Use String for docId, convert from string_view
    String docId = String(doc.getId());

    // Check if document already exists using String key
    if (documents_.count(docId) > 0) {
        // Use std::string() for logging if needed
        LOG_F(ERROR, "Document with ID {} already exists", std::string(docId));
        throw std::invalid_argument("Document with this ID already exists");
    }

    // Add to documents collection (HashMap<String, std::shared_ptr<Document>>)
    auto docPtr = std::make_shared<Document>(std::move(doc));
    documents_[docId] = docPtr;  // docId is already String type

    // Add to tag index (HashMap<std::string, std::vector<String>>)
    for (const auto& tag :
         docPtr->getTags()) {  // getTags returns std::set<std::string>
        tagIndex_[tag].push_back(
            docId);  // Add String docId to std::vector<String>
        // docFrequency_ is HashMap<String, int>, use String(tag) if key needs
        // to be String Assuming docFrequency_ key is std::string based on tfIdf
        // usage
        docFrequency_[tag]++;  // Use std::string tag as key
        LOG_F(INFO, "Tag '{}' added to index for doc {}", tag,
              std::string(docId));
    }

    // Add to content index (calls addContentToIndex which handles String)
    addContentToIndex(docPtr);

    // Increment document count
    totalDocs_++;
    LOG_F(INFO, "Document added successfully, total docs: {}",
          totalDocs_.load());
}

// Use String alias for docId parameter
void SearchEngine::removeDocument(const String& docId) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Removing document with id: {}", std::string(docId));

    if (docId.empty()) {
        throw std::invalid_argument("Document ID cannot be empty");
    }

    // Use unique_lock with the appropriate mutex type
    std::unique_lock<threading::shared_mutex> lock(indexMutex_);

    // Check if document exists using String key
    auto docIt = documents_.find(docId);
    if (docIt == documents_.end()) {
        LOG_F(ERROR, "Document with ID {} not found", std::string(docId));
        throw DocumentNotFoundException(docId);
    }

    auto& doc = docIt->second;

    // Remove from tagIndex_ (HashMap<std::string, std::vector<String>>)
    for (const auto& tag : doc->getTags()) {  // tag is std::string
        auto tagIt = tagIndex_.find(tag);
        if (tagIt != tagIndex_.end()) {
            auto& docsVec = tagIt->second;  // std::vector<String>
            // Use std::remove on std::vector<String>
            docsVec.erase(std::remove(docsVec.begin(), docsVec.end(), docId),
                          docsVec.end());

            if (docsVec.empty()) {
                tagIndex_.erase(tagIt);
            }
        }

        // Update document frequency (HashMap<String, int>)
        // Assuming key is std::string
        auto freqIt = docFrequency_.find(tag);
        if (freqIt != docFrequency_.end()) {
            if (--(freqIt->second) <= 0) {
                docFrequency_.erase(freqIt);
            }
        }
    }

    // Remove from contentIndex_ (HashMap<String, HashSet<String>>)
    // tokenizeContent returns std::vector<String>
    auto tokens = tokenizeContent(String(doc->getContent()));  // Pass String
    for (const auto& token : tokens) {  // token is String
        auto contentIt = contentIndex_.find(token);
        if (contentIt != contentIndex_.end()) {
            auto& docsSet = contentIt->second;  // HashSet<String>
            docsSet.erase(docId);  // Erase String from HashSet<String>

            if (docsSet.empty()) {
                contentIndex_.erase(contentIt);
            }
        }

        // Update document frequency (HashMap<String, int>)
        // Assuming key is std::string, convert token
        auto freqIt = docFrequency_.find(std::string(token));
        if (freqIt != docFrequency_.end()) {
            if (--(freqIt->second) <= 0) {
                docFrequency_.erase(freqIt);
            }
        }
    }

    // Remove from documents collection (HashMap<String, ...>)
    documents_.erase(docIt);
    totalDocs_--;

    LOG_F(INFO, "Document with id: {} removed, total docs: {}",
          std::string(docId), totalDocs_.load());
}

void SearchEngine::updateDocument(const Document& doc) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Updating document with id: {}", std::string(doc.getId()));

    try {
        // Validate document
        doc.validate();

        // Use unique_lock with the appropriate mutex type
        std::unique_lock<threading::shared_mutex> lock(indexMutex_);

        // Use String for docId, convert from string_view
        String docId = String(doc.getId());

        // Check if document exists using String key
        if (documents_.find(docId) == documents_.end()) {
            LOG_F(ERROR, "Document with ID {} not found", std::string(docId));
            throw DocumentNotFoundException(docId);
        }

        // Temporarily unlock to call removeDocument and addDocument which lock
        // internally This is complex and potentially racy if not careful. A
        // better approach might be to refactor remove/add logic to work under a
        // single lock. For now, let's assume removeDocument and addDocument
        // handle locking correctly.
        // *** WARNING: Potential for issues if remove/add logic changes ***
        lock.unlock();  // Unlock before calling other methods that lock

        // Remove old document (pass String)
        removeDocument(docId);

        // Add updated document (pass const ref, addDocument(const Document&)
        // handles it)
        addDocument(doc);

        LOG_F(INFO, "Document with id: {} updated", std::string(docId));
    } catch (const DocumentNotFoundException& e) {
        LOG_F(ERROR, "Error updating document (not found): {}", e.what());
        throw;
    } catch (const DocumentValidationException& e) {
        LOG_F(ERROR, "Error updating document (validation): {}", e.what());
        throw;
    } catch (const std::invalid_argument& e) {
        LOG_F(ERROR, "Error updating document (invalid arg): {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error updating document: {}", e.what());
        throw;  // Re-throw as generic SearchOperationException?
    }
}

void SearchEngine::addContentToIndex(const std::shared_ptr<Document>& doc) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Indexing content for document id: {}",
          std::string(doc->getId()));

    // Use String for docId and content
    String docId = String(doc->getId());
    String content = String(doc->getContent());

    // tokenizeContent takes String and returns std::vector<String>
    auto tokens = tokenizeContent(content);

    for (const auto& token : tokens) {  // token is String
        // contentIndex_ is HashMap<String, HashSet<String>>
        contentIndex_[token].insert(
            docId);  // Insert String into HashSet<String>
        // docFrequency_ is HashMap<String, int>, assuming key is std::string
        docFrequency_[std::string(
            token)]++;  // Convert String token to std::string key
        LOG_F(INFO, "Token '{}' indexed for document id: {}",
              std::string(token), std::string(docId));
    }
}

// Parameter and return type use String alias as per header
std::vector<String> SearchEngine::tokenizeContent(const String& content) const {
    // Use std::vector<String> for tokens
    std::vector<String> tokens;
    // Use std::stringstream with std::string conversion - Fix Most Vexing Parse
    std::stringstream ss{std::string(content)};
    std::string tokenStd;  // Use std::string for stream extraction

    // Simple tokenization by whitespace
    while (ss >> tokenStd) {
        // Convert to lowercase and remove non-alphanumeric characters using
        // std::string
        tokenStd = std::regex_replace(tokenStd, std::regex("[^a-zA-Z0-9]"), "");

        // Only add non-empty tokens
        if (!tokenStd.empty()) {
            // Convert to lowercase
            std::transform(tokenStd.begin(), tokenStd.end(), tokenStd.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            // Add to result vector as String
            tokens.push_back(String(tokenStd));
        }
    }

    return tokens;
}

// Parameter is std::string, return is std::vector<std::shared_ptr<Document>>
std::vector<std::shared_ptr<Document>> SearchEngine::searchByTag(
    const std::string& tag) {
    LOG_F(INFO, "Searching by tag: {}", tag);

    if (tag.empty()) {
        LOG_F(WARNING, "Empty tag provided for search");
        return {};
    }

    std::vector<std::shared_ptr<Document>> results;

    try {
        // Use threading::shared_lock
        threading::shared_lock lock(indexMutex_);

        // tagIndex_ key is std::string
        auto it = tagIndex_.find(tag);
        if (it != tagIndex_.end()) {
            // it->second is std::vector<String>
            results.reserve(it->second.size());
            for (const auto& docId : it->second) {  // docId is String
                // documents_ key is String
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    results.push_back(docIt->second);
                } else {
                    LOG_F(WARNING,
                          "Document ID {} found in tag index but not in "
                          "documents map",
                          std::string(docId));
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during tag search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    LOG_F(INFO, "Found {} documents with tag '{}'", results.size(), tag);
    return results;
}

// Parameter is std::string, return is std::vector<std::shared_ptr<Document>>
std::vector<std::shared_ptr<Document>> SearchEngine::fuzzySearchByTag(
    const std::string& tag, int tolerance) {
    LOG_F(INFO, "Fuzzy searching by tag: {} with tolerance: {}", tag,
          tolerance);

    if (tag.empty()) {
        LOG_F(WARNING, "Empty tag provided for fuzzy search");
        return {};
    }

    if (tolerance < 0) {
        throw std::invalid_argument("Tolerance cannot be negative");
    }

    std::vector<std::shared_ptr<Document>> results;
    // Use HashSet<String> for processedDocIds
    HashSet<String> processedDocIds;

    try {
        // Use threading::shared_lock
        threading::shared_lock lock(indexMutex_);

        // --- Parallel Processing Setup ---
        // Use std::vector<std::string> for tagKeys as tagIndex_ keys are
        // std::string
        std::vector<std::string> tagKeys;
        tagKeys.reserve(tagIndex_.size());
        for (const auto& [key, _] : tagIndex_) {
            tagKeys.push_back(key);
        }

        // Use threading::future and threading::promise
        std::vector<threading::future<std::vector<String>>>
            futures;  // Future returns std::vector<String> (doc IDs)

        // Calculate chunk size for parallel processing
        size_t numItems = tagKeys.size();
        size_t chunkSize = (numItems > 0 && maxThreads_ > 0)
                               ? std::max(size_t(1), numItems / maxThreads_)
                               : numItems;
        if (chunkSize == 0 && numItems > 0)
            chunkSize = 1;  // Ensure chunkSize is at least 1 if there are items

        // --- Launch worker threads ---
        for (size_t i = 0; i < numItems; i += chunkSize) {
            size_t end = std::min(i + chunkSize, numItems);
            // Capture necessary variables by value or reference safely
            // Use std::vector<std::string> for the chunk of keys
            std::vector<std::string> keyChunk(tagKeys.begin() + i,
                                              tagKeys.begin() + end);

#ifdef ATOM_USE_BOOST
            // Boost async equivalent or manual thread creation with promise
            threading::promise<std::vector<String>> promise;
            futures.push_back(promise.get_future());
            // Create boost::thread manually
            threading::thread([this, tag, tolerance, keyChunk,
                               promise = std::move(promise)]() mutable {
                std::vector<String> matchedDocIds;
                // Need read lock inside the thread if accessing shared index
                // data directly Or pass necessary data by value/copy
                threading::shared_lock threadLock(
                    indexMutex_);  // Lock inside thread
                for (const auto& key : keyChunk) {
                    if (levenshteinDistanceSIMD(tag, key) <= tolerance) {
                        auto tagIt = tagIndex_.find(key);
                        if (tagIt != tagIndex_.end()) {
                            const auto& docIds =
                                tagIt->second;  // std::vector<String>
                            matchedDocIds.insert(matchedDocIds.end(),
                                                 docIds.begin(), docIds.end());
                            LOG_F(INFO, "Tag '{}' matched '{}' (fuzzy)", key,
                                  tag);
                        }
                    }
                }
                threadLock.unlock();  // Unlock before setting promise
                promise.set_value(std::move(matchedDocIds));
            });

#else  // Use std::async
            futures.push_back(std::async(
                std::launch::async, [this, tag, tolerance, keyChunk]() {
                    std::vector<String> matchedDocIds;
                    // Need read lock inside the async task
                    threading::shared_lock threadLock(indexMutex_);
                    for (const auto& key : keyChunk) {  // key is std::string
                        if (levenshteinDistanceSIMD(tag, key) <= tolerance) {
                            auto tagIt = tagIndex_.find(key);
                            if (tagIt != tagIndex_.end()) {
                                const auto& docIds =
                                    tagIt->second;  // std::vector<String>
                                matchedDocIds.insert(matchedDocIds.end(),
                                                     docIds.begin(),
                                                     docIds.end());
                                LOG_F(INFO, "Tag '{}' matched '{}' (fuzzy)",
                                      key, tag);
                            }
                        }
                    }
                    // Lock automatically released when threadLock goes out of
                    // scope
                    return matchedDocIds;
                }));
#endif
        }
        // Unlock the main lock after launching threads/tasks
        lock.unlock();

        // --- Collect results ---
        for (auto& future : futures) {
            try {
                std::vector<String> docIds =
                    future.get();  // docIds is std::vector<String>
                // Need read lock again to access documents_ map
                threading::shared_lock collectLock(indexMutex_);
                for (const auto& docId : docIds) {  // docId is String
                    // Use HashSet<String>::insert
                    if (processedDocIds.insert(docId)
                            .second) {  // Insert String
                        // documents_ key is String
                        auto docIt = documents_.find(docId);
                        if (docIt != documents_.end()) {
                            results.push_back(docIt->second);
                        } else {
                            LOG_F(WARNING,
                                  "Doc ID {} from fuzzy search not found in "
                                  "documents map",
                                  std::string(docId));
                        }
                    }
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception collecting fuzzy search results: {}",
                      e.what());
                // Decide whether to continue or rethrow
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during fuzzy tag search setup: {}", e.what());
        throw SearchOperationException(e.what());
    }

    LOG_F(INFO, "Found {} documents with fuzzy tag match for '{}'",
          results.size(), tag);
    return results;
}

// Parameter is std::vector<std::string>, return is
// std::vector<std::shared_ptr<Document>>
std::vector<std::shared_ptr<Document>> SearchEngine::searchByTags(
    const std::vector<std::string>& tags) {
    LOG_F(INFO, "Searching by multiple tags");

    if (tags.empty()) {
        LOG_F(WARNING, "Empty tags list provided for search");
        return {};
    }

    // Use HashMap<String, double> for scores
    HashMap<String, double> scores;

    try {
        // Use threading::shared_lock
        threading::shared_lock lock(indexMutex_);

        for (const auto& tag : tags) {  // tag is std::string
            // tagIndex_ key is std::string
            auto it = tagIndex_.find(tag);
            if (it != tagIndex_.end()) {
                // it->second is std::vector<String>
                for (const auto& docId : it->second) {  // docId is String
                    // documents_ key is String
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        // tfIdf expects std::string_view for term
                        scores[docId] +=
                            tfIdf(*docIt->second, tag);  // Pass std::string tag
                        LOG_F(INFO, "Tag '{}' found in document id: {}", tag,
                              std::string(docId));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during multi-tag search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    // getRankedResults expects HashMap<String, double>
    auto results = getRankedResults(scores);
    LOG_F(INFO, "Found {} documents matching the tags", results.size());
    return results;
}

// Parameters match header: std::vector<String>, HashMap<String, double>,
// threading::mutex
void SearchEngine::searchByContentWorker(
    const std::vector<String>& wordChunk,  // wordChunk contains String
    HashMap<String, double>& scoresMap,    // scoresMap uses String key
    threading::mutex& scoresMutex) {
    // Use HashMap<String, double> for localScores
    HashMap<String, double> localScores;

    // Lock needed to access shared index data
    threading::shared_lock lock(indexMutex_);

    for (const auto& word : wordChunk) {  // word is String
        // contentIndex_ key is String
        auto it = contentIndex_.find(word);
        if (it != contentIndex_.end()) {
            // it->second is HashSet<String>
            for (const auto& docId : it->second) {  // docId is String
                // documents_ key is String
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    // tfIdf expects std::string_view for term, convert String
                    localScores[docId] +=
                        tfIdf(*docIt->second, std::string_view(word));
                    // Use std::string() for logging if needed
                    LOG_F(INFO, "Word '{}' found in document id: {}",
                          std::string(word), std::string(docId));
                }
            }
        }
    }
    lock.unlock();  // Release read lock before acquiring write lock

    // Merge results with main scores map
    // Use threading::unique_lock with the passed mutex reference
    threading::unique_lock writeLock(scoresMutex);
    for (const auto& [docId, score] : localScores) {  // docId is String
        scoresMap[docId] += score;  // Add to HashMap<String, double>
    }
}

// Parameter is String, return is std::vector<std::shared_ptr<Document>>
std::vector<std::shared_ptr<Document>> SearchEngine::searchByContent(
    const String& query) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Searching by content: {}", std::string(query));

    if (query.empty()) {
        LOG_F(WARNING, "Empty query provided for content search");
        return {};
    }

    // tokenizeContent takes String, returns std::vector<String>
    auto words = tokenizeContent(query);
    if (words.empty()) {
        LOG_F(WARNING, "No valid tokens in query");
        return {};
    }

    // Use HashMap<String, double> for scores
    HashMap<String, double> scores;
    // Use threading::mutex
    threading::mutex scoresMutex;

    try {
        // If we have few words or threads, no need for parallel processing
        if (words.size() <= 2 || maxThreads_ <= 1) {
            // Call worker directly, passing std::vector<String>
            searchByContentWorker(words, scores, scoresMutex);
        } else {
            // Parallel processing
            // Use threading::future
            std::vector<threading::future<void>> futures;

            // Calculate chunk size
            size_t numWords = words.size();
            size_t chunkSize = std::max(size_t(1), numWords / maxThreads_);

            // Launch worker tasks/threads
            for (size_t i = 0; i < numWords; i += chunkSize) {
                size_t end = std::min(i + chunkSize, numWords);
                // Create chunk as std::vector<String>
                std::vector<String> wordChunk(words.begin() + i,
                                              words.begin() + end);

#ifdef ATOM_USE_BOOST
                // Boost async or manual thread creation
                threading::promise<void> promise;
                futures.push_back(promise.get_future());
                threading::thread([this, wordChunk, &scores, &scoresMutex,
                                   promise = std::move(promise)]() mutable {
                    try {
                        searchByContentWorker(wordChunk, scores, scoresMutex);
                        promise.set_value();
                    } catch (...) {
                        try {
                            promise.set_exception(std::current_exception());
                        } catch (...) {
                        }  // Avoid exceptions from set_exception
                    }
                });
#else
                // Use std::async
                futures.push_back(std::async(
                    std::launch::async, &SearchEngine::searchByContentWorker,
                    this, wordChunk, std::ref(scores), std::ref(scoresMutex)));
#endif
            }

            // Wait for all tasks/threads to complete
            for (auto& future : futures) {
                try {
                    future.get();  // Will rethrow exceptions
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in content search worker: {}",
                          e.what());
                    // Decide how to handle worker exceptions (e.g., continue,
                    // throw) For now, just log and continue collecting results
                    // from others.
                }
            }
        }
    } catch (const std::exception& e) {
        // Catch exceptions during setup or waiting
        LOG_F(ERROR, "Error during content search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    // getRankedResults expects HashMap<String, double>
    auto results = getRankedResults(scores);
    LOG_F(INFO, "Found {} documents matching content query", results.size());
    return results;
}

// Parameter is String, return is std::vector<std::shared_ptr<Document>>
std::vector<std::shared_ptr<Document>> SearchEngine::booleanSearch(
    const String& query) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Performing boolean search: {}", std::string(query));

    if (query.empty()) {
        LOG_F(WARNING, "Empty query provided for boolean search");
        return {};
    }

    // Use HashMap<String, double> for scores
    HashMap<String, double> scores;
    // Use std::stringstream with std::string conversion - Fix Most Vexing Parse
    std::istringstream iss{std::string(query)};
    std::string wordStd;  // Use std::string for stream extraction
    bool isNot = false;

    try {
        // Use threading::shared_lock
        threading::shared_lock lock(indexMutex_);

        while (iss >> wordStd) {
            if (wordStd == "NOT") {
                isNot = true;
                continue;
            }

            if (wordStd == "AND" || wordStd == "OR") {
                // Simple implementation: treat AND/OR as term separators for
                // now A proper boolean parser would be needed for complex logic
                continue;
            }

            // Convert to lowercase and clean using std::string
            std::transform(wordStd.begin(), wordStd.end(), wordStd.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            wordStd =
                std::regex_replace(wordStd, std::regex("[^a-zA-Z0-9]"), "");

            if (wordStd.empty()) {
                continue;
            }

            // Convert cleaned std::string to String for index lookup
            String wordKey(wordStd);

            // contentIndex_ key is String
            auto it = contentIndex_.find(wordKey);
            if (it != contentIndex_.end()) {
                // it->second is HashSet<String>
                for (const auto& docId : it->second) {  // docId is String
                    // documents_ key is String
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        // tfIdf expects std::string_view, convert String
                        // wordKey
                        double tfidfScore =
                            tfIdf(*docIt->second, std::string_view(wordKey));

                        if (isNot) {
                            scores[docId] -=
                                tfidfScore * 2.0;  // Double negative weight
                            LOG_F(INFO,
                                  "Word '{}' excluded from document id: {}",
                                  wordStd, std::string(docId));
                        } else {
                            scores[docId] += tfidfScore;
                            LOG_F(INFO, "Word '{}' included in document id: {}",
                                  wordStd, std::string(docId));
                        }
                    }
                }
            }
            isNot = false;  // Reset for next word
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during boolean search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    // getRankedResults expects HashMap<String, double>
    auto results = getRankedResults(scores);
    LOG_F(INFO, "Found {} documents matching boolean query", results.size());
    return results;
}

// Parameter is String, return is std::vector<String>
std::vector<String> SearchEngine::autoComplete(const String& prefix,
                                               size_t maxResults) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Auto-completing for prefix: {}", std::string(prefix));

    if (prefix.empty()) {
        LOG_F(WARNING, "Empty prefix provided for autocomplete");
        return {};
    }

    // Use std::vector<String> for suggestions
    std::vector<String> suggestions;
    // Use std::string for prefix comparison (lowercase)
    std::string prefixStd = std::string(prefix);
    std::transform(prefixStd.begin(), prefixStd.end(), prefixStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    size_t prefixLen = prefixStd.length();

    try {
        // Use threading::shared_lock
        threading::shared_lock lock(indexMutex_);

        // Use prefix match on tag index (key is std::string)
        for (const auto& [tag, _] : tagIndex_) {  // tag is std::string
            if (tag.size() >= prefixLen) {
                std::string tagLower = tag;
                std::transform(tagLower.begin(), tagLower.end(),
                               tagLower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (tagLower.rfind(prefixStd, 0) == 0) {  // Check prefix
                    // Add tag as String to suggestions
                    suggestions.push_back(String(tag));
                    LOG_F(INFO, "Tag suggestion: {}", tag);
                }
            }
            if (maxResults > 0 && suggestions.size() >= maxResults)
                break;
        }

        // Add from content index (key is String) if limit not reached
        if (maxResults == 0 || suggestions.size() < maxResults) {
            for (const auto& [word, _] : contentIndex_) {  // word is String
                if (word.size() >= prefixLen) {
                    std::string wordStd =
                        std::string(word);  // Convert String to std::string
                    std::transform(
                        wordStd.begin(), wordStd.end(), wordStd.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (wordStd.rfind(prefixStd, 0) == 0) {  // Check prefix
                        // Avoid duplicates if already added from tags
                        // This requires converting String 'word' to std::string
                        // for find Or convert suggestions to std::string
                        // temporarily. Simpler: check before adding.
                        bool found = false;
                        for (const auto& sug : suggestions) {
                            if (sug == word) {  // Compare String == String
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            suggestions.push_back(word);  // Add String
                            LOG_F(INFO, "Content suggestion: {}",
                                  std::string(word));
                        }
                    }
                }
                // Limit results if requested
                if (maxResults > 0 && suggestions.size() >= maxResults) {
                    break;
                }
            }
        }

        // Sort by relevance (using docFrequency_)
        // docFrequency_ key is assumed std::string
        std::sort(suggestions.begin(), suggestions.end(),
                  [this](const String& a, const String& b) {
                      // Convert String a and b to std::string for lookup
                      std::string keyA = std::string(a);
                      std::string keyB = std::string(b);
                      int freqA = docFrequency_.count(keyA)
                                      ? docFrequency_.at(keyA)
                                      : 0;
                      int freqB = docFrequency_.count(keyB)
                                      ? docFrequency_.at(keyB)
                                      : 0;
                      // Higher frequency first
                      return freqA > freqB;
                  });

        // Limit results after sorting
        if (maxResults > 0 && suggestions.size() > maxResults) {
            suggestions.resize(maxResults);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during autocomplete: {}", e.what());
        throw SearchOperationException(e.what());
    }

    LOG_F(INFO, "Found {} suggestions for prefix '{}'", suggestions.size(),
          std::string(prefix));
    return suggestions;  // Return std::vector<String>
}

// Parameter is String
void SearchEngine::saveIndex(const String& filename) const {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Saving index to file: {}", std::string(filename));

    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }

    try {
        // Use threading::shared_lock
        threading::shared_lock lock(indexMutex_);

        // Use std::ofstream with std::string conversion for filename
        std::ofstream ofs(std::string(filename), std::ios::binary);
        if (!ofs) {
            std::string errMsg =
                "Failed to open file for writing: " + std::string(filename);
            LOG_F(ERROR, "{}", errMsg);
            throw std::ios_base::failure(errMsg);
        }

        // Write total document count
        int totalDocsValue = totalDocs_.load();
        ofs.write(reinterpret_cast<const char*>(&totalDocsValue),
                  sizeof(totalDocsValue));

        // Write documents (documents_ is HashMap<String, ...>)
        size_t docSize = documents_.size();
        ofs.write(reinterpret_cast<const char*>(&docSize), sizeof(docSize));

        for (const auto& [docId, doc] : documents_) {  // docId is String
            // Write document id (String)
            std::string docIdStd =
                std::string(docId);  // Convert to std::string for size/c_str
            size_t idLength = docIdStd.size();
            ofs.write(reinterpret_cast<const char*>(&idLength),
                      sizeof(idLength));
            ofs.write(docIdStd.c_str(), idLength);

            // Write document content (String)
            std::string contentStd = std::string(
                doc->getContent());  // Convert String to std::string
            size_t contentLength = contentStd.size();
            ofs.write(reinterpret_cast<const char*>(&contentLength),
                      sizeof(contentLength));
            ofs.write(contentStd.c_str(), contentLength);

            // Write tags (std::set<std::string>)
            const auto& tags = doc->getTags();
            size_t tagsCount = tags.size();
            ofs.write(reinterpret_cast<const char*>(&tagsCount),
                      sizeof(tagsCount));

            for (const auto& tag : tags) {  // tag is std::string
                size_t tagLength = tag.size();
                ofs.write(reinterpret_cast<const char*>(&tagLength),
                          sizeof(tagLength));
                ofs.write(tag.c_str(), tagLength);
            }

            // Write click count
            int clickCount = doc->getClickCount();
            ofs.write(reinterpret_cast<const char*>(&clickCount),
                      sizeof(clickCount));
        }

        LOG_F(INFO, "Index saved successfully to {}", std::string(filename));
    } catch (const std::ios_base::failure& e) {
        LOG_F(ERROR, "I/O error while saving index: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error while saving index: {}", e.what());
        throw;
    }
}

// Parameter is String
void SearchEngine::loadIndex(const String& filename) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Loading index from file: {}", std::string(filename));

    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }

    try {
        // Use unique_lock with the appropriate mutex type
        std::unique_lock<threading::shared_mutex> lock(indexMutex_);

        // Use std::ifstream with std::string conversion for filename
        std::ifstream ifs(std::string(filename), std::ios::binary);
        if (!ifs) {
            std::string errMsg =
                "Failed to open file for reading: " + std::string(filename);
            LOG_F(ERROR, "{}", errMsg);
            throw std::ios_base::failure(errMsg);
        }

        // Clear existing data (using HashMap, HashSet etc.)
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        totalDocs_ = 0;  // Reset atomic count

        // Read total document count
        int totalDocsValue;
        if (!ifs.read(reinterpret_cast<char*>(&totalDocsValue),
                      sizeof(totalDocsValue))) {
            if (ifs.eof()) {
                LOG_F(INFO, "Index file {} is empty or truncated at totalDocs.",
                      std::string(filename));
                return;
            }  // Handle empty/truncated file
            else {
                throw std::ios_base::failure(
                    "Failed to read totalDocs from index file: " +
                    std::string(filename));
            }
        }
        totalDocs_ = totalDocsValue;

        // Read documents count
        size_t docSize;
        if (!ifs.read(reinterpret_cast<char*>(&docSize), sizeof(docSize))) {
            if (ifs.eof() && totalDocsValue == 0) {
                LOG_F(INFO, "Index file {} contains 0 documents.",
                      std::string(filename));
                return;
            }  // Handle 0 docs case
            else {
                throw std::ios_base::failure(
                    "Failed to read docSize from index file: " +
                    std::string(filename));
            }
        }

        for (size_t i = 0; i < docSize; ++i) {
            // Read document id (read as std::string, convert to String)
            size_t idLength;
            if (!ifs.read(reinterpret_cast<char*>(&idLength), sizeof(idLength)))
                throw std::ios_base::failure("Failed to read idLength");
            std::string docIdStd(idLength, '\0');  // Use '\0' for safety
            if (!ifs.read(&docIdStd[0], idLength))
                throw std::ios_base::failure("Failed to read docId");
            String docId(docIdStd);  // Convert to String

            // Read document content (read as std::string, convert to String)
            size_t contentLength;
            if (!ifs.read(reinterpret_cast<char*>(&contentLength),
                          sizeof(contentLength)))
                throw std::ios_base::failure("Failed to read contentLength");
            std::string contentStd(contentLength, '\0');
            if (!ifs.read(&contentStd[0], contentLength))
                throw std::ios_base::failure("Failed to read content");
            String content(contentStd);  // Convert to String

            // Read tags (read as std::string, store in std::set<std::string>)
            std::set<std::string> tags;
            size_t tagsCount;
            if (!ifs.read(reinterpret_cast<char*>(&tagsCount),
                          sizeof(tagsCount)))
                throw std::ios_base::failure("Failed to read tagsCount");

            for (size_t j = 0; j < tagsCount; ++j) {
                size_t tagLength;
                if (!ifs.read(reinterpret_cast<char*>(&tagLength),
                              sizeof(tagLength)))
                    throw std::ios_base::failure("Failed to read tagLength");
                std::string tagStd(tagLength, '\0');
                if (!ifs.read(&tagStd[0], tagLength))
                    throw std::ios_base::failure("Failed to read tag");
                tags.insert(
                    tagStd);  // Insert std::string into std::set<std::string>
            }

            // Read click count
            int clickCount;
            if (!ifs.read(reinterpret_cast<char*>(&clickCount),
                          sizeof(clickCount)))
                throw std::ios_base::failure("Failed to read clickCount");

            // Create document using String id and content
            // Pass tags initializer list (it's std::set<std::string>)
            // Need to convert std::set to initializer_list or adjust
            // constructor Easiest: create doc then add tags manually
            auto doc = std::make_shared<Document>(
                docId, content,
                std::initializer_list<std::string>{});  // Empty tags initially
            for (const auto& tag : tags) {
                doc->addTag(tag);  // Add tags using the method
            }
            // Manually set click count if Document doesn't have a setter
            // doc->setClickCount(clickCount); // Assuming such a method exists
            // or is added Or modify constructor/friend access if needed. For
            // now, we can't set it back easily. Hacky way: increment N times
            // (BAD IDEA) for(int k=0; k<clickCount; ++k)
            // doc->incrementClickCount();

            // Add to documents_ map (key String)
            documents_[docId] = doc;

            // Rebuild tag index (key std::string, value std::vector<String>)
            for (const auto& tag : tags) {        // tag is std::string
                tagIndex_[tag].push_back(docId);  // Add String docId
                // Rebuild doc frequency (key std::string)
                docFrequency_[tag]++;
            }

            // Rebuild content index (calls addContentToIndex)
            addContentToIndex(doc);
        }
        // Verify totalDocs matches loaded count
        if (documents_.size() != static_cast<size_t>(totalDocs_.load())) {
            LOG_F(WARNING,
                  "Loaded document count ({}) does not match stored totalDocs "
                  "({}) in file {}",
                  documents_.size(), totalDocs_.load(), std::string(filename));
            // Optionally correct totalDocs_ here:
            // totalDocs_ = documents_.size();
        }

        LOG_F(INFO, "Index loaded successfully from {}, total docs: {}",
              std::string(filename), totalDocs_.load());
    } catch (const std::ios_base::failure& e) {
        LOG_F(ERROR, "I/O error while loading index: {}", e.what());
        // Clear potentially partially loaded data on error
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        totalDocs_ = 0;
        throw;  // Re-throw
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error while loading index: {}", e.what());
        // Clear potentially partially loaded data on error
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        totalDocs_ = 0;
        throw;  // Re-throw
    }
}

// Parameters are std::string_view
int SearchEngine::levenshteinDistanceSIMD(std::string_view s1,
                                          std::string_view s2) const noexcept {
    // Implementation uses std::string_view directly, no changes needed for
    // String alias
    const size_t m = s1.length();
    const size_t n = s2.length();

    if (m == 0)
        return static_cast<int>(n);
    if (n == 0)
        return static_cast<int>(m);

    // Use std::vector<int> for DP table rows
    std::vector<int> prevRow(n + 1);
    std::vector<int> currRow(n + 1);

    for (size_t j = 0; j <= n; ++j) {
        prevRow[j] = static_cast<int>(j);
    }

    for (size_t i = 0; i < m; ++i) {
        currRow[0] = static_cast<int>(i + 1);
        for (size_t j = 0; j < n; ++j) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            currRow[j + 1] = std::min(
                {prevRow[j + 1] + 1, currRow[j] + 1, prevRow[j] + cost});
        }
        prevRow.swap(currRow);  // Efficient swap
    }

    return prevRow[n];  // Result is in prevRow after last swap
}

// Parameter term is std::string_view
double SearchEngine::tfIdf(const Document& doc,
                           std::string_view term) const noexcept {
    // Get content as String, convert to std::string for find operations
    std::string contentStd = std::string(doc.getContent());
    // Convert term std::string_view to std::string
    std::string termStd = std::string(term);

    // Convert both to lowercase
    std::transform(contentStd.begin(), contentStd.end(), contentStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(termStd.begin(), termStd.end(), termStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Count occurrences of term in document content
    size_t count = 0;
    size_t pos = 0;
    size_t contentLen = contentStd.length();  // Cache length
    size_t termLen = termStd.length();        // Cache length
    if (termLen == 0)
        return 0.0;  // Avoid issues with empty term

    while ((pos = contentStd.find(termStd, pos)) != std::string::npos) {
        count++;
        pos += termLen;  // Move past the found term
    }

    if (count == 0)
        return 0.0;

    // Term frequency (TF) - use content length
    double tf =
        (contentLen > 0)
            ? (static_cast<double>(count) / static_cast<double>(contentLen))
            : 0.0;

    // Inverse document frequency (IDF)
    // docFrequency_ key is assumed std::string
    double df = 1.0;  // Start with 1 to avoid log(0) or division by zero
    auto freqIt = docFrequency_.find(termStd);
    if (freqIt != docFrequency_.end()) {
        df = static_cast<double>(freqIt->second);
    }

    int docsTotal = totalDocs_.load();
    // Ensure totalDocs is at least df and positive before log
    double idf =
        (docsTotal > 0 && df > 0 && static_cast<double>(docsTotal) >= df)
            ? std::log(static_cast<double>(docsTotal) / df)
            : 0.0;  // Default to 0 if totalDocs is 0, df is 0, or totalDocs <
                    // df

    // Add click count boost
    // Use a small epsilon to avoid issues if click count is huge
    double clickBoost =
        1.0 + std::log1p(static_cast<double>(doc.getClickCount()) *
                         0.1);  // Smoother boost

    double tfIdfValue = tf * idf * clickBoost;
    // Log calculation details if needed
    // LOG_F(DEBUG, "TF-IDF for term '{}' in doc '{}': TF={}, DF={}, IDF={},
    // ClickBoost={}, Score={}",
    //       termStd, std::string(doc.getId()), tf, df, idf, clickBoost,
    //       tfIdfValue);
    return tfIdfValue;
}

// Parameter is String, return is std::shared_ptr<Document>
std::shared_ptr<Document> SearchEngine::findDocumentById(const String& docId) {
    // Use std::string() for logging if needed
    LOG_F(INFO, "Finding document by id: {}", std::string(docId));

    if (docId.empty()) {
        throw std::invalid_argument("Document ID cannot be empty");
    }

    // Use threading::shared_lock
    threading::shared_lock lock(indexMutex_);

    // documents_ key is String
    auto it = documents_.find(docId);
    if (it == documents_.end()) {
        LOG_F(ERROR, "Document not found: {}", std::string(docId));
        throw DocumentNotFoundException(docId);
    }

    LOG_F(INFO, "Document found: {}", std::string(docId));
    return it->second;
}

// Parameter is HashMap<String, double>, return is
// std::vector<std::shared_ptr<Document>>
std::vector<std::shared_ptr<Document>> SearchEngine::getRankedResults(
    const HashMap<String, double>& scores) {  // scores key is String
    struct ScoredDoc {
        std::shared_ptr<Document> doc;
        double score;

        // Max-heap comparison (higher score is higher priority)
        bool operator<(const ScoredDoc& other) const {
            // Handle potential NaN scores? Assume scores are valid numbers for
            // now.
            return score < other.score;
        }
    };

    // Use std::priority_queue for ranking
    std::priority_queue<ScoredDoc> priorityQueue;

    // Lock needed to access documents_ map via findDocumentById
    threading::shared_lock lock(indexMutex_);

    // Add all documents with scores to the priority queue
    for (const auto& [docId, score] : scores) {  // docId is String
        // Skip documents with non-positive scores (including 0)
        if (score <= 0)
            continue;

        // documents_ key is String
        auto it = documents_.find(docId);
        if (it != documents_.end()) {
            // No need to call findDocumentById again, we already have the
            // iterator
            auto doc = it->second;
            // Click count boost is already included in tfIdf, no need to add
            // again here double finalScore = score; // Use score directly
            priorityQueue.push({doc, score});
            LOG_F(INFO, "Document id: {}, score: {:.6f}",
                  std::string(doc->getId()), score);
        } else {
            LOG_F(WARNING,
                  "Document ID {} found in scores but not in documents map "
                  "during ranking.",
                  std::string(docId));
        }
    }
    lock.unlock();  // Release lock after iterating scores

    // Extract sorted results
    std::vector<std::shared_ptr<Document>> results;
    results.reserve(priorityQueue.size());

    while (!priorityQueue.empty()) {
        results.push_back(priorityQueue.top().doc);
        priorityQueue.pop();
    }

    // Results are extracted from max-heap, already highest score first. No
    // reverse needed. std::reverse(results.begin(), results.end()); // Remove
    // this reverse

    LOG_F(INFO, "Ranked results obtained: {} documents", results.size());
    return results;
}

}  // namespace atom::search