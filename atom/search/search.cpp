#include "search.hpp"

#include <immintrin.h>  // For SIMD
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <future>
#include <queue>
#include <regex>
#include <sstream>
#include <thread>

#include "atom/log/loguru.hpp"

namespace atom::search {

// Document implementation
Document::Document(std::string id, std::string content,
                   std::initializer_list<std::string> tags)
    : id_(std::move(id)), content_(std::move(content)), tags_(tags) {
    validate();
    LOG_F(INFO, "Document created with id: {}", id_);
}

void Document::validate() const {
    if (id_.empty()) {
        throw DocumentValidationException("Document ID cannot be empty");
    }

    if (id_.length() > 256) {
        throw DocumentValidationException(
            "Document ID too long (max 256 chars)");
    }

    if (content_.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }

    // Check for any invalid tags
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

void Document::setContent(std::string content) {
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

// SearchEngine implementation
SearchEngine::SearchEngine(unsigned maxThreads)
    : maxThreads_(maxThreads ? maxThreads
                             : std::thread::hardware_concurrency()) {
    LOG_F(INFO, "SearchEngine initialized with max threads: {}", maxThreads_);
}

void SearchEngine::addDocument(const Document& doc) {
    try {
        // Create a shared pointer from the document
        auto docPtr = std::make_shared<Document>(doc);
        addDocument(std::move(*docPtr));
    } catch (const DocumentValidationException& e) {
        LOG_F(ERROR, "Failed to add document: {}", e.what());
        throw;
    }
}

void SearchEngine::addDocument(Document&& doc) {
    LOG_F(INFO, "Adding document with id: {}", doc.getId());

    // Validation
    try {
        doc.validate();
    } catch (const DocumentValidationException& e) {
        LOG_F(ERROR, "Document validation failed: {}", e.what());
        throw;
    }

    std::unique_lock lock(indexMutex_);
    const std::string& docId = std::string(doc.getId());

    // Check if document already exists
    if (documents_.count(docId) > 0) {
        LOG_F(ERROR, "Document with ID {} already exists", docId);
        throw std::invalid_argument("Document with this ID already exists");
    }

    // Add to documents collection
    auto docPtr = std::make_shared<Document>(std::move(doc));
    documents_[docId] = docPtr;

    // Add to tag index
    for (const auto& tag : docPtr->getTags()) {
        tagIndex_[tag].push_back(docId);
        docFrequency_[tag]++;
        LOG_F(INFO, "Tag '{}' added to index", tag);
    }

    // Add to content index
    addContentToIndex(docPtr);

    // Increment document count
    totalDocs_++;
    LOG_F(INFO, "Document added successfully, total docs: {}",
          totalDocs_.load());
}

void SearchEngine::removeDocument(const std::string& docId) {
    LOG_F(INFO, "Removing document with id: {}", docId);

    if (docId.empty()) {
        throw std::invalid_argument("Document ID cannot be empty");
    }

    std::unique_lock lock(indexMutex_);

    // Check if document exists
    auto docIt = documents_.find(docId);
    if (docIt == documents_.end()) {
        LOG_F(ERROR, "Document with ID {} not found", docId);
        throw DocumentNotFoundException(docId);
    }

    auto& doc = docIt->second;

    // Remove from tagIndex_
    for (const auto& tag : doc->getTags()) {
        auto& docs = tagIndex_[tag];
        docs.erase(std::remove(docs.begin(), docs.end(), docId), docs.end());

        if (docs.empty()) {
            tagIndex_.erase(tag);
        }

        // Update document frequency
        auto& freq = docFrequency_[tag];
        if (--freq <= 0) {
            docFrequency_.erase(tag);
        }
    }

    // Remove from contentIndex_
    auto tokens = tokenizeContent(std::string(doc->getContent()));
    for (const auto& token : tokens) {
        auto& docs = contentIndex_[token];
        docs.erase(docId);

        if (docs.empty()) {
            contentIndex_.erase(token);
        }

        // Update document frequency
        auto& freq = docFrequency_[token];
        if (--freq <= 0) {
            docFrequency_.erase(token);
        }
    }

    // Remove from documents collection
    documents_.erase(docIt);
    totalDocs_--;

    LOG_F(INFO, "Document with id: {} removed, total docs: {}", docId,
          totalDocs_.load());
}

void SearchEngine::updateDocument(const Document& doc) {
    LOG_F(INFO, "Updating document with id: {}", doc.getId());

    try {
        // Validate document
        doc.validate();

        std::unique_lock lock(indexMutex_);

        const std::string& docId = std::string(doc.getId());

        // Check if document exists
        if (documents_.find(docId) == documents_.end()) {
            LOG_F(ERROR, "Document with ID {} not found", docId);
            throw DocumentNotFoundException(docId);
        }

        // Remove old document
        removeDocument(docId);

        // Add updated document
        addDocument(doc);

        LOG_F(INFO, "Document with id: {} updated", docId);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error updating document: {}", e.what());
        throw;
    }
}

void SearchEngine::addContentToIndex(const std::shared_ptr<Document>& doc) {
    LOG_F(INFO, "Indexing content for document id: {}", doc->getId());

    auto tokens = tokenizeContent(std::string(doc->getContent()));
    std::string docId = std::string(doc->getId());

    for (const auto& token : tokens) {
        contentIndex_[token].insert(docId);
        docFrequency_[token]++;
        LOG_F(INFO, "Token '{}' indexed for document id: {}", token, docId);
    }
}

std::vector<std::string> SearchEngine::tokenizeContent(
    const std::string& content) const {
    std::vector<std::string> tokens;
    std::stringstream ss(content);
    std::string token;

    // Simple tokenization by whitespace
    while (ss >> token) {
        // Convert to lowercase and remove non-alphanumeric characters
        token = std::regex_replace(token, std::regex("[^a-zA-Z0-9]"), "");

        // Only add non-empty tokens
        if (!token.empty()) {
            // Convert to lowercase
            std::transform(token.begin(), token.end(), token.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            tokens.push_back(token);
        }
    }

    return tokens;
}

std::vector<std::shared_ptr<Document>> SearchEngine::searchByTag(
    const std::string& tag) {
    LOG_F(INFO, "Searching by tag: {}", tag);

    if (tag.empty()) {
        LOG_F(WARNING, "Empty tag provided for search");
        return {};
    }

    std::vector<std::shared_ptr<Document>> results;

    try {
        std::shared_lock lock(indexMutex_);

        auto it = tagIndex_.find(tag);
        if (it != tagIndex_.end()) {
            results.reserve(it->second.size());
            for (const auto& docId : it->second) {
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    results.push_back(docIt->second);
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
    std::unordered_set<std::string> processedDocIds;

    try {
        std::shared_lock lock(indexMutex_);

        // Divide work among threads
        std::vector<std::future<std::vector<std::string>>> futures;
        std::vector<std::string> tagKeys;

        // Get all tag keys
        for (const auto& [key, _] : tagIndex_) {
            tagKeys.push_back(key);
        }

        // Calculate chunk size for parallel processing
        size_t chunkSize = std::max(size_t(1), tagKeys.size() / maxThreads_);

        // Launch worker threads
        for (size_t i = 0; i < tagKeys.size(); i += chunkSize) {
            size_t end = std::min(i + chunkSize, tagKeys.size());
            futures.push_back(std::async(
                std::launch::async,
                [this, &tag, tolerance, &tagKeys, i, end]() {
                    std::vector<std::string> matchedDocIds;

                    for (size_t j = i; j < end; j++) {
                        const auto& key = tagKeys[j];
                        if (levenshteinDistanceSIMD(tag, key) <= tolerance) {
                            // Add all document IDs for this tag
                            const auto& docIds = tagIndex_.at(key);
                            matchedDocIds.insert(matchedDocIds.end(),
                                                 docIds.begin(), docIds.end());
                            LOG_F(INFO, "Tag '{}' matched with '{}'", key, tag);
                        }
                    }

                    return matchedDocIds;
                }));
        }

        // Collect results
        for (auto& future : futures) {
            auto docIds = future.get();
            for (const auto& docId : docIds) {
                if (processedDocIds.insert(docId).second) {
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        results.push_back(docIt->second);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during fuzzy tag search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    LOG_F(INFO, "Found {} documents with fuzzy tag match for '{}'",
          results.size(), tag);
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::searchByTags(
    const std::vector<std::string>& tags) {
    LOG_F(INFO, "Searching by multiple tags");

    if (tags.empty()) {
        LOG_F(WARNING, "Empty tags list provided for search");
        return {};
    }

    std::unordered_map<std::string, double> scores;

    try {
        std::shared_lock lock(indexMutex_);

        for (const auto& tag : tags) {
            auto it = tagIndex_.find(tag);
            if (it != tagIndex_.end()) {
                for (const auto& docId : it->second) {
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        scores[docId] += tfIdf(*docIt->second, tag);
                        LOG_F(INFO, "Tag '{}' found in document id: {}", tag,
                              docId);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during multi-tag search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    auto results = getRankedResults(scores);
    LOG_F(INFO, "Found {} documents matching the tags", results.size());
    return results;
}

void SearchEngine::searchByContentWorker(
    const std::vector<std::string>& wordChunk,
    std::unordered_map<std::string, double>& scoresMap,
    std::mutex& scoresMutex) {
    std::unordered_map<std::string, double> localScores;

    for (const auto& word : wordChunk) {
        std::shared_lock lock(indexMutex_);

        auto it = contentIndex_.find(word);
        if (it != contentIndex_.end()) {
            for (const auto& docId : it->second) {
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    localScores[docId] += tfIdf(*docIt->second, word);
                    LOG_F(INFO, "Word '{}' found in document id: {}", word,
                          docId);
                }
            }
        }
    }

    // Merge results with main scores map
    std::lock_guard<std::mutex> lock(scoresMutex);
    for (const auto& [docId, score] : localScores) {
        scoresMap[docId] += score;
    }
}

std::vector<std::shared_ptr<Document>> SearchEngine::searchByContent(
    const std::string& query) {
    LOG_F(INFO, "Searching by content: {}", query);

    if (query.empty()) {
        LOG_F(WARNING, "Empty query provided for content search");
        return {};
    }

    auto words = tokenizeContent(query);
    if (words.empty()) {
        LOG_F(WARNING, "No valid tokens in query");
        return {};
    }

    std::unordered_map<std::string, double> scores;
    std::mutex scoresMutex;

    try {
        // If we have few words, no need for parallel processing
        if (words.size() <= 2 || maxThreads_ <= 1) {
            searchByContentWorker(words, scores, scoresMutex);
        } else {
            // Parallel processing with thread pool
            std::vector<std::future<void>> futures;

            // Calculate chunk size
            size_t chunkSize = std::max(size_t(1), words.size() / maxThreads_);

            // Launch worker threads
            for (size_t i = 0; i < words.size(); i += chunkSize) {
                size_t end = std::min(i + chunkSize, words.size());
                std::vector<std::string> wordChunk(words.begin() + i,
                                                   words.begin() + end);

                futures.push_back(std::async(
                    std::launch::async, &SearchEngine::searchByContentWorker,
                    this, wordChunk, std::ref(scores), std::ref(scoresMutex)));
            }

            // Wait for all threads to complete
            for (auto& future : futures) {
                future.get();
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during content search: {}", e.what());
        throw SearchOperationException(e.what());
    }

    auto results = getRankedResults(scores);
    LOG_F(INFO, "Found {} documents matching content query", results.size());
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::booleanSearch(
    const std::string& query) {
    LOG_F(INFO, "Performing boolean search: {}", query);

    if (query.empty()) {
        LOG_F(WARNING, "Empty query provided for boolean search");
        return {};
    }

    std::unordered_map<std::string, double> scores;
    std::istringstream iss(query);
    std::string word;
    bool isNot = false;

    try {
        std::shared_lock lock(indexMutex_);

        while (iss >> word) {
            if (word == "NOT") {
                isNot = true;
                continue;
            }

            if (word == "AND" || word == "OR") {
                continue;  // Skip these operators for now
            }

            // Convert to lowercase and clean
            std::transform(word.begin(), word.end(), word.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            word = std::regex_replace(word, std::regex("[^a-zA-Z0-9]"), "");

            if (word.empty()) {
                continue;
            }

            auto it = contentIndex_.find(word);
            if (it != contentIndex_.end()) {
                for (const auto& docId : it->second) {
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        double tfidfScore = tfIdf(*docIt->second, word);

                        if (isNot) {
                            scores[docId] -=
                                tfidfScore * 2.0;  // Double negative weight
                            LOG_F(INFO,
                                  "Word '{}' excluded from document id: {}",
                                  word, docId);
                        } else {
                            scores[docId] += tfidfScore;
                            LOG_F(INFO, "Word '{}' included in document id: {}",
                                  word, docId);
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

    auto results = getRankedResults(scores);
    LOG_F(INFO, "Found {} documents matching boolean query", results.size());
    return results;
}

std::vector<std::string> SearchEngine::autoComplete(const std::string& prefix,
                                                    size_t maxResults) {
    LOG_F(INFO, "Auto-completing for prefix: {}", prefix);

    if (prefix.empty()) {
        LOG_F(WARNING, "Empty prefix provided for autocomplete");
        return {};
    }

    std::vector<std::string> suggestions;

    try {
        std::shared_lock lock(indexMutex_);

        // Use prefix match on both tag and content indices
        for (const auto& [tag, _] : tagIndex_) {
            if (tag.size() >= prefix.size() &&
                std::equal(prefix.begin(), prefix.end(), tag.begin(),
                           [](char a, char b) {
                               return std::tolower(a) == std::tolower(b);
                           })) {
                suggestions.push_back(tag);
                LOG_F(INFO, "Tag suggestion: {}", tag);
            }
        }

        // Add from content index as well
        for (const auto& [word, _] : contentIndex_) {
            if (word.size() >= prefix.size() &&
                std::equal(prefix.begin(), prefix.end(), word.begin(),
                           [](char a, char b) {
                               return std::tolower(a) == std::tolower(b);
                           }) &&
                std::find(suggestions.begin(), suggestions.end(), word) ==
                    suggestions.end()) {
                suggestions.push_back(word);
                LOG_F(INFO, "Content suggestion: {}", word);
            }

            // Limit results if requested
            if (maxResults > 0 && suggestions.size() >= maxResults) {
                break;
            }
        }

        // Sort by relevance (could be improved to use docFrequency)
        std::sort(
            suggestions.begin(), suggestions.end(),
            [this](const std::string& a, const std::string& b) {
                int freqA = docFrequency_.count(a) ? docFrequency_.at(a) : 0;
                int freqB = docFrequency_.count(b) ? docFrequency_.at(b) : 0;
                return freqA > freqB;
            });

        // Limit results if requested
        if (maxResults > 0 && suggestions.size() > maxResults) {
            suggestions.resize(maxResults);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error during autocomplete: {}", e.what());
        throw SearchOperationException(e.what());
    }

    LOG_F(INFO, "Found {} suggestions for prefix '{}'", suggestions.size(),
          prefix);
    return suggestions;
}

void SearchEngine::saveIndex(const std::string& filename) const {
    LOG_F(INFO, "Saving index to file: {}", filename);

    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }

    try {
        std::shared_lock lock(indexMutex_);

        std::ofstream ofs(filename, std::ios::binary);
        if (!ofs) {
            std::string errMsg = "Failed to open file for writing: " + filename;
            LOG_F(ERROR, "{}", errMsg);
            throw std::ios_base::failure(errMsg);
        }

        // Write total document count
        int totalDocsValue = totalDocs_.load();
        ofs.write(reinterpret_cast<const char*>(&totalDocsValue),
                  sizeof(totalDocsValue));

        // Write documents
        size_t docSize = documents_.size();
        ofs.write(reinterpret_cast<const char*>(&docSize), sizeof(docSize));

        for (const auto& [docId, doc] : documents_) {
            // Write document id
            size_t idLength = docId.size();
            ofs.write(reinterpret_cast<const char*>(&idLength),
                      sizeof(idLength));
            ofs.write(docId.c_str(), idLength);

            // Write document content
            std::string content = std::string(doc->getContent());
            size_t contentLength = content.size();
            ofs.write(reinterpret_cast<const char*>(&contentLength),
                      sizeof(contentLength));
            ofs.write(content.c_str(), contentLength);

            // Write tags
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

            // Write click count
            int clickCount = doc->getClickCount();
            ofs.write(reinterpret_cast<const char*>(&clickCount),
                      sizeof(clickCount));
        }

        LOG_F(INFO, "Index saved successfully to {}", filename);
    } catch (const std::ios_base::failure& e) {
        LOG_F(ERROR, "I/O error while saving index: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error while saving index: {}", e.what());
        throw;
    }
}

void SearchEngine::loadIndex(const std::string& filename) {
    LOG_F(INFO, "Loading index from file: {}", filename);

    if (filename.empty()) {
        throw std::invalid_argument("Filename cannot be empty");
    }

    try {
        std::unique_lock lock(indexMutex_);

        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs) {
            std::string errMsg = "Failed to open file for reading: " + filename;
            LOG_F(ERROR, "{}", errMsg);
            throw std::ios_base::failure(errMsg);
        }

        // Clear existing data
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();

        // Read total document count
        int totalDocsValue;
        ifs.read(reinterpret_cast<char*>(&totalDocsValue),
                 sizeof(totalDocsValue));
        totalDocs_ = totalDocsValue;

        // Read documents
        size_t docSize;
        ifs.read(reinterpret_cast<char*>(&docSize), sizeof(docSize));

        for (size_t i = 0; i < docSize; ++i) {
            // Read document id
            size_t idLength;
            ifs.read(reinterpret_cast<char*>(&idLength), sizeof(idLength));
            std::string docId(idLength, ' ');
            ifs.read(&docId[0], idLength);

            // Read document content
            size_t contentLength;
            ifs.read(reinterpret_cast<char*>(&contentLength),
                     sizeof(contentLength));
            std::string content(contentLength, ' ');
            ifs.read(&content[0], contentLength);

            // Read tags
            std::set<std::string> tags;
            size_t tagsCount;
            ifs.read(reinterpret_cast<char*>(&tagsCount), sizeof(tagsCount));

            for (size_t j = 0; j < tagsCount; ++j) {
                size_t tagLength;
                ifs.read(reinterpret_cast<char*>(&tagLength),
                         sizeof(tagLength));
                std::string tag(tagLength, ' ');
                ifs.read(&tag[0], tagLength);
                tags.insert(tag);
            }

            // Read click count
            int clickCount;
            ifs.read(reinterpret_cast<char*>(&clickCount), sizeof(clickCount));

            // Create document
            auto doc = std::make_shared<Document>(
                docId, content, std::initializer_list<std::string>{});
            for (const auto& tag : tags) {
                doc->addTag(tag);
            }

            // Add to index
            documents_[docId] = doc;

            // Add to tag index
            for (const auto& tag : tags) {
                tagIndex_[tag].push_back(docId);
                docFrequency_[tag]++;
            }

            // Add to content index
            addContentToIndex(doc);
        }

        LOG_F(INFO, "Index loaded successfully from {}, total docs: {}",
              filename, totalDocs_.load());
    } catch (const std::ios_base::failure& e) {
        LOG_F(ERROR, "I/O error while loading index: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error while loading index: {}", e.what());
        throw;
    }
}

int SearchEngine::levenshteinDistanceSIMD(std::string_view s1,
                                          std::string_view s2) const noexcept {
    // For small strings or if SIMD is not available, use a simple
    // implementation
    const size_t m = s1.length();
    const size_t n = s2.length();

    // Handle trivial cases
    if (m == 0)
        return static_cast<int>(n);
    if (n == 0)
        return static_cast<int>(m);

    // Optimization for common prefix
    size_t prefixLen = 0;
    while (prefixLen < std::min(m, n) && s1[prefixLen] == s2[prefixLen]) {
        prefixLen++;
    }

    // Skip prefix in both strings
    s1 = s1.substr(prefixLen);
    s2 = s2.substr(prefixLen);
    const size_t newM = m - prefixLen;
    const size_t newN = n - prefixLen;

    // Optimization for common suffix
    size_t suffixLen = 0;
    while (suffixLen < std::min(newM, newN) &&
           s1[newM - 1 - suffixLen] == s2[newN - 1 - suffixLen]) {
        suffixLen++;
    }

    // Adjust for suffix
    const size_t compM = newM - suffixLen;
    const size_t compN = newN - suffixLen;

    // Edge cases after optimizations
    if (compM == 0)
        return static_cast<int>(compN);
    if (compN == 0)
        return static_cast<int>(compM);

    // Dynamic programming with two vectors
    std::vector<int> prevRow(compN + 1);
    std::vector<int> currRow(compN + 1);

    // Initialize first row
    for (size_t j = 0; j <= compN; j++) {
        prevRow[j] = static_cast<int>(j);
    }

    // Fill the matrix
    for (size_t i = 0; i < compM; i++) {
        currRow[0] = static_cast<int>(i + 1);

        for (size_t j = 0; j < compN; j++) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            currRow[j + 1] = std::min({
                prevRow[j + 1] + 1,  // deletion
                currRow[j] + 1,      // insertion
                prevRow[j] + cost    // substitution
            });
        }

        // Swap rows
        std::swap(prevRow, currRow);
    }

    return prevRow[compN];
}

double SearchEngine::tfIdf(const Document& doc,
                           std::string_view term) const noexcept {
    std::string content = std::string(doc.getContent());
    std::string termStr = std::string(term);

    // Convert both to lowercase
    std::transform(content.begin(), content.end(), content.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(termStr.begin(), termStr.end(), termStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Count occurrences of term in document
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find(termStr, pos)) != std::string::npos) {
        count++;
        pos += termStr.length();
    }

    if (count == 0)
        return 0.0;

    // Term frequency
    double tf =
        static_cast<double>(count) / static_cast<double>(content.length());

    // Inverse document frequency
    double df = docFrequency_.count(termStr) ? docFrequency_.at(termStr) : 1;
    double idf = std::log(static_cast<double>(totalDocs_) / df);

    // Add click count boost
    double clickBoost =
        1.0 + (doc.getClickCount() / 10.0);  // 10% boost per click

    double tfIdfValue = tf * idf * clickBoost;
    return tfIdfValue;
}

std::shared_ptr<Document> SearchEngine::findDocumentById(
    const std::string& docId) {
    LOG_F(INFO, "Finding document by id: {}", docId);

    if (docId.empty()) {
        throw std::invalid_argument("Document ID cannot be empty");
    }

    std::shared_lock lock(indexMutex_);

    auto it = documents_.find(docId);
    if (it == documents_.end()) {
        LOG_F(ERROR, "Document not found: {}", docId);
        throw DocumentNotFoundException(docId);
    }

    LOG_F(INFO, "Document found: {}", docId);
    return it->second;
}

std::vector<std::shared_ptr<Document>> SearchEngine::getRankedResults(
    const std::unordered_map<std::string, double>& scores) {
    struct ScoredDoc {
        std::shared_ptr<Document> doc;
        double score;

        bool operator<(const ScoredDoc& other) const {
            return score < other.score;
        }
    };

    std::priority_queue<ScoredDoc> priorityQueue;

    // Add all documents with scores to the priority queue
    for (const auto& [docId, score] : scores) {
        // Skip documents with negative scores
        if (score <= 0)
            continue;

        try {
            auto doc = findDocumentById(docId);
            double finalScore =
                score + (doc->getClickCount() * 0.1);  // 10% boost per click
            priorityQueue.push({doc, finalScore});
            LOG_F(INFO, "Document id: {}, score: {:.6f}", doc->getId(),
                  finalScore);
        } catch (const DocumentNotFoundException& e) {
            LOG_F(WARNING, "{}", e.what());
        }
    }

    // Extract sorted results
    std::vector<std::shared_ptr<Document>> results;
    results.reserve(priorityQueue.size());

    while (!priorityQueue.empty()) {
        results.push_back(priorityQueue.top().doc);
        priorityQueue.pop();
    }

    // Reverse to get highest scores first
    std::reverse(results.begin(), results.end());

    LOG_F(INFO, "Ranked results obtained: {} documents", results.size());
    return results;
}

}  // namespace atom::search