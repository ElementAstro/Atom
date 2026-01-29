#ifndef ATOM_SEARCH_CORE_DOCUMENT_HPP
#define ATOM_SEARCH_CORE_DOCUMENT_HPP

#include "exceptions.hpp"
#include "types.hpp"

namespace atom::search {

/**
 * @brief Represents a document with an ID, content, tags, and click count.
 * @details Thread-safe document class with atomic operations for click
 * counting.
 */
class Document {
public:
    using TagSet = std::set<std::string>;
    using Timestamp = std::chrono::steady_clock::time_point;

    /**
     * @brief Constructs a Document object.
     * @param id The unique identifier of the document
     * @param content The content of the document
     * @param tags The tags associated with the document
     * @param config Validation configuration
     * @throws DocumentValidationException if validation fails
     */
    explicit Document(String id, String content,
                      std::initializer_list<std::string> tags = {},
                      DocumentConfig config = {});

    /**
     * @brief Constructs a Document from a span of tags.
     * @param id The unique identifier
     * @param content The content
     * @param tags Span of tags
     * @param config Validation configuration
     */
    Document(String id, String content, std::span<const std::string> tags,
             DocumentConfig config = {});

    Document(const Document& other);
    Document& operator=(const Document& other);
    Document(Document&& other) noexcept;
    Document& operator=(Document&& other) noexcept;
    ~Document() = default;

    /**
     * @brief Validates document fields.
     * @return SearchResult indicating success or error
     */
    [[nodiscard]] SearchResult<void> validate() const noexcept;

    /**
     * @brief Validates and throws on error.
     * @throws DocumentValidationException if validation fails
     */
    void validateOrThrow() const;

    [[nodiscard]] std::string_view getId() const noexcept {
        return std::string_view(id_);
    }

    [[nodiscard]] std::string_view getContent() const noexcept {
        return std::string_view(content_);
    }

    [[nodiscard]] const TagSet& getTags() const noexcept { return tags_; }

    [[nodiscard]] int getClickCount() const noexcept {
        return clickCount_.load(std::memory_order_acquire);
    }

    [[nodiscard]] Timestamp getCreatedAt() const noexcept { return createdAt_; }

    [[nodiscard]] Timestamp getUpdatedAt() const noexcept {
        return updatedAt_.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t getContentHash() const noexcept {
        return contentHash_;
    }

    [[nodiscard]] const DocumentConfig& getConfig() const noexcept {
        return config_;
    }

    void setContent(String content);

    /**
     * @brief Sets content without validation (for internal use).
     */
    void setContentUnchecked(String content) noexcept;

    void addTag(const std::string& tag);
    void addTag(std::string&& tag);

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    void addTags(R&& tags) {
        for (auto&& tag : tags) {
            addTag(std::forward<decltype(tag)>(tag));
        }
    }

    void removeTag(const std::string& tag);
    [[nodiscard]] bool hasTag(const std::string& tag) const noexcept;

    void incrementClickCount() noexcept {
        clickCount_.fetch_add(1, std::memory_order_acq_rel);
    }

    void setClickCount(int count) noexcept {
        clickCount_.store(count, std::memory_order_release);
    }

    void resetClickCount() noexcept {
        clickCount_.store(0, std::memory_order_release);
    }

    /**
     * @brief Computes Jaccard similarity score with another document based on
     * tags.
     * @param other The other document
     * @return Similarity score between 0.0 and 1.0
     */
    [[nodiscard]] double computeSimilarity(
        const Document& other) const noexcept;

    /**
     * @brief Checks equality based on ID.
     */
    [[nodiscard]] bool operator==(const Document& other) const noexcept {
        return id_ == other.id_;
    }

    [[nodiscard]] auto operator<=>(const Document& other) const noexcept {
        return id_ <=> other.id_;
    }

private:
    void updateTimestamp() noexcept;
    void computeContentHash() noexcept;

    String id_;
    String content_;
    TagSet tags_;
    std::atomic<int> clickCount_{0};
    Timestamp createdAt_;
    std::atomic<Timestamp> updatedAt_;
    size_t contentHash_{0};
    DocumentConfig config_;
};

/**
 * @brief Search result with score and metadata.
 */
struct ScoredDocument {
    std::shared_ptr<Document> document;
    double score;
    std::vector<std::string> matchedTerms;

    [[nodiscard]] bool operator<(const ScoredDocument& other) const noexcept {
        return score < other.score;
    }

    [[nodiscard]] bool operator>(const ScoredDocument& other) const noexcept {
        return score > other.score;
    }

    [[nodiscard]] bool operator==(const ScoredDocument& other) const noexcept {
        return document == other.document && score == other.score;
    }
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_CORE_DOCUMENT_HPP
