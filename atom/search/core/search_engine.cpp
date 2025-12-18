#include "search_engine.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

#include <spdlog/spdlog.h>

#ifdef ATOM_USE_BOOST
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#else
#include <chrono>
#include <thread>
#endif

namespace atom::search {

SearchEngine::SearchEngine(SearchEngineConfig config)
    : config_(std::move(config)) {
    if (config_.maxThreads == 0) {
#ifdef ATOM_USE_BOOST
        config_.maxThreads = boost::thread::hardware_concurrency();
#else
        config_.maxThreads = std::thread::hardware_concurrency();
#endif
    }
    if (config_.maxThreads == 0) {
        config_.maxThreads = 4;
    }

    spdlog::info("SearchEngine initialized with max threads: {}",
                 config_.maxThreads);

    taskQueue_ = std::make_unique<threading::lockfree_queue<SearchTask>>(4096);
    startWorkerThreads();
}

SearchEngine::SearchEngine(unsigned maxThreads)
    : SearchEngine(SearchEngineConfig{.maxThreads = maxThreads}) {}

SearchEngine::SearchEngine(SearchEngine&& other) noexcept
    : config_(std::move(other.config_)),
      tokenizer_(std::move(other.tokenizer_)),
      documents_(std::move(other.documents_)),
      tagIndex_(std::move(other.tagIndex_)),
      contentIndex_(std::move(other.contentIndex_)),
      docFrequency_(std::move(other.docFrequency_)),
      termFrequency_(std::move(other.termFrequency_)),
      totalDocs_(other.totalDocs_.load(std::memory_order_acquire)),
      totalTokens_(other.totalTokens_.load(std::memory_order_acquire)),
      avgDocLength_(other.avgDocLength_) {
    other.stopWorkerThreads();
    taskQueue_ = std::move(other.taskQueue_);
    startWorkerThreads();
}

SearchEngine& SearchEngine::operator=(SearchEngine&& other) noexcept {
    if (this != &other) {
        stopWorkerThreads();

        config_ = std::move(other.config_);
        tokenizer_ = std::move(other.tokenizer_);
        documents_ = std::move(other.documents_);
        tagIndex_ = std::move(other.tagIndex_);
        contentIndex_ = std::move(other.contentIndex_);
        docFrequency_ = std::move(other.docFrequency_);
        termFrequency_ = std::move(other.termFrequency_);
        totalDocs_.store(other.totalDocs_.load(std::memory_order_acquire),
                         std::memory_order_release);
        totalTokens_.store(other.totalTokens_.load(std::memory_order_acquire),
                           std::memory_order_release);
        avgDocLength_ = other.avgDocLength_;

        other.stopWorkerThreads();
        taskQueue_ = std::move(other.taskQueue_);
        startWorkerThreads();
    }
    return *this;
}

SearchEngine::~SearchEngine() {
    spdlog::debug("SearchEngine being destroyed");
    stopWorkerThreads();
}

void SearchEngine::startWorkerThreads() {
    shouldStopWorkers_.store(false, std::memory_order_release);
    workerThreads_.clear();
    workerThreads_.reserve(config_.maxThreads);

    for (unsigned i = 0; i < config_.maxThreads; ++i) {
        workerThreads_.push_back(std::make_unique<threading::thread>(
            [this]() { workerFunction(); }));
    }
}

void SearchEngine::stopWorkerThreads() {
    shouldStopWorkers_.store(true, std::memory_order_release);

    for (auto& thread : workerThreads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    workerThreads_.clear();
}

void SearchEngine::workerFunction() {
    SearchTask task;

    while (!shouldStopWorkers_.load(std::memory_order_acquire)) {
        if (taskQueue_->pop(task)) {
            try {
                task.callback(std::span<const String>(task.words));
            } catch (const std::exception& e) {
                spdlog::error("Error in worker thread: {}", e.what());
            }
        } else {
#ifdef ATOM_USE_BOOST
            boost::this_thread::sleep_for(boost::chrono::microseconds(100));
#else
            std::this_thread::sleep_for(std::chrono::microseconds(100));
#endif
        }
    }
}

SearchResult<void> SearchEngine::addDocument(const Document& doc) {
    Document tempDoc = doc;
    return addDocument(std::move(tempDoc));
}

SearchResult<void> SearchEngine::addDocument(Document&& doc) {
    auto validationResult = doc.validate();
    if (!validationResult) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    std::unique_lock<threading::shared_mutex> lock(indexMutex_);

    if (documents_.size() >= config_.maxDocuments) {
        return std::unexpected(SearchErrorCode::InternalError);
    }

    String docId = String(doc.getId());

    if (documents_.contains(docId)) {
        return std::unexpected(SearchErrorCode::DuplicateDocument);
    }

    auto docPtr = std::make_shared<Document>(std::move(doc));
    documents_[docId] = docPtr;

    for (const auto& tag : docPtr->getTags()) {
        tagIndex_[tag].push_back(docId);
        docFrequency_[String(tag)]++;
    }

    addContentToIndex(docPtr);

    size_t newTotal = totalDocs_.fetch_add(1, std::memory_order_acq_rel) + 1;

    auto tokens = tokenizer_.tokenize(docPtr->getContent());
    size_t docTokens = tokens.size();
    totalTokens_.fetch_add(docTokens, std::memory_order_acq_rel);
    avgDocLength_ =
        static_cast<double>(totalTokens_.load(std::memory_order_acquire)) /
        static_cast<double>(newTotal);

    return {};
}

SearchResult<void> SearchEngine::removeDocument(std::string_view docId) {
    if (docId.empty()) {
        return std::unexpected(SearchErrorCode::InvalidQuery);
    }

    std::unique_lock<threading::shared_mutex> lock(indexMutex_);

    String docIdStr(docId);
    auto docIt = documents_.find(docIdStr);
    if (docIt == documents_.end()) {
        return std::unexpected(SearchErrorCode::DocumentNotFound);
    }

    auto& doc = docIt->second;

    for (const auto& tag : doc->getTags()) {
        auto tagIt = tagIndex_.find(tag);
        if (tagIt != tagIndex_.end()) {
            std::erase(tagIt->second, docIdStr);
            if (tagIt->second.empty()) {
                tagIndex_.erase(tagIt);
            }
        }

        auto freqIt = docFrequency_.find(String(tag));
        if (freqIt != docFrequency_.end()) {
            if (--(freqIt->second) <= 0) {
                docFrequency_.erase(freqIt);
            }
        }
    }

    removeContentFromIndex(doc);

    auto tokens = tokenizer_.tokenize(doc->getContent());
    totalTokens_.fetch_sub(tokens.size(), std::memory_order_acq_rel);

    documents_.erase(docIt);
    size_t remaining = totalDocs_.fetch_sub(1, std::memory_order_acq_rel) - 1;

    if (remaining > 0) {
        avgDocLength_ =
            static_cast<double>(totalTokens_.load(std::memory_order_acquire)) /
            static_cast<double>(remaining);
    } else {
        avgDocLength_ = 0.0;
    }

    return {};
}

SearchResult<void> SearchEngine::updateDocument(const Document& doc) {
    auto validationResult = doc.validate();
    if (!validationResult) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    String docId = String(doc.getId());

    {
        threading::shared_lock lock(indexMutex_);
        if (!documents_.contains(docId)) {
            return std::unexpected(SearchErrorCode::DocumentNotFound);
        }
    }

    auto removeResult = removeDocument(docId);
    if (!removeResult) {
        return removeResult;
    }

    return addDocument(doc);
}

SearchResult<void> SearchEngine::upsertDocument(Document doc) {
    String docId = String(doc.getId());

    {
        threading::shared_lock lock(indexMutex_);
        if (documents_.contains(docId)) {
            lock.unlock();
            return updateDocument(doc);
        }
    }

    return addDocument(std::move(doc));
}

void SearchEngine::clear() noexcept {
    std::unique_lock<threading::shared_mutex> lock(indexMutex_);
    documents_.clear();
    tagIndex_.clear();
    contentIndex_.clear();
    docFrequency_.clear();
    termFrequency_.clear();
    totalDocs_.store(0, std::memory_order_release);
    totalTokens_.store(0, std::memory_order_release);
    avgDocLength_ = 0.0;
}

bool SearchEngine::hasDocument(std::string_view docId) const noexcept {
    threading::shared_lock lock(indexMutex_);
    return documents_.contains(String(docId));
}

std::optional<SearchEngine::DocumentPtr> SearchEngine::getDocument(
    std::string_view docId) const noexcept {
    threading::shared_lock lock(indexMutex_);
    auto it = documents_.find(String(docId));
    if (it != documents_.end()) {
        return it->second;
    }
    return std::nullopt;
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

size_t SearchEngine::getDocumentCount() const noexcept {
    return totalDocs_.load(std::memory_order_acquire);
}

IndexStats SearchEngine::getStats() const noexcept {
    threading::shared_lock lock(indexMutex_);
    return IndexStats{
        .documentCount = totalDocs_.load(std::memory_order_acquire),
        .uniqueTerms = contentIndex_.size(),
        .uniqueTags = tagIndex_.size(),
        .totalTokens = totalTokens_.load(std::memory_order_acquire),
        .avgDocumentLength = avgDocLength_};
}

void SearchEngine::addContentToIndex(const DocumentPtr& doc) {
    String docId = String(doc->getId());
    auto tokens = tokenizer_.tokenize(doc->getContent());

    for (const auto& token : tokens) {
        contentIndex_[token].insert(docId);
        termFrequency_[token]++;
    }
}

void SearchEngine::removeContentFromIndex(const DocumentPtr& doc) {
    String docId = String(doc->getId());
    auto tokens = tokenizer_.tokenize(doc->getContent());

    for (const auto& token : tokens) {
        auto contentIt = contentIndex_.find(token);
        if (contentIt != contentIndex_.end()) {
            contentIt->second.erase(docId);
            if (contentIt->second.empty()) {
                contentIndex_.erase(contentIt);
            }
        }

        auto freqIt = termFrequency_.find(token);
        if (freqIt != termFrequency_.end()) {
            if (--(freqIt->second) <= 0) {
                termFrequency_.erase(freqIt);
            }
        }
    }
}

void SearchEngine::optimize() {
    std::unique_lock<threading::shared_mutex> lock(indexMutex_);

    std::erase_if(contentIndex_,
                  [](const auto& pair) { return pair.second.empty(); });

    std::erase_if(tagIndex_,
                  [](const auto& pair) { return pair.second.empty(); });

    std::erase_if(docFrequency_,
                  [](const auto& pair) { return pair.second <= 0; });

    std::erase_if(termFrequency_,
                  [](const auto& pair) { return pair.second <= 0; });
}

void SearchEngine::rebuildIndex() {
    std::unique_lock<threading::shared_mutex> lock(indexMutex_);

    auto docs = std::move(documents_);

    documents_.clear();
    tagIndex_.clear();
    contentIndex_.clear();
    docFrequency_.clear();
    termFrequency_.clear();
    totalDocs_.store(0, std::memory_order_release);
    totalTokens_.store(0, std::memory_order_release);
    avgDocLength_ = 0.0;

    lock.unlock();

    for (auto& [_, docPtr] : docs) {
        [[maybe_unused]] auto result = addDocument(*docPtr);
    }
}

SearchEngine::DocumentList SearchEngine::searchByTag(std::string_view tag,
                                                     SearchOptions options) {
    if (tag.empty()) {
        return {};
    }

    DocumentList results;

    threading::shared_lock lock(indexMutex_);

    auto it = tagIndex_.find(std::string(tag));
    if (it != tagIndex_.end()) {
        size_t maxSize = options.maxResults > 0
                             ? std::min(it->second.size(), options.maxResults)
                             : it->second.size();
        results.reserve(maxSize);

        for (const auto& docId : it->second) {
            if (options.maxResults > 0 &&
                results.size() >= options.maxResults) {
                break;
            }
            auto docIt = documents_.find(docId);
            if (docIt != documents_.end()) {
                results.push_back(docIt->second);
            }
        }
    }

    return results;
}

SearchEngine::DocumentList SearchEngine::fuzzySearchByTag(
    std::string_view tag, int tolerance, SearchOptions options) {
    if (tag.empty()) {
        return {};
    }

    if (tolerance < 0) {
        tolerance = 0;
    }

    DocumentList results;
    HashSet<String> processedDocIds;

    threading::shared_lock lock(indexMutex_);

    for (const auto& [key, docIds] : tagIndex_) {
        if (Scorer::levenshteinDistance(tag, key) <= tolerance) {
            for (const auto& docId : docIds) {
                if (options.maxResults > 0 &&
                    results.size() >= options.maxResults) {
                    break;
                }
                if (processedDocIds.insert(docId).second) {
                    auto docIt = documents_.find(docId);
                    if (docIt != documents_.end()) {
                        results.push_back(docIt->second);
                    }
                }
            }
        }
        if (options.maxResults > 0 && results.size() >= options.maxResults) {
            break;
        }
    }

    return results;
}

SearchEngine::DocumentList SearchEngine::searchByTags(
    std::span<const std::string> tags, SearchOptions options) {
    if (tags.empty()) {
        return {};
    }

    std::set<String> candidateDocIds;
    bool firstTag = true;

    threading::shared_lock lock(indexMutex_);

    for (const auto& tag : tags) {
        auto it = tagIndex_.find(tag);
        if (it == tagIndex_.end()) {
            return {};
        }

        std::set<String> tagDocIds(it->second.begin(), it->second.end());

        if (firstTag) {
            candidateDocIds = std::move(tagDocIds);
            firstTag = false;
        } else {
            std::set<String> intersection;
            std::set_intersection(
                candidateDocIds.begin(), candidateDocIds.end(),
                tagDocIds.begin(), tagDocIds.end(),
                std::inserter(intersection, intersection.begin()));
            candidateDocIds = std::move(intersection);
        }

        if (candidateDocIds.empty()) {
            return {};
        }
    }

    DocumentList results;
    results.reserve(std::min(
        candidateDocIds.size(),
        options.maxResults > 0 ? options.maxResults : candidateDocIds.size()));

    for (const auto& docId : candidateDocIds) {
        if (options.maxResults > 0 && results.size() >= options.maxResults) {
            break;
        }
        auto docIt = documents_.find(docId);
        if (docIt != documents_.end()) {
            results.push_back(docIt->second);
        }
    }

    return results;
}

SearchEngine::DocumentList SearchEngine::searchByAnyTag(
    std::span<const std::string> tags, SearchOptions options) {
    if (tags.empty()) {
        return {};
    }

    HashSet<String> candidateDocIds;

    threading::shared_lock lock(indexMutex_);

    for (const auto& tag : tags) {
        auto it = tagIndex_.find(tag);
        if (it != tagIndex_.end()) {
            for (const auto& docId : it->second) {
                candidateDocIds.insert(docId);
            }
        }
    }

    DocumentList results;
    results.reserve(std::min(
        candidateDocIds.size(),
        options.maxResults > 0 ? options.maxResults : candidateDocIds.size()));

    for (const auto& docId : candidateDocIds) {
        if (options.maxResults > 0 && results.size() >= options.maxResults) {
            break;
        }
        auto docIt = documents_.find(docId);
        if (docIt != documents_.end()) {
            results.push_back(docIt->second);
        }
    }

    return results;
}

void SearchEngine::searchByContentWorker(std::span<const String> wordChunk,
                                         HashMap<String, double>& scoresMap,
                                         threading::mutex& scoresMutex) {
    HashMap<String, double> localScores;
    IndexStats stats = getStats();
    Scorer scorer(stats);

    threading::shared_lock lock(indexMutex_);

    for (const auto& word : wordChunk) {
        auto it = contentIndex_.find(word);
        if (it != contentIndex_.end()) {
            size_t termFreq = 1;
            auto freqIt = termFrequency_.find(word);
            if (freqIt != termFrequency_.end()) {
                termFreq = static_cast<size_t>(freqIt->second);
            }

            for (const auto& docId : it->second) {
                auto docIt = documents_.find(docId);
                if (docIt != documents_.end()) {
                    localScores[docId] +=
                        scorer.bm25(*docIt->second, word, termFreq);
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

SearchEngine::ScoredDocumentList SearchEngine::searchByContent(
    std::string_view query, SearchOptions options) {
    if (query.empty()) {
        return {};
    }

    auto words = tokenizer_.tokenize(query);
    if (words.empty()) {
        return {};
    }

    HashMap<String, double> scores;
    threading::mutex scoresMutex;

    if (words.size() <= 2 || config_.maxThreads <= 1 ||
        !config_.enableParallelSearch) {
        searchByContentWorker(std::span<const String>(words), scores,
                              scoresMutex);
    } else {
        std::vector<threading::future<void>> futures;
        size_t numWords = words.size();
        size_t chunkSize = std::max(size_t(1), numWords / config_.maxThreads);

        for (size_t i = 0; i < numWords; i += chunkSize) {
            size_t end = std::min(i + chunkSize, numWords);
            std::vector<String> wordChunk(words.begin() + i,
                                          words.begin() + end);

            futures.push_back(std::async(
                std::launch::async, [this, wordChunk, &scores, &scoresMutex]() {
                    searchByContentWorker(std::span<const String>(wordChunk),
                                          scores, scoresMutex);
                }));
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

    return getRankedResults(scores, options);
}

SearchEngine::DocumentList SearchEngine::booleanSearch(std::string_view query,
                                                       SearchOptions options) {
    if (query.empty()) {
        return {};
    }

    std::string queryStr(query);
    std::istringstream iss{queryStr};
    std::string token;
    std::vector<std::string> terms;
    std::vector<std::string> operators;
    bool isNot = false;

    while (iss >> token) {
        if (token == "NOT") {
            isNot = true;
            continue;
        }

        if (token == "AND" || token == "OR") {
            operators.push_back(token);
            continue;
        }

        std::transform(token.begin(), token.end(), token.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        token = std::regex_replace(token, std::regex("[^a-zA-Z0-9]"), "");

        if (!token.empty()) {
            if (isNot) {
                terms.push_back("NOT_" + token);
                isNot = false;
            } else {
                terms.push_back(token);
            }
        }
    }

    if (terms.empty()) {
        return {};
    }

    threading::shared_lock lock(indexMutex_);

    std::set<String> candidateDocIds;
    bool firstTerm = true;

    for (size_t i = 0; i < terms.size(); ++i) {
        std::set<String> termDocIds;
        bool isNotTerm = terms[i].substr(0, 4) == "NOT_";
        std::string actualTerm = isNotTerm ? terms[i].substr(4) : terms[i];

        String wordKey(actualTerm);
        auto it = contentIndex_.find(wordKey);

        if (it != contentIndex_.end()) {
            for (const auto& docId : it->second) {
                termDocIds.insert(docId);
            }
        }

        if (isNotTerm) {
            std::set<String> allDocIds;
            for (const auto& doc : documents_) {
                allDocIds.insert(doc.first);
            }
            std::set<String> notTermDocIds;
            std::set_difference(
                allDocIds.begin(), allDocIds.end(), termDocIds.begin(),
                termDocIds.end(),
                std::inserter(notTermDocIds, notTermDocIds.begin()));
            termDocIds = notTermDocIds;
        }

        if (firstTerm) {
            candidateDocIds = termDocIds;
            firstTerm = false;
        } else {
            std::string op =
                (i - 1 < operators.size()) ? operators[i - 1] : "AND";

            if (op == "AND") {
                std::set<String> intersection;
                std::set_intersection(
                    candidateDocIds.begin(), candidateDocIds.end(),
                    termDocIds.begin(), termDocIds.end(),
                    std::inserter(intersection, intersection.begin()));
                candidateDocIds = intersection;
            } else if (op == "OR") {
                std::set<String> unionSet;
                std::set_union(candidateDocIds.begin(), candidateDocIds.end(),
                               termDocIds.begin(), termDocIds.end(),
                               std::inserter(unionSet, unionSet.begin()));
                candidateDocIds = unionSet;
            }
        }
    }

    DocumentList results;
    results.reserve(std::min(
        candidateDocIds.size(),
        options.maxResults > 0 ? options.maxResults : candidateDocIds.size()));

    for (const auto& docId : candidateDocIds) {
        if (options.maxResults > 0 && results.size() >= options.maxResults) {
            break;
        }
        auto docIt = documents_.find(docId);
        if (docIt != documents_.end()) {
            results.push_back(docIt->second);
        }
    }

    return results;
}

SearchEngine::DocumentList SearchEngine::phraseSearch(std::string_view phrase,
                                                      SearchOptions options) {
    if (phrase.empty()) {
        return {};
    }

    DocumentList results;
    std::string phraseStr(phrase);

    std::transform(phraseStr.begin(), phraseStr.end(), phraseStr.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    threading::shared_lock lock(indexMutex_);

    for (const auto& [docId, doc] : documents_) {
        if (options.maxResults > 0 && results.size() >= options.maxResults) {
            break;
        }

        std::string content(doc->getContent());
        std::transform(content.begin(), content.end(), content.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (content.find(phraseStr) != std::string::npos) {
            results.push_back(doc);
        }
    }

    return results;
}

std::vector<String> SearchEngine::autoComplete(std::string_view prefix,
                                               size_t maxResults) {
    if (prefix.empty()) {
        return {};
    }

    if (maxResults == 0) {
        maxResults = 10;
    }

    std::vector<String> suggestions;
    suggestions.reserve(maxResults);

    std::string prefixStd(prefix);
    std::transform(prefixStd.begin(), prefixStd.end(), prefixStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    size_t prefixLen = prefixStd.length();

    threading::shared_lock lock(indexMutex_);

    for (const auto& [tag, _] : tagIndex_) {
        if (tag.size() >= prefixLen) {
            std::string tagLower = tag;
            std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (tagLower.rfind(prefixStd, 0) == 0) {
                suggestions.push_back(String(tag));
            }
        }
        if (suggestions.size() >= maxResults)
            break;
    }

    if (suggestions.size() < maxResults) {
        for (const auto& [word, _] : contentIndex_) {
            if (word.size() >= prefixLen) {
                std::string wordStd(word);
                std::transform(wordStd.begin(), wordStd.end(), wordStd.begin(),
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
                    }
                }
            }
            if (suggestions.size() >= maxResults)
                break;
        }
    }

    std::sort(suggestions.begin(), suggestions.end(),
              [this](const String& a, const String& b) {
                  int freqA = 0, freqB = 0;
                  auto itA = docFrequency_.find(a);
                  auto itB = docFrequency_.find(b);
                  if (itA != docFrequency_.end())
                      freqA = itA->second;
                  if (itB != docFrequency_.end())
                      freqB = itB->second;
                  return freqA > freqB;
              });

    if (suggestions.size() > maxResults) {
        suggestions.resize(maxResults);
    }

    return suggestions;
}

SearchEngine::ScoredDocumentList SearchEngine::getRankedResults(
    const HashMap<String, double>& scores, const SearchOptions& options) const {
    struct ScoredDoc {
        DocumentPtr doc;
        double score;
        bool operator<(const ScoredDoc& other) const {
            return score < other.score;
        }
    };

    std::priority_queue<ScoredDoc> priorityQueue;
    threading::shared_lock lock(indexMutex_);

    for (const auto& [docId, score] : scores) {
        if (score <= options.minScore)
            continue;

        auto it = documents_.find(docId);
        if (it != documents_.end()) {
            priorityQueue.push({it->second, score});
        }
    }
    lock.unlock();

    ScoredDocumentList results;
    size_t maxResults =
        options.maxResults > 0 ? options.maxResults : priorityQueue.size();
    results.reserve(std::min(maxResults, priorityQueue.size()));

    while (!priorityQueue.empty() && results.size() < maxResults) {
        auto& top = priorityQueue.top();
        results.push_back(ScoredDocument{
            .document = top.doc, .score = top.score, .matchedTerms = {}});
        priorityQueue.pop();
    }

    return results;
}

SearchResult<void> SearchEngine::saveIndex(std::string_view filename) const {
    if (filename.empty()) {
        return std::unexpected(SearchErrorCode::InvalidQuery);
    }

    try {
        threading::shared_lock lock(indexMutex_);
        std::ofstream ofs(std::string(filename), std::ios::binary);
        if (!ofs) {
            return std::unexpected(SearchErrorCode::IOError);
        }

        constexpr uint32_t VERSION = 2;
        ofs.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));

        size_t totalDocsValue = totalDocs_.load(std::memory_order_acquire);
        ofs.write(reinterpret_cast<const char*>(&totalDocsValue),
                  sizeof(totalDocsValue));

        size_t docSize = documents_.size();
        ofs.write(reinterpret_cast<const char*>(&docSize), sizeof(docSize));

        for (const auto& [docId, doc] : documents_) {
            std::string docIdStd(docId);
            size_t idLength = docIdStd.size();
            ofs.write(reinterpret_cast<const char*>(&idLength),
                      sizeof(idLength));
            ofs.write(docIdStd.c_str(), static_cast<std::streamsize>(idLength));

            std::string contentStd(doc->getContent());
            size_t contentLength = contentStd.size();
            ofs.write(reinterpret_cast<const char*>(&contentLength),
                      sizeof(contentLength));
            ofs.write(contentStd.c_str(),
                      static_cast<std::streamsize>(contentLength));

            const auto& tags = doc->getTags();
            size_t tagsCount = tags.size();
            ofs.write(reinterpret_cast<const char*>(&tagsCount),
                      sizeof(tagsCount));

            for (const auto& tag : tags) {
                size_t tagLength = tag.size();
                ofs.write(reinterpret_cast<const char*>(&tagLength),
                          sizeof(tagLength));
                ofs.write(tag.c_str(), static_cast<std::streamsize>(tagLength));
            }

            int clickCount = doc->getClickCount();
            ofs.write(reinterpret_cast<const char*>(&clickCount),
                      sizeof(clickCount));
        }

        return {};
    } catch (const std::exception& e) {
        spdlog::error("Error saving index: {}", e.what());
        return std::unexpected(SearchErrorCode::IOError);
    }
}

SearchResult<void> SearchEngine::loadIndex(std::string_view filename) {
    if (filename.empty()) {
        return std::unexpected(SearchErrorCode::InvalidQuery);
    }

    try {
        std::unique_lock<threading::shared_mutex> lock(indexMutex_);
        std::ifstream ifs(std::string(filename), std::ios::binary);
        if (!ifs) {
            return std::unexpected(SearchErrorCode::IOError);
        }

        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        termFrequency_.clear();
        totalDocs_.store(0, std::memory_order_release);
        totalTokens_.store(0, std::memory_order_release);
        avgDocLength_ = 0.0;

        uint32_t version = 1;
        if (!ifs.read(reinterpret_cast<char*>(&version), sizeof(version))) {
            if (ifs.eof())
                return {};
            return std::unexpected(SearchErrorCode::IndexCorrupted);
        }

        size_t totalDocsValue;
        if (version >= 2) {
            if (!ifs.read(reinterpret_cast<char*>(&totalDocsValue),
                          sizeof(totalDocsValue))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }
        } else {
            totalDocsValue = static_cast<size_t>(version);
        }

        size_t docSize;
        if (!ifs.read(reinterpret_cast<char*>(&docSize), sizeof(docSize))) {
            if (ifs.eof() && totalDocsValue == 0)
                return {};
            return std::unexpected(SearchErrorCode::IndexCorrupted);
        }

        size_t totalTokenCount = 0;

        for (size_t i = 0; i < docSize; ++i) {
            size_t idLength;
            if (!ifs.read(reinterpret_cast<char*>(&idLength),
                          sizeof(idLength))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }
            std::string docIdStd(idLength, '\0');
            if (!ifs.read(docIdStd.data(),
                          static_cast<std::streamsize>(idLength))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }
            String docId(docIdStd);

            size_t contentLength;
            if (!ifs.read(reinterpret_cast<char*>(&contentLength),
                          sizeof(contentLength))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }
            std::string contentStd(contentLength, '\0');
            if (!ifs.read(contentStd.data(),
                          static_cast<std::streamsize>(contentLength))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }
            String content(contentStd);

            std::vector<std::string> tagsVec;
            size_t tagsCount;
            if (!ifs.read(reinterpret_cast<char*>(&tagsCount),
                          sizeof(tagsCount))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }

            tagsVec.reserve(tagsCount);
            for (size_t j = 0; j < tagsCount; ++j) {
                size_t tagLength;
                if (!ifs.read(reinterpret_cast<char*>(&tagLength),
                              sizeof(tagLength))) {
                    return std::unexpected(SearchErrorCode::IndexCorrupted);
                }
                std::string tagStd(tagLength, '\0');
                if (!ifs.read(tagStd.data(),
                              static_cast<std::streamsize>(tagLength))) {
                    return std::unexpected(SearchErrorCode::IndexCorrupted);
                }
                tagsVec.push_back(std::move(tagStd));
            }

            int clickCount;
            if (!ifs.read(reinterpret_cast<char*>(&clickCount),
                          sizeof(clickCount))) {
                return std::unexpected(SearchErrorCode::IndexCorrupted);
            }

            auto doc = std::make_shared<Document>(
                docId, content, std::span<const std::string>(tagsVec));
            doc->setClickCount(clickCount);

            documents_[docId] = doc;

            for (const auto& tag : tagsVec) {
                tagIndex_[tag].push_back(docId);
                docFrequency_[String(tag)]++;
            }

            auto tokens = tokenizer_.tokenize(content);
            totalTokenCount += tokens.size();
            for (const auto& token : tokens) {
                contentIndex_[token].insert(docId);
                termFrequency_[token]++;
            }
        }

        totalDocs_.store(documents_.size(), std::memory_order_release);
        totalTokens_.store(totalTokenCount, std::memory_order_release);
        if (!documents_.empty()) {
            avgDocLength_ = static_cast<double>(totalTokenCount) /
                            static_cast<double>(documents_.size());
        }

        return {};
    } catch (const std::exception& e) {
        spdlog::error("Error loading index: {}", e.what());
        documents_.clear();
        tagIndex_.clear();
        contentIndex_.clear();
        docFrequency_.clear();
        termFrequency_.clear();
        totalDocs_.store(0, std::memory_order_release);
        totalTokens_.store(0, std::memory_order_release);
        avgDocLength_ = 0.0;
        return std::unexpected(SearchErrorCode::IOError);
    }
}

int SearchEngine::levenshteinDistance(std::string_view s1,
                                      std::string_view s2) const noexcept {
    return Scorer::levenshteinDistance(s1, s2);
}

}  // namespace atom::search
