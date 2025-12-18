#include "document.hpp"

#include <spdlog/spdlog.h>

namespace atom::search {

Document::Document(String id, String content,
                   std::initializer_list<std::string> tags,
                   DocumentConfig config)
    : id_(std::move(id)),
      content_(std::move(content)),
      tags_(tags),
      createdAt_(std::chrono::steady_clock::now()),
      config_(std::move(config)) {
    updatedAt_.store(createdAt_, std::memory_order_release);
    computeContentHash();
    validateOrThrow();
    spdlog::debug("Document created with id: {}", std::string(id_));
}

Document::Document(String id, String content, std::span<const std::string> tags,
                   DocumentConfig config)
    : id_(std::move(id)),
      content_(std::move(content)),
      tags_(tags.begin(), tags.end()),
      createdAt_(std::chrono::steady_clock::now()),
      config_(std::move(config)) {
    updatedAt_.store(createdAt_, std::memory_order_release);
    computeContentHash();
    validateOrThrow();
    spdlog::debug("Document created with id: {}", std::string(id_));
}

Document::Document(const Document& other)
    : id_(other.id_),
      content_(other.content_),
      tags_(other.tags_),
      clickCount_(other.clickCount_.load(std::memory_order_acquire)),
      createdAt_(other.createdAt_),
      contentHash_(other.contentHash_),
      config_(other.config_) {
    updatedAt_.store(other.updatedAt_.load(std::memory_order_acquire),
                     std::memory_order_release);
}

Document& Document::operator=(const Document& other) {
    if (this != &other) {
        id_ = other.id_;
        content_ = other.content_;
        tags_ = other.tags_;
        clickCount_.store(other.clickCount_.load(std::memory_order_acquire),
                          std::memory_order_release);
        createdAt_ = other.createdAt_;
        updatedAt_.store(other.updatedAt_.load(std::memory_order_acquire),
                         std::memory_order_release);
        contentHash_ = other.contentHash_;
        config_ = other.config_;
    }
    return *this;
}

Document::Document(Document&& other) noexcept
    : id_(std::move(other.id_)),
      content_(std::move(other.content_)),
      tags_(std::move(other.tags_)),
      clickCount_(other.clickCount_.load(std::memory_order_acquire)),
      createdAt_(other.createdAt_),
      contentHash_(other.contentHash_),
      config_(std::move(other.config_)) {
    updatedAt_.store(other.updatedAt_.load(std::memory_order_acquire),
                     std::memory_order_release);
}

Document& Document::operator=(Document&& other) noexcept {
    if (this != &other) {
        id_ = std::move(other.id_);
        content_ = std::move(other.content_);
        tags_ = std::move(other.tags_);
        clickCount_.store(other.clickCount_.load(std::memory_order_acquire),
                          std::memory_order_release);
        createdAt_ = other.createdAt_;
        updatedAt_.store(other.updatedAt_.load(std::memory_order_acquire),
                         std::memory_order_release);
        contentHash_ = other.contentHash_;
        config_ = std::move(other.config_);
    }
    return *this;
}

SearchResult<void> Document::validate() const noexcept {
    if (id_.empty()) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    if (id_.size() > config_.maxIdLength) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    if (!config_.allowEmptyContent && content_.empty()) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    if (content_.size() > config_.maxContentLength) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    if (tags_.size() > config_.maxTagCount) {
        return std::unexpected(SearchErrorCode::DocumentValidationFailed);
    }

    for (const auto& tag : tags_) {
        if (tag.empty() || tag.length() > config_.maxTagLength) {
            return std::unexpected(SearchErrorCode::DocumentValidationFailed);
        }
    }

    return {};
}

void Document::validateOrThrow() const {
    auto result = validate();
    if (!result) {
        if (id_.empty()) {
            throw DocumentValidationException("Document ID cannot be empty");
        }
        if (id_.size() > config_.maxIdLength) {
            throw DocumentValidationException("Document ID too long");
        }
        if (!config_.allowEmptyContent && content_.empty()) {
            throw DocumentValidationException(
                "Document content cannot be empty");
        }
        if (content_.size() > config_.maxContentLength) {
            throw DocumentValidationException("Document content too long");
        }
        if (tags_.size() > config_.maxTagCount) {
            throw DocumentValidationException("Too many tags");
        }
        for (const auto& tag : tags_) {
            if (tag.empty()) {
                throw DocumentValidationException("Tags cannot be empty");
            }
            if (tag.length() > config_.maxTagLength) {
                throw DocumentValidationException("Tag too long: " + tag);
            }
        }
        throw DocumentValidationException("Document validation failed");
    }
}

void Document::setContent(String content) {
    if (!config_.allowEmptyContent && content.empty()) {
        throw DocumentValidationException("Document content cannot be empty");
    }
    if (content.size() > config_.maxContentLength) {
        throw DocumentValidationException("Document content too long");
    }
    content_ = std::move(content);
    computeContentHash();
    updateTimestamp();
}

void Document::setContentUnchecked(String content) noexcept {
    content_ = std::move(content);
    computeContentHash();
    updateTimestamp();
}

void Document::addTag(const std::string& tag) {
    if (tag.empty()) {
        throw DocumentValidationException("Tag cannot be empty");
    }
    if (tag.length() > config_.maxTagLength) {
        throw DocumentValidationException("Tag too long: " + tag);
    }
    if (tags_.size() >= config_.maxTagCount) {
        throw DocumentValidationException("Too many tags");
    }
    tags_.insert(tag);
    updateTimestamp();
}

void Document::addTag(std::string&& tag) {
    if (tag.empty()) {
        throw DocumentValidationException("Tag cannot be empty");
    }
    if (tag.length() > config_.maxTagLength) {
        throw DocumentValidationException("Tag too long: " + tag);
    }
    if (tags_.size() >= config_.maxTagCount) {
        throw DocumentValidationException("Too many tags");
    }
    tags_.insert(std::move(tag));
    updateTimestamp();
}

void Document::removeTag(const std::string& tag) {
    tags_.erase(tag);
    updateTimestamp();
}

bool Document::hasTag(const std::string& tag) const noexcept {
    return tags_.contains(tag);
}

void Document::updateTimestamp() noexcept {
    updatedAt_.store(std::chrono::steady_clock::now(),
                     std::memory_order_release);
}

void Document::computeContentHash() noexcept {
    contentHash_ = std::hash<std::string>{}(content_);
}

double Document::computeSimilarity(const Document& other) const noexcept {
    if (tags_.empty() && other.tags_.empty()) {
        return 0.0;
    }

    size_t intersection = 0;
    for (const auto& tag : tags_) {
        if (other.tags_.contains(tag)) {
            ++intersection;
        }
    }

    size_t unionSize = tags_.size() + other.tags_.size() - intersection;
    return unionSize > 0 ? static_cast<double>(intersection) /
                               static_cast<double>(unionSize)
                         : 0.0;
}

}  // namespace atom::search
