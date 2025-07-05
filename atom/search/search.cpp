#include "search.hpp"

#include <immintrin.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <queue>
#include <regex>
#include <sstream>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#else
#include <chrono>
#include <future>
#include <thread>
#endif

namespace atom::search {

Document::Document(String id, String content,
                   std::initializer_list<std::string> tags)
    : id_(std::move(id)), content_(std::move(content)), tags_(tags) {
    validate();
    spdlog::info("Document created with id: {}", std::string(id_));
}

Document::Document(const Document& other)
    : id_(other.id_),
      content_(other.content_),
      tags_(other.tags_),
      clickCount_(other.clickCount_.load(std::memory_order_relaxed)) {}

Document& Document::operator=(const Document& other) {
    if (this != &other) {
        id_ = other.id_;
        content_ = other.content_;
        tags_ = other.tags_;
        clickCount_.store(other.clickCount_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
    }
    return *this;
}

Document::Document(Document&& other) noexcept
    : id_(std::move(other.id_)),
      content_(std::move(other.content_)),
      tags_(std::move(other.tags_)),
      clickCount_(other.clickCount_.load(std::memory_order_relaxed)) {}

Document& Document::operator=(Document&& other) noexcept {
    if (this != &other) {
        id_ = std::move(other.id_);
        content_ = std::move(other.content_);
        tags_ = std::move(other.tags_);
        clickCount_.store(other.clickCount_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
    }
    return *this;
}

void Document::validate() const {
    if (id_.empty()) {
        throw DocumentValidationException("Document ID cannot be empty");
    }

    if (id_.size() > 256) {
        throw DocumentValidationException(
            "Document ID too long (max 256 chars)");
    }

    if (content_.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }

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

void Document::setContent(String content) {
    if (content.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }
    content_ = std::move(content);
}

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

void Document::removeTag(const std::string& tag) { tags_.erase(tag); }

SearchEngine::SearchEngine(unsigned maxThreads)
    : maxThreads_(maxThreads ? maxThreads
#ifdef ATOM_USE_BOOST
                             : boost::thread::hardware_concurrency())
#else
                             : std::thread::hardware_concurrency())
#endif
{
    spdlog::info("SearchEngine initialized with max threads: {}", maxThreads_);

    taskQueue_ = std::make_unique<threading::lockfree_queue<SearchTask>>(1024);
    startWorkerThreads();
    spdlog::info("Task queue initialized with {} worker threads", maxThreads_);
}

SearchEngine::~SearchEngine() {
    spdlog::info("SearchEngine being destroyed");
    stopWorkerThreads();
    spdlog::info("Worker threads stopped and cleaned up");
}

void SearchEngine::startWorkerThreads() {
    shouldStopWorkers_.store(false);
    workerThreads_.reserve(maxThreads_);

    for (unsigned i = 0; i < maxThreads_; ++i) {
        workerThreads_.push_back(std::make_unique<threading::thread>(
            [this]() { workerFunction(); }));
    }
    spdlog::info("Started {} worker threads", maxThreads_);
}

void SearchEngine::stopWorkerThreads() {
    spdlog::info("Stopping worker threads");
    shouldStopWorkers_.store(true);

    for (auto& thread : workerThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    workerThreads_.clear();
    spdlog::info("All worker threads stopped");
}

void SearchEngine::workerFunction() {
    SearchTask task;

    while (!shouldStopWorkers_.load()) {
        if (taskQueue_->pop(task)) {
            try {
                task.callback(task.words);
            } catch (const std::exception& e) {
                spdlog::error("Error in worker thread: {}", e.what());
            }
        } else {
#ifdef ATOM_USE_BOOST
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
        }
    }
}

void SearchEngine::addDocument(const Document& doc) {
    try {
        spdlog::info("Adding document copy with id: {}",
                     std::string(doc.getId()));
        Document tempDoc = doc;
        addDocument(std::move(tempDoc));
    } catch (const DocumentValidationException& e) {
        spdlog::error("Failed to add document copy: {}", e.what());
        throw;
    } catch (const std::invalid_argument& e) {
        spdlog::error("Failed to add document copy: {}", e.what());
        throw;
    }
}

void SearchEngine::addDocument(Document&& doc) {
    spdlog::info("Adding document move with id: {}", std::string(doc.getId()));

    try {
        doc.validate();
    } catch (const DocumentValidationException& e) {
        spdlog::error("Document validation failed: {}", e.what());
        throw;
    }

    std::unique_lock<threading::shared_mutex> lock(indexMutex_);
    String docId = String(doc.getId());

    if (documents_.count(docId) > 0) {
        spdlog::error("Document with ID {} already exists", std::string(docId));
        throw std::invalid_argument("Document with this ID already exists");
    }

    auto docPtr = std::make_shared<Document>(std::move(doc));
    documents_[docId] = docPtr;

    for (const auto& tag : docPtr->getTags()) {
        tagIndex_[tag].push_back(docId);
        docFrequency_[tag]++;
        spdlog::debug("Tag '{}' added to index for doc {}", tag,
                      std::string(docId));
    }

    addContentToIndex(docPtr);
    totalDocs_++;
    spdlog::info("Document added successfully, total docs: {}",
                 totalDocs_.load());
}

void SearchEngine::removeDocument(const String& docId) {
    spdlog::info("Removing document with id: {}", std::string(docId));

    if (docId.empty()) {
        throw std::invalid_argument("Document ID cannot be empty");
    }

    std::unique_lock<threading::shared_mutex> lock(indexMutex_);

    auto docIt = documents_.find(docId);
    if (docIt == documents_.end()) {
        spdlog::error("Document with ID {} not found", std::string(docId));
        throw DocumentNotFoundException(docId);
    }

    auto& doc = docIt->second;

    for (const auto& tag : doc->getTags()) {
        auto tagIt = tagIndex_.find(tag);
        if (tagIt != tagIndex_.end()) {
            auto& docsVec = tagIt->second;
            docsVec.erase(std::remove(docsVec.begin(), docsVec.end(), docId),
                          docsVec.end());

            if (docsVec.empty()) {
                tagIndex_.erase(tagIt);
            }
        }

        auto freqIt = docFrequency_.find(tag);
        if (freqIt != docFrequency_.end()) {
            if (--(freqIt->second) <= 0) {
                docFrequency_.erase(freqIt);
            }
        }
    }

    auto tokens = tokenizeContent(String(doc->getContent()));
    for (const auto& token : tokens) {
        auto contentIt = contentIndex_.find(token);
        if (contentIt != contentIndex_.end()) {
            auto& docsSet = contentIt->second;
            docsSet.erase(docId);

            if (docsSet.empty()) {
                contentIndex_.erase(contentIt);
            }
        }

        auto freqIt = docFrequency_.find(std::string(token));
        if (freqIt != docFrequency_.end()) {
            if (--(freqIt->second) <= 0) {
                docFrequency_.erase(freqIt);
            }
        }
    }

    documents_.erase(docIt);
    totalDocs_--;

    spdlog::info("Document with id: {} removed, total docs: {}",
                 std::string(docId), totalDocs_.load());
}

void SearchEngine::updateDocument(const Document& doc) {
    spdlog::info("Updating document with id: {}", std::string(doc.getId()));

    try {
        doc.validate();
        std::unique_lock<threading::shared_mutex> lock(indexMutex_);
        String docId = String(doc.getId());

        if (documents_.find(docId) == documents_.end()) {
            spdlog::error("Document with ID {} not found", std::string(docId));
            throw DocumentNotFoundException(docId);
        }

        lock.unlock();
        removeDocument(docId);
        addDocument(doc);

        spdlog::info("Document with id: {} updated", std::string(docId));
    } catch (const DocumentNotFoundException& e) {
        spdlog::error("Error updating document (not found): {}", e.what());
        throw;
    } catch (const DocumentValidationException& e) {
        spdlog::error("Error updating document (validation): {}", e.what());
        throw;
    } catch (const std::invalid_argument& e) {
        spdlog::error("Error updating document (invalid arg): {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error updating document: {}", e.what());
        throw;
    }
}

void SearchEngine::clear() {
    spdlog::info("Clearing all documents and indexes");

    std::unique_lock<threading::shared_mutex> lock(indexMutex_);
    documents_.clear();
    tagIndex_.clear();
    contentIndex_.clear();
    docFrequency_.clear();
    totalDocs_ = 0;

    spdlog::info("All documents and indexes cleared");
}

bool SearchEngine::hasDocument(const String& docId) const {
    threading::shared_lock lock(indexMutex_);
    return documents_.find(docId) != documents_.end();
}

std::vector<String> SearchEngine::getAllDocumentIds() const {
    threading::shared_lock lock(indexMutex_);
    std::vector<String> ids;
    ids.reserve(documents_.size());

    for (const auto& [docId, _] : documents_) {
        ids.push_back(docId);
    }

    return ids;
}

void SearchEngine::addContentToIndex(const std::shared_ptr<Document>& doc) {
    spdlog::debug("Indexing content for document id: {}",
                  std::string(doc->getId()));

    String docId = String(doc->getId());
    String content = String(doc->getContent());
    auto tokens = tokenizeContent(content);

    for (const auto& token : tokens) {
        contentIndex_[token].insert(docId);
        docFrequency_[std::string(token)]++;
        spdlog::trace("Token '{}' indexed for document id: {}",
                      std::string(token), std::string(docId));
    }
}

std::vector<String> SearchEngine::tokenizeContent(const String& content) const {
    std::vector<String> tokens;
    std::stringstream ss{std::string(content)};
    std::string tokenStd;

    while (ss >> tokenStd) {
        tokenStd = std::regex_replace(tokenStd, std::regex("[^a-zA-Z0-9]"), "");

        if (!tokenStd.empty()) {
            std::transform(tokenStd.begin(), tokenStd.end(), tokenStd.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            tokens.push_back(String(tokenStd));
        }
    }

    return tokens;
}

std::vector<std::shared_ptr<Document>> SearchEngine::searchByTag(
    const std::string& tag) {
    spdlog::debug("Searching by tag: {}", tag);

    if (tag.empty()) {
        spdlog::warn("Empty tag provided for search");
        return {};
    }

    std::vector<std::shared_ptr<Document>> results;

    try {
        threading::shared_lock lock(indexMutex_);

        auto it = tagIndex_.find(tag);
        if (it != tagIndex_.end()) {
            results.reserve(it->second.size());
            for (const auto& docId : it->second) {
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    results.push_back(docIt->second);
                } else {
                    spdlog::warn(
                        "Document ID {} found in tag index but not in "
                        "documents map",
                        std::string(docId));
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during tag search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    spdlog::debug("Found {} documents with tag '{}'", results.size(), tag);
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::fuzzySearchByTag(
    const std::string& tag, int tolerance) {
    spdlog::debug("Fuzzy searching by tag: {} with tolerance: {}", tag,
                  tolerance);

    if (tag.empty()) {
        spdlog::warn("Empty tag provided for fuzzy search");
        return {};
    }

    if (tolerance < 0) {
        throw std::invalid_argument("Tolerance cannot be negative");
    }

    std::vector<std::shared_ptr<Document>> results;
    HashSet<String> processedDocIds;

    try {
        threading::shared_lock lock(indexMutex_);

        std::vector<std::string> tagKeys;
        tagKeys.reserve(tagIndex_.size());
        for (const auto& [key, _] : tagIndex_) {
            tagKeys.push_back(key);
        }

        std::vector<threading::future<std::vector<String>>> futures;

        size_t numItems = tagKeys.size();
        size_t chunkSize = (numItems > 0 && maxThreads_ > 0)
                               ? std::max(size_t(1), numItems / maxThreads_)
                               : numItems;
        if (chunkSize == 0 && numItems > 0)
            chunkSize = 1;

        for (size_t i = 0; i < numItems; i += chunkSize) {
            size_t end = std::min(i + chunkSize, numItems);
            std::vector<std::string> keyChunk(tagKeys.begin() + i,
                                              tagKeys.begin() + end);

#ifdef ATOM_USE_BOOST
            threading::promise<std::vector<String>> promise;
            futures.push_back(promise.get_future());
            threading::thread([this, tag, tolerance, keyChunk,
                               promise = std::move(promise)]() mutable {
                std::vector<String> matchedDocIds;
                threading::shared_lock threadLock(indexMutex_);
                for (const auto& key : keyChunk) {
                    if (levenshteinDistanceSIMD(tag, key) <= tolerance) {
                        auto tagIt = tagIndex_.find(key);
                        if (tagIt != tagIndex_.end()) {
                            const auto& docIds = tagIt->second;
                            matchedDocIds.insert(matchedDocIds.end(),
                                                 docIds.begin(), docIds.end());
                            spdlog::trace("Tag '{}' matched '{}' (fuzzy)", key,
                                          tag);
                        }
                    }
                }
                threadLock.unlock();
                promise.set_value(std::move(matchedDocIds));
            });
#else
            futures.push_back(std::async(
                std::launch::async, [this, tag, tolerance, keyChunk]() {
                    std::vector<String> matchedDocIds;
                    threading::shared_lock threadLock(indexMutex_);
                    for (const auto& key : keyChunk) {
                        if (levenshteinDistanceSIMD(tag, key) <= tolerance) {
                            auto tagIt = tagIndex_.find(key);
                            if (tagIt != tagIndex_.end()) {
                                const auto& docIds = tagIt->second;
                                matchedDocIds.insert(matchedDocIds.end(),
                                                     docIds.begin(),
                                                     docIds.end());
                                spdlog::trace("Tag '{}' matched '{}' (fuzzy)",
                                              key, tag);
                            }
                        }
                    }
                    return matchedDocIds;
                }));
#endif
        }
        lock.unlock();

        for (auto& future : futures) {
            try {
                std::vector<String> docIds = future.get();
                threading::shared_lock collectLock(indexMutex_);
                for (const auto& docId : docIds) {
                    if (processedDocIds.insert(docId).second) {
                        auto docIt = documents_.find(docId);
                        if (docIt != documents_.end()) {
                            results.push_back(docIt->second);
                        } else {
                            spdlog::warn(
                                "Doc ID {} from fuzzy search not found in "
                                "documents map",
                                std::string(docId));
                        }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception collecting fuzzy search results: {}",
                              e.what());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during fuzzy tag search setup: {}", e.what());
        throw SearchOperationException(e.what());
    }

    spdlog::debug("Found {} documents with fuzzy tag match for '{}'",
                  results.size(), tag);
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::searchByTags(
    const std::vector<std::string>& tags) {
    spdlog::debug("Searching by multiple tags");

    if (tags.empty()) {
        spdlog::warn("Empty tags list provided for search");
        return {};
    }

    HashMap<String, double> scores;

    try {
        threading::shared_lock lock(indexMutex_);

        for (const auto& tag : tags) {
            auto it = tagIndex_.find(tag);
            if (it != tagIndex_.end()) {
                for (const auto& docId : it->second) {
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        scores[docId] += tfIdf(*docIt->second, tag);
                        spdlog::trace("Tag '{}' found in document id: {}", tag,
                                      std::string(docId));
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during multi-tag search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    auto results = getRankedResults(scores);
    spdlog::debug("Found {} documents matching the tags", results.size());
    return results;
}

void SearchEngine::searchByContentWorker(const std::vector<String>& wordChunk,
                                         HashMap<String, double>& scoresMap,
                                         threading::mutex& scoresMutex) {
    HashMap<String, double> localScores;
    threading::shared_lock lock(indexMutex_);

    for (const auto& word : wordChunk) {
        auto it = contentIndex_.find(word);
        if (it != contentIndex_.end()) {
            for (const auto& docId : it->second) {
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    localScores[docId] +=
                        tfIdf(*docIt->second, std::string_view(word));
                    spdlog::trace("Word '{}' found in document id: {}",
                                  std::string(word), std::string(docId));
                }
            }
        }
    }
    lock.unlock();

    threading::unique_lock writeLock(scoresMutex);
    for (const auto& [docId, score] : localScores) {
        scoresMap[docId] += score;
    }
}

std::vector<std::shared_ptr<Document>> SearchEngine::searchByContent(
    const String& query) {
    spdlog::debug("Searching by content: {}", std::string(query));

    if (query.empty()) {
        spdlog::warn("Empty query provided for content search");
        return {};
    }

    auto words = tokenizeContent(query);
    if (words.empty()) {
        spdlog::warn("No valid tokens in query");
        return {};
    }

    HashMap<String, double> scores;
    threading::mutex scoresMutex;

    try {
        if (words.size() <= 2 || maxThreads_ <= 1) {
            searchByContentWorker(words, scores, scoresMutex);
        } else {
            std::vector<threading::future<void>> futures;
            size_t numWords = words.size();
            size_t chunkSize = std::max(size_t(1), numWords / maxThreads_);

            for (size_t i = 0; i < numWords; i += chunkSize) {
                size_t end = std::min(i + chunkSize, numWords);
                std::vector<String> wordChunk(words.begin() + i,
                                              words.begin() + end);

#ifdef ATOM_USE_BOOST
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
                        }
                    }
                });
#else
                futures.push_back(std::async(
                    std::launch::async, &SearchEngine::searchByContentWorker,
                    this, wordChunk, std::ref(scores), std::ref(scoresMutex)));
#endif
            }

            for (auto& future : futures) {
                try {
                    future.get();
                } catch (const std::exception& e) {
                    spdlog::error("Exception in content search worker: {}",
                                  e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during content search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    auto results = getRankedResults(scores);
    spdlog::debug("Found {} documents matching content query", results.size());
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::booleanSearch(
    const String& query) {
    spdlog::debug("Performing boolean search: {}", std::string(query));

    if (query.empty()) {
        spdlog::warn("Empty query provided for boolean search");
        return {};
    }

    HashMap<String, double> scores;
    std::istringstream iss{std::string(query)};
    std::string wordStd;
    bool isNot = false;

    try {
        threading::shared_lock lock(indexMutex_);

        while (iss >> wordStd) {
            if (wordStd == "NOT") {
                isNot = true;
                continue;
            }

            if (wordStd == "AND" || wordStd == "OR") {
                continue;
            }

            std::transform(wordStd.begin(), wordStd.end(), wordStd.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            wordStd =
                std::regex_replace(wordStd, std::regex("[^a-zA-Z0-9]"), "");

            if (wordStd.empty()) {
                continue;
            }

            String wordKey(wordStd);
            auto it = contentIndex_.find(wordKey);
            if (it != contentIndex_.end()) {
                for (const auto& docId : it->second) {
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        double tfidfScore =
                            tfIdf(*docIt->second, std::string_view(wordKey));

                        if (isNot) {
                            scores[docId] -= tfidfScore * 2.0;
                            spdlog::trace(
                                "Word '{}' excluded from document id: {}",
                                wordStd, std::string(docId));
                        } else {
                            scores[docId] += tfidfScore;
                            spdlog::trace(
                                "Word '{}' included in document id: {}",
                                wordStd, std::string(docId));
                        }
                    }
                }
            }
            isNot = false;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during boolean search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    auto results = getRankedResults(scores);
    spdlog::debug("Found {} documents matching boolean query", results.size());
    return results;
}

std::vector<String> SearchEngine::autoComplete(const String& prefix,
                                               size_t maxResults) {
    spdlog::debug("Auto-completing for prefix: {}", std::string(prefix));

    if (prefix.empty()) {
        spdlog::warn("Empty prefix provided for autocomplete");
        return {};
    }

    std::vector<String> suggestions;
    std::string prefixStd = std::string(prefix);
    std::transform(prefixStd.begin(), prefixStd.end(), prefixStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    size_t prefixLen = prefixStd.length();

    try {
        threading::shared_lock lock(indexMutex_);

        for (const auto& [tag, _] : tagIndex_) {
            if (tag.size() >= prefixLen) {
                std::string tagLower = tag;
                std::transform(tagLower.begin(), tagLower.end(),
                               tagLower.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (tagLower.rfind(prefixStd, 0) == 0) {
                    suggestions.push_back(String(tag));
                    spdlog::trace("Tag suggestion: {}", tag);
                }
            }
            if (maxResults > 0 && suggestions.size() >= maxResults)
                break;
        }

        if (maxResults == 0 || suggestions.size() < maxResults) {
            for (const auto& [word, _] : contentIndex_) {
                if (word.size() >= prefixLen) {
                    std::string wordStd = std::string(word);
                    std::transform(
                        wordStd.begin(), wordStd.end(), wordStd.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (wordStd.rfind(prefixStd, 0) == 0) {
                        bool found = false;
                        for (const auto& sug : suggestions) {
                            if (sug == word) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            suggestions.push_back(word);
                            spdlog::trace("Content suggestion: {}",
                                          std::string(word));
                        }
                    }
                }
                if (maxResults > 0 && suggestions.size() >= maxResults) {
                    break;
                }
            }
        }

        std::sort(suggestions.begin(), suggestions.end(),
                  [this](const String& a, const String& b) {
                      std::string keyA = std::string(a);
                      std::string keyB = std::string(b);
                      int freqA = docFrequency_.count(keyA)
                                      ? docFrequency_.at(keyA)
                                      : 0;
                      int freqB = docFrequency_.count(keyB)
                                      ? docFrequency_.at(keyB)
                                      : 0;
                      return freqA > freqB;
                  });

        if (maxResults > 0 && suggestions.size() > maxResults) {
            suggestions.resize(maxResults);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during autocomplete: {}", e.what());
        throw SearchOperationException(e.what());
    }

    spdlog::debug("Found {} suggestions for prefix '{}'", suggestions.size(),
                  std::string(prefix));
    return suggestions;
}

void SearchEngine::saveIndex(const String& filename) const {
    spdlog::info("Saving index to file: {}", std::string(filename));

    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }

    try {
        threading::shared_lock lock(indexMutex_);
        std::ofstream ofs(std::string(filename), std::ios::binary);
        if (!ofs) {
            std::string errMsg =
                "Failed to open file for writing: " + std::string(filename);
            spdlog::error("{}", errMsg);
            throw std::ios_base::failure(errMsg);
        }

        int totalDocsValue = totalDocs_.load();
        ofs.write(reinterpret_cast<const char*>(&totalDocsValue),
                  sizeof(totalDocsValue));

        size_t docSize = documents_.size();
        ofs.write(reinterpret_cast<const char*>(&docSize), sizeof(docSize));

        for (const auto& [docId, doc] : documents_) {
            std::string docIdStd = std::string(docId);
            size_t idLength = docIdStd.size();
            ofs.write(reinterpret_cast<const char*>(&idLength),
                      sizeof(idLength));
            ofs.write(docIdStd.c_str(), idLength);

            std::string contentStd = std::string(doc->getContent());
            size_t contentLength = contentStd.size();
            ofs.write(reinterpret_cast<const char*>(&contentLength),
                      sizeof(contentLength));
            ofs.write(contentStd.c_str(), contentLength);

            const auto& tags = doc->getTags();
            size_t tagsCount = tags.size();
            ofs.write(reinterpret_cast<const char*>(&tagsCount),
                      sizeof(tagsCount));

            for (const auto& tag : tags) {
                size_t tagLength = tag.size();
                ofs.write(reinterpret_cast<const char*>(&tagLength),
                          sizeof(tagLength));
                ofs.write(tag.c_str(), tagLength);
            }

            int clickCount = doc->getClickCount();
            ofs.write(reinterpret_cast<const char*>(&clickCount),
                      sizeof(clickCount));
        }

        spdlog::info("Index saved successfully to {}", std::string(filename));
    } catch (const std::ios_base::failure& e) {
        spdlog::error("I/O error while saving index: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error while saving index: {}", e.what());
        throw;
    }
}

void SearchEngine::loadIndex(const String& filename) {
    spdlog::info("Loading index from file: {}", std::string(filename));

    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }

    try {
        std::unique_lock<threading::shared_mutex> lock(indexMutex_);
        std::ifstream ifs(std::string(filename), std::ios::binary);
        if (!ifs) {
            std::string errMsg =
                "Failed to open file for reading: " + std::string(filename);
            spdlog::error("{}", errMsg);
            throw std::ios_base::failure(errMsg);
        }

        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        totalDocs_ = 0;

        int totalDocsValue;
        if (!ifs.read(reinterpret_cast<char*>(&totalDocsValue),
                      sizeof(totalDocsValue))) {
            if (ifs.eof()) {
                spdlog::info(
                    "Index file {} is empty or truncated at totalDocs.",
                    std::string(filename));
                return;
            } else {
                throw std::ios_base::failure(
                    "Failed to read totalDocs from index file: " +
                    std::string(filename));
            }
        }
        totalDocs_ = totalDocsValue;

        size_t docSize;
        if (!ifs.read(reinterpret_cast<char*>(&docSize), sizeof(docSize))) {
            if (ifs.eof() && totalDocsValue == 0) {
                spdlog::info("Index file {} contains 0 documents.",
                             std::string(filename));
                return;
            } else {
                throw std::ios_base::failure(
                    "Failed to read docSize from index file: " +
                    std::string(filename));
            }
        }

        for (size_t i = 0; i < docSize; ++i) {
            size_t idLength;
            if (!ifs.read(reinterpret_cast<char*>(&idLength), sizeof(idLength)))
                throw std::ios_base::failure("Failed to read idLength");
            std::string docIdStd(idLength, '\0');
            if (!ifs.read(&docIdStd[0], idLength))
                throw std::ios_base::failure("Failed to read docId");
            String docId(docIdStd);

            size_t contentLength;
            if (!ifs.read(reinterpret_cast<char*>(&contentLength),
                          sizeof(contentLength)))
                throw std::ios_base::failure("Failed to read contentLength");
            std::string contentStd(contentLength, '\0');
            if (!ifs.read(&contentStd[0], contentLength))
                throw std::ios_base::failure("Failed to read content");
            String content(contentStd);

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
                tags.insert(tagStd);
            }

            int clickCount;
            if (!ifs.read(reinterpret_cast<char*>(&clickCount),
                          sizeof(clickCount)))
                throw std::ios_base::failure("Failed to read clickCount");

            auto doc = std::make_shared<Document>(
                docId, content, std::initializer_list<std::string>{});
            for (const auto& tag : tags) {
                doc->addTag(tag);
            }
            doc->setClickCount(clickCount);

            documents_[docId] = doc;

            for (const auto& tag : tags) {
                tagIndex_[tag].push_back(docId);
                docFrequency_[tag]++;
            }

            addContentToIndex(doc);
        }

        if (documents_.size() != static_cast<size_t>(totalDocs_.load())) {
            spdlog::warn(
                "Loaded document count ({}) does not match stored totalDocs "
                "({}) in file {}",
                documents_.size(), totalDocs_.load(), std::string(filename));
        }

        spdlog::info("Index loaded successfully from {}, total docs: {}",
                     std::string(filename), totalDocs_.load());
    } catch (const std::ios_base::failure& e) {
        spdlog::error("I/O error while loading index: {}", e.what());
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        totalDocs_ = 0;
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error while loading index: {}", e.what());
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        totalDocs_ = 0;
        throw;
    }
}

int SearchEngine::levenshteinDistanceSIMD(std::string_view s1,
                                          std::string_view s2) const noexcept {
    const size_t m = s1.length();
    const size_t n = s2.length();

    if (m == 0)
        return static_cast<int>(n);
    if (n == 0)
        return static_cast<int>(m);

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
        prevRow.swap(currRow);
    }

    return prevRow[n];
}

double SearchEngine::tfIdf(const Document& doc,
                           std::string_view term) const noexcept {
    std::string contentStd = std::string(doc.getContent());
    std::string termStd = std::string(term);

    std::transform(contentStd.begin(), contentStd.end(), contentStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(termStd.begin(), termStd.end(), termStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    size_t count = 0;
    size_t pos = 0;
    size_t contentLen = contentStd.length();
    size_t termLen = termStd.length();
    if (termLen == 0)
        return 0.0;

    while ((pos = contentStd.find(termStd, pos)) != std::string::npos) {
        count++;
        pos += termLen;
    }

    if (count == 0)
        return 0.0;

    double tf =
        (contentLen > 0)
            ? (static_cast<double>(count) / static_cast<double>(contentLen))
            : 0.0;

    double df = 1.0;
    auto freqIt = docFrequency_.find(termStd);
    if (freqIt != docFrequency_.end()) {
        df = static_cast<double>(freqIt->second);
    }

    int docsTotal = totalDocs_.load();
    double idf =
        (docsTotal > 0 && df > 0 && static_cast<double>(docsTotal) >= df)
            ? std::log(static_cast<double>(docsTotal) / df)
            : 0.0;

    double clickBoost =
        1.0 + std::log1p(static_cast<double>(doc.getClickCount()) * 0.1);
    double tfIdfValue = tf * idf * clickBoost;
    return tfIdfValue;
}

std::shared_ptr<Document> SearchEngine::findDocumentById(const String& docId) {
    spdlog::debug("Finding document by id: {}", std::string(docId));

    if (docId.empty()) {
        throw std::invalid_argument("Document ID cannot be empty");
    }

    threading::shared_lock lock(indexMutex_);
    auto it = documents_.find(docId);
    if (it == documents_.end()) {
        spdlog::error("Document not found: {}", std::string(docId));
        throw DocumentNotFoundException(docId);
    }

    spdlog::debug("Document found: {}", std::string(docId));
    return it->second;
}

std::vector<std::shared_ptr<Document>> SearchEngine::getRankedResults(
    const HashMap<String, double>& scores) {
    struct ScoredDoc {
        std::shared_ptr<Document> doc;
        double score;

        bool operator<(const ScoredDoc& other) const {
            return score < other.score;
        }
    };

    std::priority_queue<ScoredDoc> priorityQueue;
    threading::shared_lock lock(indexMutex_);

    for (const auto& [docId, score] : scores) {
        if (score <= 0)
            continue;

        auto it = documents_.find(docId);
        if (it != documents_.end()) {
            auto doc = it->second;
            priorityQueue.push({doc, score});
            spdlog::trace("Document id: {}, score: {:.6f}",
                          std::string(doc->getId()), score);
        } else {
            spdlog::warn(
                "Document ID {} found in scores but not in documents map "
                "during ranking.",
                std::string(docId));
        }
    }
    lock.unlock();

    std::vector<std::shared_ptr<Document>> results;
    results.reserve(priorityQueue.size());

    while (!priorityQueue.empty()) {
        results.push_back(priorityQueue.top().doc);
        priorityQueue.pop();
    }

    spdlog::info("Ranked results obtained: {} documents", results.size());
    return results;
}

}  // namespace atom::search
