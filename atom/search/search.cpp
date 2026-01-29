/**
 * @file search.cpp
 * @brief Implements the Document and SearchEngine classes for Atom Search.
 * @date 2025-07-16
 */

#include "search.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <queue>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace atom::search {

// Document Implementation
Document::Document(String id, String content,
                   std::initializer_list<std::string> tags)
    : id_(std::move(id)),
      content_(std::move(content)),
      tags_(tags.begin(), tags.end()) {
    validate();
    spdlog::info("Document created with id: {}", std::string(id_));
}

Document::Document(String id, String content,
                   const std::vector<std::string>& tags)
    : id_(std::move(id)),
      content_(std::move(content)),
      tags_{tags.begin(), tags.end()} {
    validate();
    spdlog::info("Document created with id: {}", std::string(id_));
}

Document::Document(const Document& other)
    : id_(other.id_),
      content_(other.content_),
      tags_(other.tags_),
      click_count_(other.click_count_.load(std::memory_order_relaxed)) {}

Document& Document::operator=(const Document& other) {
    if (this != &other) {
        id_ = other.id_;
        content_ = other.content_;
        tags_ = other.tags_;
        click_count_.store(other.click_count_.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
    }
    return *this;
}

Document::Document(Document&& other) noexcept
    : id_(std::move(other.id_)),
      content_(std::move(other.content_)),
      tags_(std::move(other.tags_)),
      click_count_(other.click_count_.load(std::memory_order_relaxed)) {}

Document& Document::operator=(Document&& other) noexcept {
    if (this != &other) {
        id_ = std::move(other.id_);
        content_ = std::move(other.content_);
        tags_ = std::move(other.tags_);
        click_count_.store(other.click_count_.load(std::memory_order_relaxed),
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

void Document::set_content(String content) {
    if (content.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }
    content_ = std::move(content);
}

void Document::add_tag(const std::string& tag) {
    if (tag.empty()) {
        throw DocumentValidationException("Tag cannot be empty");
    }
    if (tag.length() > 100) {
        throw DocumentValidationException("Tag too long (max 100 chars): " +
                                          tag);
    }
    tags_.insert(tag);
}

void Document::remove_tag(const std::string& tag) { tags_.erase(tag); }

// SearchEngine Implementation
SearchEngine::SearchEngine(unsigned num_threads, SearchConfig config)
    : num_threads_(num_threads > 0 ? num_threads
                                   : std::thread::hardware_concurrency()),
      shard_mask_([this] {
          size_t shard_count = num_threads_;
          if (shard_count == 0)
              shard_count = 1;
          size_t power = 1;
          while (power < shard_count)
              power <<= 1;
          return power - 1;
      }()),
      config_(std::move(config)) {
    shards_.resize(shard_mask_ + 1);
    for (auto& shard : shards_) {
        shard = std::make_unique<Shard>();
    }
    task_queue_ = std::make_unique<ConcurrentQueue<SearchTask>>();
    start_worker_threads();
    spdlog::info(
        "SearchEngine initialized with {} shards, {} worker threads, cache "
        "size: {}.",
        shards_.size(), num_threads_, config_.cache_size);
}

SearchEngine::~SearchEngine() {
    spdlog::info("Shutting down SearchEngine.");
    stop_worker_threads();
}

SearchEngine::Shard& SearchEngine::get_shard(const String& key) const {
    return *shards_[std::hash<String>{}(key)&shard_mask_];
}

void SearchEngine::add_document(const Document& doc) {
    add_document(Document(doc));
}

void SearchEngine::add_document(Document&& doc) {
    doc.validate();
    auto doc_ptr = std::make_shared<Document>(std::move(doc));
    String doc_id = String(doc_ptr->get_id());
    auto& shard = get_shard(doc_id);

    std::unique_lock lock(shard.mutex);
    if (shard.documents.count(doc_id)) {
        throw std::invalid_argument("Document with this ID already exists");
    }

    shard.documents[doc_id] = doc_ptr;
    for (const auto& tag : doc_ptr->get_tags()) {
        auto& tag_shard = get_shard(tag);
        std::unique_lock tag_lock(tag_shard.mutex);
        tag_shard.tag_index[tag].push_back(doc_id);
        tag_shard.doc_frequency[String(tag)]++;
    }

    add_content_to_index(shard, doc_ptr);
    total_docs_++;
    spdlog::info("Added document: {}. Total docs: {}", std::string(doc_id),
                 total_docs_.load());
}

void SearchEngine::remove_document(const String& doc_id) {
    auto& shard = get_shard(doc_id);
    std::unique_lock lock(shard.mutex);

    auto it = shard.documents.find(doc_id);
    if (it == shard.documents.end()) {
        throw DocumentNotFoundException(doc_id);
    }

    auto doc_ptr = it->second;

    for (const auto& tag : doc_ptr->get_tags()) {
        auto& tag_shard = get_shard(tag);
        std::unique_lock tag_lock(tag_shard.mutex);
        auto& doc_ids = tag_shard.tag_index[tag];
        doc_ids.erase(std::remove(doc_ids.begin(), doc_ids.end(), doc_id),
                      doc_ids.end());
        if (doc_ids.empty()) {
            tag_shard.tag_index.erase(tag);
        }
        tag_shard.doc_frequency[String(tag)]--;
    }

    remove_content_from_index(shard, doc_ptr);

    // Invalidate caches for this document
    invalidate_content_cache(doc_id);

    shard.documents.erase(it);
    total_docs_--;
    spdlog::info("Removed document: {}. Total docs: {}", std::string(doc_id),
                 total_docs_.load());
}

void SearchEngine::update_document(const Document& doc) {
    doc.validate();
    String doc_id = String(doc.get_id());
    remove_document(doc_id);
    add_document(doc);
    spdlog::info("Updated document: {}", std::string(doc_id));
}

std::vector<std::shared_ptr<Document>> SearchEngine::search_by_tag(
    const std::string& tag) {
    auto& shard = get_shard(tag);
    std::shared_lock lock(shard.mutex);

    auto it = shard.tag_index.find(tag);
    if (it == shard.tag_index.end()) {
        return {};
    }

    std::vector<std::shared_ptr<Document>> results;
    results.reserve(it->second.size());
    for (const auto& doc_id : it->second) {
        auto& doc_shard = get_shard(doc_id);
        std::shared_lock doc_lock(doc_shard.mutex);
        auto doc_it = doc_shard.documents.find(doc_id);
        if (doc_it != doc_shard.documents.end()) {
            results.push_back(doc_it->second);
        }
    }
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::fuzzy_search_by_tag(
    const std::string& tag, int tolerance) {
    if (tolerance < 0) {
        throw std::invalid_argument("Tolerance cannot be negative");
    }

    std::vector<std::future<std::vector<String>>> futures;
    for (const auto& shard_ptr : shards_) {
        futures.push_back(std::async(std::launch::async, [&tag, tolerance, this,
                                                          &shard_ptr] {
            std::vector<String> matched_doc_ids;
            std::shared_lock lock(shard_ptr->mutex);
            for (const auto& [current_tag, doc_ids] : shard_ptr->tag_index) {
                if (levenshtein_distance(tag, current_tag) <= tolerance) {
                    matched_doc_ids.insert(matched_doc_ids.end(),
                                           doc_ids.begin(), doc_ids.end());
                }
            }
            return matched_doc_ids;
        }));
    }

    std::vector<std::shared_ptr<Document>> results;
    HashSet<String> processed_doc_ids;
    for (auto& future : futures) {
        for (const auto& doc_id : future.get()) {
            if (processed_doc_ids.insert(doc_id).second) {
                auto& doc_shard = get_shard(doc_id);
                std::shared_lock doc_lock(doc_shard.mutex);
                auto doc_it = doc_shard.documents.find(doc_id);
                if (doc_it != doc_shard.documents.end()) {
                    results.push_back(doc_it->second);
                }
            }
        }
    }
    return results;
}

std::vector<std::shared_ptr<Document>> SearchEngine::search_by_tags(
    const std::vector<std::string>& tags) {
    HashMap<String, double> scores;
    for (const auto& tag : tags) {
        auto& shard = get_shard(tag);
        std::shared_lock lock(shard.mutex);
        auto it = shard.tag_index.find(tag);
        if (it != shard.tag_index.end()) {
            for (const auto& doc_id : it->second) {
                auto& doc_shard = get_shard(doc_id);
                std::shared_lock doc_lock(doc_shard.mutex);
                auto doc_it = doc_shard.documents.find(doc_id);
                if (doc_it != doc_shard.documents.end()) {
                    scores[doc_id] += tf_idf(*doc_it->second, tag);
                }
            }
        }
    }
    return get_ranked_results(scores);
}

std::vector<std::shared_ptr<Document>> SearchEngine::search_by_content(
    const String& query) {
    auto tokens = tokenize_content(query);
    if (tokens.empty()) {
        return {};
    }

    std::vector<std::future<HashMap<String, double>>> futures;
    for (const auto& shard_ptr : shards_) {
        futures.push_back(
            std::async(std::launch::async, [&tokens, this, &shard_ptr] {
                HashMap<String, double> local_scores;
                std::shared_lock lock(shard_ptr->mutex);
                for (const auto& token : tokens) {
                    auto it = shard_ptr->content_index.find(token);
                    if (it != shard_ptr->content_index.end()) {
                        for (const auto& doc_id : it->second) {
                            auto doc_it = shard_ptr->documents.find(doc_id);
                            if (doc_it != shard_ptr->documents.end()) {
                                local_scores[doc_id] += tf_idf(
                                    *doc_it->second, std::string_view(token));
                            }
                        }
                    }
                }
                return local_scores;
            }));
    }

    HashMap<String, double> total_scores;
    for (auto& future : futures) {
        for (const auto& [doc_id, score] : future.get()) {
            total_scores[doc_id] += score;
        }
    }

    return get_ranked_results(total_scores);
}

std::vector<std::shared_ptr<Document>> SearchEngine::boolean_search(
    const String& query) {
    // This is a simplified implementation. A full boolean search would require
    // a proper parser for boolean expressions.
    auto tokens = tokenize_content(query);
    if (tokens.empty()) {
        return {};
    }

    HashSet<String> matching_docs;
    bool first = true;
    for (const auto& token : tokens) {
        HashSet<String> current_docs;
        auto& shard = get_shard(token);
        std::shared_lock lock(shard.mutex);
        auto it = shard.content_index.find(token);
        if (it != shard.content_index.end()) {
            current_docs.insert(it->second.begin(), it->second.end());
        }
        lock.unlock();

        if (first) {
            matching_docs = std::move(current_docs);
            first = false;
        } else {
            // Perform AND operation
            Vector<String> to_remove;
            for (const auto& doc_id : matching_docs) {
                if (current_docs.find(doc_id) == current_docs.end()) {
                    to_remove.push_back(doc_id);
                }
            }
            for (const auto& doc_id : to_remove) {
                matching_docs.erase(doc_id);
            }
        }
    }

    std::vector<std::shared_ptr<Document>> results;
    for (const auto& doc_id : matching_docs) {
        auto& shard = get_shard(doc_id);
        std::shared_lock lock(shard.mutex);
        auto it = shard.documents.find(doc_id);
        if (it != shard.documents.end()) {
            results.push_back(it->second);
        }
    }
    return results;
}

std::vector<String> SearchEngine::auto_complete(const String& prefix,
                                                size_t max_results) {
    if (prefix.empty()) {
        return {};
    }

    std::string prefix_lower = std::string(prefix);
    std::transform(prefix_lower.begin(), prefix_lower.end(),
                   prefix_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::vector<std::future<std::vector<String>>> futures;
    for (const auto& shard_ptr : shards_) {
        futures.push_back(
            std::async(std::launch::async, [&prefix_lower, &shard_ptr] {
                std::vector<String> suggestions;
                std::shared_lock lock(shard_ptr->mutex);
                for (const auto& [tag, _] : shard_ptr->tag_index) {
                    std::string tag_lower = tag;
                    std::transform(
                        tag_lower.begin(), tag_lower.end(), tag_lower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (tag_lower.rfind(prefix_lower, 0) == 0) {
                        suggestions.push_back(String(tag));
                    }
                }
                return suggestions;
            }));
    }

    std::vector<String> all_suggestions;
    for (auto& future : futures) {
        auto suggestions = future.get();
        all_suggestions.insert(all_suggestions.end(), suggestions.begin(),
                               suggestions.end());
    }

    std::sort(all_suggestions.begin(), all_suggestions.end());
    all_suggestions.erase(
        std::unique(all_suggestions.begin(), all_suggestions.end()),
        all_suggestions.end());

    if (all_suggestions.size() > max_results) {
        all_suggestions.resize(max_results);
    }

    return all_suggestions;
}

void SearchEngine::save_index(const String& filename) const {
    std::ofstream ofs(std::string(filename), std::ios::binary);
    if (!ofs) {
        throw std::ios_base::failure("Failed to open file for writing: " +
                                     std::string(filename));
    }

    size_t total_docs = total_docs_.load();
    ofs.write(reinterpret_cast<const char*>(&total_docs), sizeof(total_docs));

    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        size_t num_docs = shard_ptr->documents.size();
        ofs.write(reinterpret_cast<const char*>(&num_docs), sizeof(num_docs));
        for (const auto& [doc_id, doc] : shard_ptr->documents) {
            std::string doc_id_str = std::string(doc_id);
            size_t len = doc_id_str.size();
            ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
            ofs.write(doc_id_str.c_str(), len);

            std::string content_str = std::string(doc->get_content());
            len = content_str.size();
            ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
            ofs.write(content_str.c_str(), len);

            const auto& tags = doc->get_tags();
            size_t num_tags = tags.size();
            ofs.write(reinterpret_cast<const char*>(&num_tags),
                      sizeof(num_tags));
            for (const auto& tag : tags) {
                len = tag.size();
                ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
                ofs.write(tag.c_str(), len);
            }
            int click_count = doc->get_click_count();
            ofs.write(reinterpret_cast<const char*>(&click_count),
                      sizeof(click_count));
        }
    }
}

void SearchEngine::load_index(const String& filename) {
    std::ifstream ifs(std::string(filename), std::ios::binary);
    if (!ifs) {
        throw std::ios_base::failure("Failed to open file for reading: " +
                                     std::string(filename));
    }

    clear();

    size_t total_docs = 0;
    ifs.read(reinterpret_cast<char*>(&total_docs), sizeof(total_docs));

    for (const auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        size_t num_docs = 0;
        ifs.read(reinterpret_cast<char*>(&num_docs), sizeof(num_docs));
        for (size_t i = 0; i < num_docs; ++i) {
            size_t len;
            ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
            std::string doc_id_str(len, '\0');
            ifs.read(&doc_id_str[0], len);

            ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
            std::string content_str(len, '\0');
            ifs.read(&content_str[0], len);

            size_t num_tags;
            ifs.read(reinterpret_cast<char*>(&num_tags), sizeof(num_tags));
            std::set<std::string> tags;
            for (size_t j = 0; j < num_tags; ++j) {
                ifs.read(reinterpret_cast<char*>(&len), sizeof(len));
                std::string tag(len, '\0');
                ifs.read(&tag[0], len);
                tags.insert(tag);
            }

            int click_count;
            ifs.read(reinterpret_cast<char*>(&click_count),
                     sizeof(click_count));

            auto doc = std::make_shared<Document>(
                String(doc_id_str), String(content_str),
                std::initializer_list<std::string>{});
            for (const auto& tag : tags) {
                doc->add_tag(tag);
            }
            doc->set_click_count(click_count);

            add_document(std::move(*doc));
        }
    }
    total_docs_ = total_docs;
}

void SearchEngine::clear() {
    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);
        shard_ptr->documents.clear();
        shard_ptr->tag_index.clear();
        shard_ptr->content_index.clear();
        shard_ptr->doc_frequency.clear();
    }
    total_docs_ = 0;
    spdlog::info("Cleared all search engine data.");
}

bool SearchEngine::has_document(const String& doc_id) const {
    auto& shard = get_shard(doc_id);
    std::shared_lock lock(shard.mutex);
    return shard.documents.count(doc_id) > 0;
}

std::vector<String> SearchEngine::get_all_document_ids() const {
    std::vector<String> all_ids;
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& [id, _] : shard_ptr->documents) {
            all_ids.push_back(id);
        }
    }
    return all_ids;
}

void SearchEngine::add_content_to_index(Shard& /* doc_shard */,
                                        const std::shared_ptr<Document>& doc) {
    auto tokens = tokenize_content(String(doc->get_content()));
    String doc_id = String(doc->get_id());
    for (const auto& token : tokens) {
        auto& token_shard = get_shard(token);
        std::unique_lock lock(token_shard.mutex);
        token_shard.content_index[token].insert(doc_id);
        token_shard.doc_frequency[token]++;
    }
}

void SearchEngine::remove_content_from_index(
    Shard& /* doc_shard */, const std::shared_ptr<Document>& doc) {
    auto tokens = tokenize_content(String(doc->get_content()));
    String doc_id = String(doc->get_id());
    for (const auto& token : tokens) {
        auto& token_shard = get_shard(token);
        std::unique_lock lock(token_shard.mutex);
        auto it = token_shard.content_index.find(token);
        if (it != token_shard.content_index.end()) {
            it->second.erase(doc_id);
            if (it->second.empty()) {
                token_shard.content_index.erase(it);
            }
        }
        token_shard.doc_frequency[token]--;
    }
}

std::vector<String> SearchEngine::tokenize_content(
    const String& content) const {
    return tokenize_content_optimized(content);
}

std::vector<String> SearchEngine::tokenize_content_optimized(
    const String& content) const {
    std::vector<String> tokens;
    tokens.reserve(50);  // Reserve space for typical document size

    std::string content_str = std::string(content);
    std::string token_std;
    token_std.reserve(32);  // Reserve space for typical token size

    // Use more efficient tokenization
    const char* start = content_str.c_str();
    const char* end = start + content_str.length();
    const char* current = start;

    while (current < end) {
        // Skip non-alphanumeric characters
        while (current < end && !std::isalnum(*current)) {
            ++current;
        }

        if (current >= end)
            break;

        // Extract alphanumeric token
        const char* token_start = current;
        while (current < end && std::isalnum(*current)) {
            ++current;
        }

        if (current > token_start) {
            token_std.assign(token_start, current);

            // Convert to lowercase in-place
            std::transform(token_std.begin(), token_std.end(),
                           token_std.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            tokens.emplace_back(String(token_std));
        }
    }

    return tokens;
}

std::vector<String> SearchEngine::get_cached_tokens(
    const String& doc_id, const String& content) const {
    auto& shard = get_shard(doc_id);

    // Check cache first
    {
        std::shared_lock cache_lock(shard.cache_mutex);
        auto cache_it = shard.tokenized_content_cache.find(doc_id);
        if (cache_it != shard.tokenized_content_cache.end()) {
            return cache_it->second;
        }
    }

    // Tokenize and cache
    auto tokens = tokenize_content_optimized(content);
    cache_tokenized_content(doc_id, tokens);

    return tokens;
}

void SearchEngine::cache_tokenized_content(
    const String& doc_id, const std::vector<String>& tokens) const {
    auto& shard = get_shard(doc_id);
    std::unique_lock cache_lock(shard.cache_mutex);

    shard.tokenized_content_cache[doc_id] = tokens;

    // Limit cache size to prevent memory bloat
    if (shard.tokenized_content_cache.size() > 5000) {
        // Remove oldest 20% of entries (simple cleanup)
        auto it = shard.tokenized_content_cache.begin();
        std::advance(it, shard.tokenized_content_cache.size() / 5);
        shard.tokenized_content_cache.erase(
            shard.tokenized_content_cache.begin(), it);
    }
}

void SearchEngine::invalidate_content_cache(const String& doc_id) const {
    auto& shard = get_shard(doc_id);
    std::unique_lock cache_lock(shard.cache_mutex);

    // Remove from tokenized content cache
    shard.tokenized_content_cache.erase(doc_id);

    // Remove related TF-IDF cache entries
    for (auto it = shard.tf_idf_cache.begin();
         it != shard.tf_idf_cache.end();) {
        if (it->first.first == doc_id) {
            it = shard.tf_idf_cache.erase(it);
        } else {
            ++it;
        }
    }
}

double SearchEngine::tf_idf(const Document& doc, std::string_view term) const {
    // Use cached version for better performance
    return tf_idf_cached(doc, term);
}

double SearchEngine::tf_idf_cached(const Document& doc,
                                   std::string_view term) const {
    String doc_id = String(doc.get_id());
    String term_str(term);

    // Check cache first
    auto cache_key = std::make_pair(doc_id, term_str);
    auto& shard = get_shard(doc_id);

    {
        std::shared_lock cache_lock(shard.cache_mutex);
        auto cache_it = shard.tf_idf_cache.find(cache_key);
        if (cache_it != shard.tf_idf_cache.end()) {
            shard.cache_hits++;
            return cache_it->second;
        }
    }

    shard.cache_misses++;

    // Get cached tokens or tokenize content
    auto tokens = get_cached_tokens(doc_id, String(doc.get_content()));
    size_t term_freq = 0;
    size_t total_terms = tokens.size();

    for (const auto& token : tokens) {
        if (token == term_str) {
            term_freq++;
        }
    }

    if (term_freq == 0 || total_terms == 0) {
        return 0.0;
    }

    // TF using log normalization: 1 + log(freq)
    double tf = 1.0 + std::log(static_cast<double>(term_freq));

    // IDF calculation
    auto& term_shard = get_shard(term_str);
    std::shared_lock lock(term_shard.mutex);
    auto it = term_shard.doc_frequency.find(term_str);
    int doc_freq = (it != term_shard.doc_frequency.end()) ? it->second : 0;
    lock.unlock();

    double idf =
        (doc_freq > 0)
            ? std::log(static_cast<double>(total_docs_.load()) / doc_freq)
            : 0;

    double result = tf * idf;

    // Cache the result
    {
        std::unique_lock cache_lock(shard.cache_mutex);
        shard.tf_idf_cache[cache_key] = result;

        // Limit cache size to prevent memory bloat
        if (shard.tf_idf_cache.size() > 10000) {
            // Remove oldest 20% of entries (simple cleanup)
            auto it = shard.tf_idf_cache.begin();
            std::advance(it, shard.tf_idf_cache.size() / 5);
            shard.tf_idf_cache.erase(shard.tf_idf_cache.begin(), it);
        }
    }

    return result;
}

std::vector<std::shared_ptr<Document>> SearchEngine::get_ranked_results(
    const HashMap<String, double>& scores) const {
    std::vector<std::pair<double, String>> sorted_scores;
    for (const auto& [doc_id, score] : scores) {
        sorted_scores.emplace_back(score, doc_id);
    }

    std::sort(sorted_scores.rbegin(), sorted_scores.rend());

    std::vector<std::shared_ptr<Document>> results;
    for (const auto& [score, doc_id] : sorted_scores) {
        auto& shard = get_shard(doc_id);
        std::shared_lock lock(shard.mutex);
        auto it = shard.documents.find(doc_id);
        if (it != shard.documents.end()) {
            results.push_back(it->second);
        }
    }
    return results;
}

int SearchEngine::levenshtein_distance(std::string_view s1,
                                       std::string_view s2) const noexcept {
    const size_t m = s1.length();
    const size_t n = s2.length();

    if (m == 0)
        return static_cast<int>(n);
    if (n == 0)
        return static_cast<int>(m);

    std::vector<int> prev_row(n + 1);
    std::vector<int> curr_row(n + 1);

    for (size_t j = 0; j <= n; ++j) {
        prev_row[j] = static_cast<int>(j);
    }

    for (size_t i = 0; i < m; ++i) {
        curr_row[0] = static_cast<int>(i + 1);
        for (size_t j = 0; j < n; ++j) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            curr_row[j + 1] = std::min(
                {prev_row[j + 1] + 1, curr_row[j] + 1, prev_row[j] + cost});
        }
        prev_row.swap(curr_row);
    }

    return prev_row[n];
}

void SearchEngine::start_worker_threads() {
    worker_threads_.reserve(num_threads_);
    for (unsigned i = 0; i < num_threads_; ++i) {
        worker_threads_.emplace_back([this] { worker_function(); });
    }
}

void SearchEngine::stop_worker_threads() {
    task_queue_->stop();
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void SearchEngine::worker_function() {
    while (!stop_workers_.load()) {
        SearchTask task;
        if (task_queue_->pop(task)) {
            try {
                task.callback(task.words);
            } catch (const std::exception& e) {
                spdlog::error("Error in worker thread: {}", e.what());
            }
        } else if (stop_workers_.load()) {
            break;
        }
    }
}

// Enhanced search methods implementation
SearchResults SearchEngine::search_by_tag_enhanced(
    const std::string& tag, const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key = generate_cache_key("tag:" + tag, pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    // Perform search
    auto documents = search_by_tag(tag);

    SearchResults results;
    results.total_count = documents.size();
    results.offset = pagination.offset;

    // Apply pagination
    size_t start_idx = std::min(pagination.offset, documents.size());
    size_t end_idx = std::min(start_idx + pagination.limit, documents.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = documents[i];
        result.score = 1.0;  // Tag matches have uniform score
        result.matched_terms = {tag};
        results.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

SearchResults SearchEngine::search_by_content_enhanced(
    const String& query, const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key =
        generate_cache_key("content:" + std::string(query), pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    // Perform search
    auto documents = search_by_content(query);
    auto query_terms = tokenize_content(query);
    std::vector<std::string> query_terms_str;
    for (const auto& term : query_terms) {
        query_terms_str.push_back(std::string(term));
    }

    SearchResults results;
    results.total_count = documents.size();
    results.offset = pagination.offset;

    // Apply pagination and enhance results
    size_t start_idx = std::min(pagination.offset, documents.size());
    size_t end_idx = std::min(start_idx + pagination.limit, documents.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = documents[i];

        // Calculate combined TF-IDF score
        double total_score = 0.0;
        for (const auto& term : query_terms_str) {
            total_score += tf_idf(*documents[i], term);
        }
        result.score = total_score;

        result.matched_terms =
            extract_matched_terms(*documents[i], query_terms_str);
        result.snippet = generate_snippet(*documents[i], query_terms_str);

        results.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

// Helper method implementations
std::string SearchEngine::generate_cache_key(
    const std::string& query, const SearchPagination& pagination) const {
    return query + "|offset:" + std::to_string(pagination.offset) +
           "|limit:" + std::to_string(pagination.limit);
}

bool SearchEngine::get_cached_result(const std::string& cache_key,
                                     SearchResults& result) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = result_cache_.find(cache_key);
    if (it != result_cache_.end()) {
        auto now = std::chrono::steady_clock::now();
        if (now - it->second.second < config_.cache_ttl) {
            result = it->second.first;
            result.from_cache = true;
            return true;
        } else {
            // Remove expired entry
            result_cache_.erase(it);
        }
    }
    return false;
}

void SearchEngine::cache_result(const std::string& cache_key,
                                const SearchResults& result) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // Clean up cache if it's too large
    if (result_cache_.size() >= config_.cache_size) {
        cleanup_expired_cache();

        // If still too large, remove oldest entries more efficiently
        if (result_cache_.size() >= config_.cache_size) {
            // Use a more efficient approach: remove 20% of entries
            size_t to_remove = result_cache_.size() / 5;

            // Create vector of entries with timestamps for sorting
            std::vector<
                std::pair<std::chrono::steady_clock::time_point, std::string>>
                entries;
            entries.reserve(result_cache_.size());

            for (const auto& [key, value] : result_cache_) {
                entries.emplace_back(value.second, key);
            }

            // Sort by timestamp (oldest first)
            std::partial_sort(entries.begin(), entries.begin() + to_remove,
                              entries.end());

            // Remove oldest entries
            for (size_t i = 0; i < to_remove; ++i) {
                result_cache_.erase(entries[i].second);
            }
        }
    }

    result_cache_[cache_key] = {result, std::chrono::steady_clock::now()};
}

void SearchEngine::clear_performance_caches() const {
    // Clear all performance caches
    for (auto& shard_ptr : shards_) {
        std::unique_lock cache_lock(shard_ptr->cache_mutex);
        shard_ptr->tokenized_content_cache.clear();
        shard_ptr->tf_idf_cache.clear();
        shard_ptr->cache_hits = 0;
        shard_ptr->cache_misses = 0;
    }

    // Clear result cache
    std::lock_guard<std::mutex> lock(cache_mutex_);
    result_cache_.clear();

    spdlog::info("Performance caches cleared");
}

void SearchEngine::cleanup_expired_cache() const {
    auto now = std::chrono::steady_clock::now();
    for (auto it = result_cache_.begin(); it != result_cache_.end();) {
        if (now - it->second.second >= config_.cache_ttl) {
            it = result_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string SearchEngine::generate_snippet(
    const Document& doc, const std::vector<std::string>& terms,
    size_t max_length) const {
    std::string content = std::string(doc.get_content());
    if (content.length() <= max_length) {
        return content;
    }

    // Find the first occurrence of any term
    size_t best_pos = 0;
    for (const auto& term : terms) {
        size_t pos = content.find(term);
        if (pos != std::string::npos) {
            best_pos = pos;
            break;
        }
    }

    // Extract snippet around the found term
    size_t start = (best_pos > max_length / 2) ? best_pos - max_length / 2 : 0;
    size_t length = std::min(max_length, content.length() - start);

    std::string snippet = content.substr(start, length);
    if (start > 0)
        snippet = "..." + snippet;
    if (start + length < content.length())
        snippet += "...";

    return snippet;
}

std::vector<std::string> SearchEngine::extract_matched_terms(
    const Document& doc, const std::vector<std::string>& query_terms) const {
    std::vector<std::string> matched;
    std::string content = std::string(doc.get_content());
    std::transform(content.begin(), content.end(), content.begin(), ::tolower);

    for (const auto& term : query_terms) {
        std::string term_lower = term;
        std::transform(term_lower.begin(), term_lower.end(), term_lower.begin(),
                       ::tolower);
        if (content.find(term_lower) != std::string::npos) {
            matched.push_back(term);
        }
    }

    return matched;
}

// Configuration and metrics methods
void SearchEngine::reset_metrics() noexcept {
    metrics_.total_searches = 0;
    metrics_.cache_hits = 0;
    metrics_.cache_misses = 0;
    metrics_.total_documents_indexed = 0;
    metrics_.total_search_time_ms = 0;
    spdlog::info("Search metrics reset");
}

void SearchEngine::update_config(const SearchConfig& config) {
    config_ = config;

    // Clear cache if size changed
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (result_cache_.size() > config_.cache_size) {
        result_cache_.clear();
    }

    spdlog::info("Search configuration updated");
}

void SearchEngine::optimize_index() {
    spdlog::info("Starting index optimization...");

    // Clean up expired cache entries
    cleanup_expired_cache();

    // Compact shards by removing empty entries and optimize caches
    size_t cleaned_entries = 0;
    size_t total_cache_hits = 0;
    size_t total_cache_misses = 0;

    for (auto& shard_ptr : shards_) {
        std::unique_lock lock(shard_ptr->mutex);

        // Clean up empty tag index entries
        for (auto it = shard_ptr->tag_index.begin();
             it != shard_ptr->tag_index.end();) {
            if (it->second.empty()) {
                it = shard_ptr->tag_index.erase(it);
                cleaned_entries++;
            } else {
                ++it;
            }
        }

        // Clean up empty content index entries
        for (auto it = shard_ptr->content_index.begin();
             it != shard_ptr->content_index.end();) {
            if (it->second.empty()) {
                it = shard_ptr->content_index.erase(it);
                cleaned_entries++;
            } else {
                ++it;
            }
        }

        // Collect cache statistics
        total_cache_hits += shard_ptr->cache_hits.load();
        total_cache_misses += shard_ptr->cache_misses.load();

        // Optimize performance caches if they're too large
        {
            std::unique_lock cache_lock(shard_ptr->cache_mutex);
            if (shard_ptr->tokenized_content_cache.size() > 3000) {
                // Remove oldest 30% of tokenized content cache entries
                auto it = shard_ptr->tokenized_content_cache.begin();
                std::advance(
                    it, shard_ptr->tokenized_content_cache.size() * 3 / 10);
                shard_ptr->tokenized_content_cache.erase(
                    shard_ptr->tokenized_content_cache.begin(), it);
            }

            if (shard_ptr->tf_idf_cache.size() > 8000) {
                // Remove oldest 30% of TF-IDF cache entries
                auto it = shard_ptr->tf_idf_cache.begin();
                std::advance(it, shard_ptr->tf_idf_cache.size() * 3 / 10);
                shard_ptr->tf_idf_cache.erase(shard_ptr->tf_idf_cache.begin(),
                                              it);
            }
        }
    }

    double cache_hit_ratio = (total_cache_hits + total_cache_misses > 0)
                                 ? static_cast<double>(total_cache_hits) /
                                       (total_cache_hits + total_cache_misses)
                                 : 0.0;

    spdlog::info(
        "Index optimization completed. Cleaned {} empty entries. Cache hit "
        "ratio: {:.2f}%",
        cleaned_entries, cache_hit_ratio * 100.0);
}

std::unordered_map<std::string, size_t> SearchEngine::get_index_stats() const {
    std::unordered_map<std::string, size_t> stats;

    size_t total_tag_entries = 0;
    size_t total_content_entries = 0;
    size_t total_doc_frequency_entries = 0;
    size_t total_tokenized_cache_entries = 0;
    size_t total_tf_idf_cache_entries = 0;
    size_t total_cache_hits = 0;
    size_t total_cache_misses = 0;

    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        total_tag_entries += shard_ptr->tag_index.size();
        total_content_entries += shard_ptr->content_index.size();
        total_doc_frequency_entries += shard_ptr->doc_frequency.size();

        // Cache statistics
        {
            std::shared_lock cache_lock(shard_ptr->cache_mutex);
            total_tokenized_cache_entries +=
                shard_ptr->tokenized_content_cache.size();
            total_tf_idf_cache_entries += shard_ptr->tf_idf_cache.size();
        }

        total_cache_hits += shard_ptr->cache_hits.load();
        total_cache_misses += shard_ptr->cache_misses.load();
    }

    stats["total_documents"] = total_docs_.load();
    stats["total_shards"] = shards_.size();
    stats["total_tag_entries"] = total_tag_entries;
    stats["total_content_entries"] = total_content_entries;
    stats["total_doc_frequency_entries"] = total_doc_frequency_entries;
    stats["result_cache_size"] = result_cache_.size();
    stats["result_cache_capacity"] = config_.cache_size;
    stats["tokenized_cache_entries"] = total_tokenized_cache_entries;
    stats["tf_idf_cache_entries"] = total_tf_idf_cache_entries;
    stats["performance_cache_hits"] = total_cache_hits;
    stats["performance_cache_misses"] = total_cache_misses;

    return stats;
}

// Additional enhanced search methods
SearchResults SearchEngine::fuzzy_search_by_tag_enhanced(
    const std::string& tag, int tolerance, const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key = generate_cache_key(
        "fuzzy:" + tag + "|tol:" + std::to_string(tolerance), pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    // Perform search
    auto documents = fuzzy_search_by_tag(tag, tolerance);

    SearchResults results;
    results.total_count = documents.size();
    results.offset = pagination.offset;

    // Apply pagination
    size_t start_idx = std::min(pagination.offset, documents.size());
    size_t end_idx = std::min(start_idx + pagination.limit, documents.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = documents[i];
        result.score = 1.0 / (1.0 + tolerance);  // Score based on tolerance
        result.matched_terms = {tag};
        results.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

SearchResults SearchEngine::search_by_tags_enhanced(
    const std::vector<std::string>& tags, const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Create cache key from tags
    std::string tags_str;
    for (const auto& tag : tags) {
        if (!tags_str.empty())
            tags_str += ",";
        tags_str += tag;
    }
    std::string cache_key = generate_cache_key("tags:" + tags_str, pagination);

    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    // Perform search
    auto documents = search_by_tags(tags);

    SearchResults results;
    results.total_count = documents.size();
    results.offset = pagination.offset;

    // Apply pagination
    size_t start_idx = std::min(pagination.offset, documents.size());
    size_t end_idx = std::min(start_idx + pagination.limit, documents.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = documents[i];

        // Calculate score based on tag matches
        double score = 0.0;
        for (const auto& tag : tags) {
            score += tf_idf(*documents[i], tag);
        }
        result.score = score;
        result.matched_terms = tags;

        results.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

SearchResults SearchEngine::boolean_search_enhanced(
    const String& query, const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key =
        generate_cache_key("boolean:" + std::string(query), pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    // Perform search (using existing boolean_search for now)
    auto documents = boolean_search(query);

    SearchResults results;
    results.total_count = documents.size();
    results.offset = pagination.offset;

    // Apply pagination
    size_t start_idx = std::min(pagination.offset, documents.size());
    size_t end_idx = std::min(start_idx + pagination.limit, documents.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = documents[i];
        result.score = 1.0;  // Boolean matches have uniform score
        results.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

SearchResults SearchEngine::phrase_search(const String& phrase,
                                          const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key =
        generate_cache_key("phrase:" + std::string(phrase), pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    SearchResults results;
    results.offset = pagination.offset;

    std::string phrase_str = std::string(phrase);
    std::transform(phrase_str.begin(), phrase_str.end(), phrase_str.begin(),
                   ::tolower);

    // Search through all documents for exact phrase match
    std::vector<std::shared_ptr<Document>> matching_docs;
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& [doc_id, doc] : shard_ptr->documents) {
            std::string content = std::string(doc->get_content());
            std::transform(content.begin(), content.end(), content.begin(),
                           ::tolower);
            if (content.find(phrase_str) != std::string::npos) {
                matching_docs.push_back(doc);
            }
        }
    }

    results.total_count = matching_docs.size();

    // Apply pagination
    size_t start_idx = std::min(pagination.offset, matching_docs.size());
    size_t end_idx =
        std::min(start_idx + pagination.limit, matching_docs.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = matching_docs[i];
        result.score = 1.0;  // Phrase matches have uniform score
        result.matched_terms = {phrase_str};
        result.snippet = generate_snippet(*matching_docs[i], {phrase_str});
        results.results.push_back(std::move(result));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

SearchResults SearchEngine::wildcard_search(
    const String& pattern, const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key =
        generate_cache_key("wildcard:" + std::string(pattern), pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    SearchResults results;
    results.offset = pagination.offset;

    // Convert wildcard pattern to regex
    std::string pattern_str = std::string(pattern);
    std::string regex_pattern;
    for (char c : pattern_str) {
        switch (c) {
            case '*':
                regex_pattern += ".*";
                break;
            case '?':
                regex_pattern += ".";
                break;
            case '.':
            case '^':
            case '$':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                regex_pattern += "\\";
                regex_pattern += c;
                break;
            default:
                regex_pattern += c;
                break;
        }
    }

    try {
        std::regex pattern_regex(regex_pattern, std::regex_constants::icase);
        std::vector<std::shared_ptr<Document>> matching_docs;

        // Search through all documents
        for (const auto& shard_ptr : shards_) {
            std::shared_lock lock(shard_ptr->mutex);
            for (const auto& [doc_id, doc] : shard_ptr->documents) {
                std::string content = std::string(doc->get_content());
                if (std::regex_search(content, pattern_regex)) {
                    matching_docs.push_back(doc);
                }
            }
        }

        results.total_count = matching_docs.size();

        // Apply pagination
        size_t start_idx = std::min(pagination.offset, matching_docs.size());
        size_t end_idx =
            std::min(start_idx + pagination.limit, matching_docs.size());

        for (size_t i = start_idx; i < end_idx; ++i) {
            SearchResult result;
            result.document = matching_docs[i];
            result.score = 1.0;
            result.matched_terms = {pattern_str};
            result.snippet = generate_snippet(*matching_docs[i], {pattern_str});
            results.results.push_back(std::move(result));
        }
    } catch (const std::regex_error& e) {
        spdlog::error("Invalid wildcard pattern: {}", e.what());
        throw SearchOperationException("Invalid wildcard pattern: " +
                                       std::string(e.what()));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

SearchResults SearchEngine::regex_search(const String& regex_pattern,
                                         const SearchPagination& pagination) {
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first
    std::string cache_key =
        generate_cache_key("regex:" + std::string(regex_pattern), pagination);
    SearchResults cached_result;
    if (get_cached_result(cache_key, cached_result)) {
        metrics_.cache_hits++;
        return cached_result;
    }
    metrics_.cache_misses++;

    SearchResults results;
    results.offset = pagination.offset;

    try {
        std::regex pattern(std::string(regex_pattern),
                           std::regex_constants::icase);
        std::vector<std::shared_ptr<Document>> matching_docs;

        // Search through all documents
        for (const auto& shard_ptr : shards_) {
            std::shared_lock lock(shard_ptr->mutex);
            for (const auto& [doc_id, doc] : shard_ptr->documents) {
                std::string content = std::string(doc->get_content());
                if (std::regex_search(content, pattern)) {
                    matching_docs.push_back(doc);
                }
            }
        }

        results.total_count = matching_docs.size();

        // Apply pagination
        size_t start_idx = std::min(pagination.offset, matching_docs.size());
        size_t end_idx =
            std::min(start_idx + pagination.limit, matching_docs.size());

        for (size_t i = start_idx; i < end_idx; ++i) {
            SearchResult result;
            result.document = matching_docs[i];
            result.score = 1.0;
            result.matched_terms = {std::string(regex_pattern)};
            result.snippet = generate_snippet(*matching_docs[i],
                                              {std::string(regex_pattern)});
            results.results.push_back(std::move(result));
        }
    } catch (const std::regex_error& e) {
        spdlog::error("Invalid regex pattern: {}", e.what());
        throw SearchOperationException("Invalid regex pattern: " +
                                       std::string(e.what()));
    }

    auto end_time = std::chrono::steady_clock::now();
    results.search_time_ms =
        std::chrono::duration<double, std::milli>(end_time - start_time)
            .count();

    // Cache result
    cache_result(cache_key, results);

    metrics_.total_searches++;
    metrics_.total_search_time_ms +=
        static_cast<uint64_t>(results.search_time_ms);

    return results;
}

// Bulk operations implementation
size_t SearchEngine::bulk_insert(const std::vector<Document>& documents) {
    size_t successful_inserts = 0;

    for (const auto& doc : documents) {
        try {
            add_document(doc);
            successful_inserts++;
        } catch (const std::exception& e) {
            spdlog::warn("Failed to insert document {}: {}",
                         std::string(doc.get_id()), e.what());
        }
    }

    spdlog::info("Bulk insert completed: {}/{} documents inserted",
                 successful_inserts, documents.size());
    return successful_inserts;
}

size_t SearchEngine::bulk_update(const std::vector<Document>& documents) {
    size_t successful_updates = 0;

    for (const auto& doc : documents) {
        try {
            update_document(doc);
            successful_updates++;
        } catch (const std::exception& e) {
            spdlog::warn("Failed to update document {}: {}",
                         std::string(doc.get_id()), e.what());
        }
    }

    spdlog::info("Bulk update completed: {}/{} documents updated",
                 successful_updates, documents.size());
    return successful_updates;
}

size_t SearchEngine::bulk_delete(const std::vector<String>& doc_ids) {
    size_t successful_deletes = 0;

    for (const auto& doc_id : doc_ids) {
        try {
            remove_document(doc_id);
            successful_deletes++;
        } catch (const std::exception& e) {
            spdlog::warn("Failed to delete document {}: {}",
                         std::string(doc_id), e.what());
        }
    }

    spdlog::info("Bulk delete completed: {}/{} documents deleted",
                 successful_deletes, doc_ids.size());
    return successful_deletes;
}

// Stemming implementation (basic Porter stemmer)
std::string SearchEngine::stem_word(const std::string& word) const {
    if (!config_.enable_stemming || word.length() < 3) {
        return word;
    }

    std::string stemmed = word;

    // Simple suffix removal rules (basic Porter stemmer subset)
    static const std::vector<std::pair<std::string, std::string>> rules = {
        {"ies", "y"}, {"ied", "y"}, {"ying", "y"}, {"ing", ""}, {"ly", ""},
        {"ed", ""},   {"ies", "i"}, {"ied", "i"},  {"ies", ""}, {"s", ""}};

    for (const auto& [suffix, replacement] : rules) {
        if (stemmed.length() > suffix.length() &&
            stemmed.substr(stemmed.length() - suffix.length()) == suffix) {
            stemmed = stemmed.substr(0, stemmed.length() - suffix.length()) +
                      replacement;
            break;
        }
    }

    return stemmed;
}

std::vector<String> SearchEngine::tokenize_with_stemming(
    const String& content) const {
    auto tokens = tokenize_content_optimized(content);

    if (config_.enable_stemming) {
        for (auto& token : tokens) {
            std::string token_str = std::string(token);
            std::string stemmed = stem_word(token_str);
            token = String(stemmed);
        }
    }

    return tokens;
}

// Enhanced Boolean Query Parser Implementation
SearchEngine::BooleanQuery SearchEngine::parse_boolean_query(
    const String& query) const {
    BooleanQuery parsed_query;
    std::string query_str = std::string(query);

    // Simple tokenization and operator detection
    std::istringstream iss(query_str);
    std::string token;

    while (iss >> token) {
        // Convert to uppercase for operator matching
        std::string upper_token = token;
        std::transform(upper_token.begin(), upper_token.end(),
                       upper_token.begin(), ::toupper);

        if (upper_token == "AND") {
            parsed_query.operators.push_back(BooleanQuery::Operator::AND);
        } else if (upper_token == "OR") {
            parsed_query.operators.push_back(BooleanQuery::Operator::OR);
        } else if (upper_token == "NOT") {
            parsed_query.operators.push_back(BooleanQuery::Operator::NOT);
        } else {
            // Remove quotes if present
            if (token.front() == '"' && token.back() == '"' &&
                token.length() > 1) {
                token = token.substr(1, token.length() - 2);
            }

            // Convert to lowercase for searching
            std::transform(token.begin(), token.end(), token.begin(),
                           ::tolower);
            parsed_query.terms.push_back(token);
        }
    }

    return parsed_query;
}

std::vector<std::shared_ptr<Document>> SearchEngine::execute_boolean_query(
    const BooleanQuery& query) const {
    if (query.terms.empty()) {
        return {};
    }

    // Start with first term
    HashSet<String> result_docs;
    bool first_term = true;

    for (size_t i = 0; i < query.terms.size(); ++i) {
        const std::string& term = query.terms[i];
        HashSet<String> term_docs;

        // Find documents containing this term
        String term_str(term);
        auto& shard = get_shard(term_str);
        std::shared_lock lock(shard.mutex);
        auto it = shard.content_index.find(term_str);
        if (it != shard.content_index.end()) {
            term_docs.insert(it->second.begin(), it->second.end());
        }
        lock.unlock();

        if (first_term) {
            result_docs = std::move(term_docs);
            first_term = false;
        } else {
            // Apply operator (default to AND if no operator specified)
            BooleanQuery::Operator op = (i - 1 < query.operators.size())
                                            ? query.operators[i - 1]
                                            : BooleanQuery::Operator::AND;

            switch (op) {
                case BooleanQuery::Operator::AND: {
                    HashSet<String> intersection;
                    for (const auto& doc_id : result_docs) {
                        if (term_docs.count(doc_id)) {
                            intersection.insert(doc_id);
                        }
                    }
                    result_docs = std::move(intersection);
                    break;
                }
                case BooleanQuery::Operator::OR: {
                    result_docs.insert(term_docs.begin(), term_docs.end());
                    break;
                }
                case BooleanQuery::Operator::NOT: {
                    for (const auto& doc_id : term_docs) {
                        result_docs.erase(doc_id);
                    }
                    break;
                }
            }
        }
    }

    // Convert to document pointers
    std::vector<std::shared_ptr<Document>> results;
    for (const auto& doc_id : result_docs) {
        auto& shard = get_shard(doc_id);
        std::shared_lock lock(shard.mutex);
        auto it = shard.documents.find(doc_id);
        if (it != shard.documents.end()) {
            results.push_back(it->second);
        }
    }

    return results;
}

// Enhanced autocomplete with frequency ranking
std::vector<std::pair<String, size_t>> SearchEngine::auto_complete_ranked(
    const String& prefix, size_t max_results) {
    if (prefix.empty()) {
        return {};
    }

    std::string prefix_lower = std::string(prefix);
    std::transform(prefix_lower.begin(), prefix_lower.end(),
                   prefix_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::vector<std::future<std::vector<std::pair<String, size_t>>>> futures;
    for (const auto& shard_ptr : shards_) {
        futures.push_back(
            std::async(std::launch::async, [&prefix_lower, &shard_ptr] {
                std::vector<std::pair<String, size_t>> suggestions;
                std::shared_lock lock(shard_ptr->mutex);
                for (const auto& [tag, doc_ids] : shard_ptr->tag_index) {
                    std::string tag_lower = tag;
                    std::transform(
                        tag_lower.begin(), tag_lower.end(), tag_lower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    if (tag_lower.rfind(prefix_lower, 0) == 0) {
                        suggestions.emplace_back(String(tag), doc_ids.size());
                    }
                }
                return suggestions;
            }));
    }

    std::unordered_map<String, size_t> suggestion_map;
    for (auto& future : futures) {
        auto suggestions = future.get();
        for (const auto& [suggestion, frequency] : suggestions) {
            suggestion_map[suggestion] += frequency;
        }
    }

    // Convert to vector and sort by frequency
    std::vector<std::pair<String, size_t>> ranked_suggestions;
    for (const auto& [suggestion, frequency] : suggestion_map) {
        ranked_suggestions.emplace_back(suggestion, frequency);
    }

    std::sort(ranked_suggestions.begin(), ranked_suggestions.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    if (ranked_suggestions.size() > max_results) {
        ranked_suggestions.resize(max_results);
    }

    return ranked_suggestions;
}

// Document similarity calculation
double SearchEngine::calculate_cosine_similarity(const Document& doc1,
                                                 const Document& doc2) const {
    auto tf_vector1 = create_tf_vector(doc1);
    auto tf_vector2 = create_tf_vector(doc2);

    double dot_product = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    // Calculate dot product and norms
    for (const auto& [term, tf1] : tf_vector1) {
        auto it = tf_vector2.find(term);
        if (it != tf_vector2.end()) {
            dot_product += tf1 * it->second;
        }
        norm1 += tf1 * tf1;
    }

    for (const auto& [term, tf2] : tf_vector2) {
        norm2 += tf2 * tf2;
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dot_product / (std::sqrt(norm1) * std::sqrt(norm2));
}

double SearchEngine::calculate_jaccard_similarity(const Document& doc1,
                                                  const Document& doc2) const {
    auto tokens1 =
        get_cached_tokens(String(doc1.get_id()), String(doc1.get_content()));
    auto tokens2 =
        get_cached_tokens(String(doc2.get_id()), String(doc2.get_content()));

    HashSet<String> set1(tokens1.begin(), tokens1.end());
    HashSet<String> set2(tokens2.begin(), tokens2.end());

    // Calculate intersection
    HashSet<String> intersection;
    for (const auto& token : set1) {
        if (set2.count(token)) {
            intersection.insert(token);
        }
    }

    // Calculate union size
    size_t union_size = set1.size() + set2.size() - intersection.size();

    if (union_size == 0) {
        return 0.0;
    }

    return static_cast<double>(intersection.size()) / union_size;
}

std::unordered_map<String, double> SearchEngine::create_tf_vector(
    const Document& doc) const {
    std::unordered_map<String, double> tf_vector;
    auto tokens =
        get_cached_tokens(String(doc.get_id()), String(doc.get_content()));

    // Count term frequencies
    std::unordered_map<String, size_t> term_counts;
    for (const auto& token : tokens) {
        term_counts[token]++;
    }

    // Calculate TF values
    for (const auto& [term, count] : term_counts) {
        tf_vector[term] = 1.0 + std::log(static_cast<double>(count));
    }

    return tf_vector;
}

// Find similar documents
std::vector<std::pair<std::shared_ptr<Document>, double>>
SearchEngine::find_similar_documents(const String& doc_id, size_t max_results,
                                     double min_similarity) {
    // Find the reference document
    auto& shard = get_shard(doc_id);
    std::shared_lock lock(shard.mutex);
    auto it = shard.documents.find(doc_id);
    if (it == shard.documents.end()) {
        lock.unlock();
        throw DocumentNotFoundException(doc_id);
    }
    auto reference_doc = it->second;
    lock.unlock();

    std::vector<std::pair<std::shared_ptr<Document>, double>> similar_docs;

    // Compare with all other documents
    for (const auto& shard_ptr : shards_) {
        std::shared_lock shard_lock(shard_ptr->mutex);
        for (const auto& [other_doc_id, other_doc] : shard_ptr->documents) {
            if (other_doc_id == doc_id) {
                continue;  // Skip self
            }

            double similarity =
                calculate_cosine_similarity(*reference_doc, *other_doc);
            if (similarity >= min_similarity) {
                similar_docs.emplace_back(other_doc, similarity);
            }
        }
    }

    // Sort by similarity (descending)
    std::sort(similar_docs.begin(), similar_docs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Limit results
    if (similar_docs.size() > max_results) {
        similar_docs.resize(max_results);
    }

    return similar_docs;
}

// Semantic search implementation
SearchResults SearchEngine::semantic_search(
    const String& query_text, const SearchPagination& pagination) {
    SearchResults results;
    results.total_count = 0;
    results.offset = pagination.offset;
    results.search_time_ms = 0.0;
    results.from_cache = false;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Create a temporary document from the query
    Document query_doc("__query__", std::string(query_text), {});

    std::vector<std::pair<std::shared_ptr<Document>, double>> scored_docs;

    // Calculate similarity with all documents
    for (const auto& shard_ptr : shards_) {
        std::shared_lock lock(shard_ptr->mutex);
        for (const auto& [doc_id, doc] : shard_ptr->documents) {
            double similarity = calculate_cosine_similarity(query_doc, *doc);
            if (similarity > 0.01) {  // Minimum threshold
                scored_docs.emplace_back(doc, similarity);
            }
        }
    }

    // Sort by similarity
    std::sort(scored_docs.begin(), scored_docs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    results.total_count = scored_docs.size();

    // Apply pagination
    size_t start_idx = pagination.offset;
    size_t end_idx = std::min(start_idx + pagination.limit, scored_docs.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        SearchResult result;
        result.document = scored_docs[i].first;
        result.score = scored_docs[i].second;
        result.matched_terms = {};  // Could be enhanced to show matched terms
        results.results.push_back(result);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    results.search_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count();

    return results;
}

}  // namespace atom::search
