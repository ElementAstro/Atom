/**
 * @file search.cpp
 * @brief Implements the Document and SearchEngine classes for Atom Search.
 * @date 2025-07-16
 */

#include "search.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <queue>
#include <regex>
#include <sstream>

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
        throw DocumentValidationException("Document ID too long (max 256 chars)");
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
        throw DocumentValidationException("Tag too long (max 100 chars): " + tag);
    }
    tags_.insert(tag);
}

void Document::remove_tag(const std::string& tag) { tags_.erase(tag); }

// SearchEngine Implementation
SearchEngine::SearchEngine(unsigned num_threads)
    : num_threads_(num_threads > 0 ? num_threads
                                   : std::thread::hardware_concurrency()),
      shard_mask_([this] {
          size_t shard_count = num_threads_;
          if (shard_count == 0) shard_count = 1;
          size_t power = 1;
          while (power < shard_count) power <<= 1;
          return power - 1;
      }()) {
    shards_.resize(shard_mask_ + 1);
    for (auto& shard : shards_) {
        shard = std::make_unique<Shard>();
    }
    task_queue_ = std::make_unique<ConcurrentQueue<SearchTask>>();
    start_worker_threads();
    spdlog::info("SearchEngine initialized with {} shards and {} worker threads.",
                 shards_.size(), num_threads_);
}

SearchEngine::~SearchEngine() {
    spdlog::info("Shutting down SearchEngine.");
    stop_worker_threads();
}

SearchEngine::Shard& SearchEngine::get_shard(const String& key) const {
    return *shards_[std::hash<String>{}(key) & shard_mask_];
}

SearchEngine::Shard& SearchEngine::get_shard(const std::string& key) const {
    return *shards_[std::hash<std::string>{}(key) & shard_mask_];
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
        futures.push_back(std::async(std::launch::async, [&, shard_ptr] {
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
        futures.push_back(std::async(std::launch::async, [&, shard_ptr] {
            HashMap<String, double> local_scores;
            std::shared_lock lock(shard_ptr->mutex);
            for (const auto& token : tokens) {
                auto it = shard_ptr->content_index.find(token);
                if (it != shard_ptr->content_index.end()) {
                    for (const auto& doc_id : it->second) {
                        auto doc_it = shard_ptr->documents.find(doc_id);
                        if (doc_it != shard_ptr->documents.end()) {
                            local_scores[doc_id] +=
                                tf_idf(*doc_it->second, std::string_view(token));
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
        futures.push_back(std::async(std::launch::async, [&, shard_ptr] {
            std::vector<String> suggestions;
            std::shared_lock lock(shard_ptr->mutex);
            for (const auto& [tag, _] : shard_ptr->tag_index) {
                std::string tag_lower = tag;
                std::transform(tag_lower.begin(), tag_lower.end(),
                               tag_lower.begin(),
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
            ofs.write(reinterpret_cast<const char*>(&num_tags), sizeof(num_tags));
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
            ifs.read(reinterpret_cast<char*>(&click_count), sizeof(click_count));

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

size_t SearchEngine::get_document_count() const noexcept {
    return total_docs_.load();
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

void SearchEngine::add_content_to_index(
    Shard& doc_shard, const std::shared_ptr<Document>& doc) {
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
    Shard& doc_shard, const std::shared_ptr<Document>& doc) {
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

std::vector<String> SearchEngine::tokenize_content(const String& content) const {
    std::vector<String> tokens;
    std::stringstream ss{std::string(content)};
    std::string token_std;

    while (ss >> token_std) {
        token_std.erase(std::remove_if(token_std.begin(), token_std.end(),
                                       [](unsigned char c) {
                                           return !std::isalnum(c);
                                       }),
                        token_std.end());
        if (!token_std.empty()) {
            std::transform(token_std.begin(), token_std.end(),
                           token_std.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            tokens.push_back(String(token_std));
        }
    }
    return tokens;
}

double SearchEngine::tf_idf(const Document& doc, std::string_view term) const {
    std::string content_str = std::string(doc.get_content());
    size_t term_freq = 0;
    size_t pos = content_str.find(term);
    while (pos != std::string::npos) {
        term_freq++;
        pos = content_str.find(term, pos + 1);
    }

    double tf = static_cast<double>(term_freq) / content_str.length();

    String term_str(term);
    auto& shard = get_shard(term_str);
    std::shared_lock lock(shard.mutex);
    auto it = shard.doc_frequency.find(term_str);
    int doc_freq = (it != shard.doc_frequency.end()) ? it->second : 0;
    lock.unlock();

    double idf = (doc_freq > 0)
                     ? std::log(static_cast<double>(total_docs_.load()) /
                                (1.0 + doc_freq))
                     : 0;

    return tf * idf;
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

    if (m == 0) return static_cast<int>(n);
    if (n == 0) return static_cast<int>(m);

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

}  // namespace atom::search
