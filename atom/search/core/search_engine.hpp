#ifndef ATOM_SEARCH_CORE_SEARCH_ENGINE_HPP
#define ATOM_SEARCH_CORE_SEARCH_ENGINE_HPP

#include "document.hpp"
#include "exceptions.hpp"
#include "scoring.hpp"
#include "tokenizer.hpp"
#include "types.hpp"

namespace atom::search {

/**
 * @brief A high-performance search engine for indexing and searching documents.
 * @details Thread-safe search engine with parallel indexing and searching,
 *          TF-IDF/BM25 scoring, fuzzy matching, and autocomplete support.
 */
class SearchEngine {
public:
    using DocumentPtr = std::shared_ptr<Document>;
    using DocumentList = std::vector<DocumentPtr>;
    using ScoredDocumentList = std::vector<ScoredDocument>;

    /**
     * @brief Constructs a SearchEngine with configuration.
     * @param config Engine configuration
     */
    explicit SearchEngine(SearchEngineConfig config = {});

    /**
     * @brief Legacy constructor for backward compatibility.
     * @param maxThreads Maximum number of threads to use
     */
    explicit SearchEngine(unsigned maxThreads);

    ~SearchEngine();

    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
    SearchEngine(SearchEngine&&) noexcept;
    SearchEngine& operator=(SearchEngine&&) noexcept;

    // Document Management
    [[nodiscard]] SearchResult<void> addDocument(const Document& doc);
    [[nodiscard]] SearchResult<void> addDocument(Document&& doc);
    [[nodiscard]] SearchResult<void> removeDocument(std::string_view docId);
    [[nodiscard]] SearchResult<void> updateDocument(const Document& doc);
    [[nodiscard]] SearchResult<void> upsertDocument(Document doc);

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Document>
    size_t addDocuments(R&& docs) {
        size_t count = 0;
        for (auto&& doc : docs) {
            if (addDocument(std::forward<decltype(doc)>(doc))) {
                ++count;
            }
        }
        return count;
    }

    void clear() noexcept;
    [[nodiscard]] bool hasDocument(std::string_view docId) const noexcept;
    [[nodiscard]] std::optional<DocumentPtr> getDocument(
        std::string_view docId) const noexcept;
    [[nodiscard]] std::vector<String> getAllDocumentIds() const;
    [[nodiscard]] size_t getDocumentCount() const noexcept;

    // Search Operations
    [[nodiscard]] DocumentList searchByTag(std::string_view tag,
                                           SearchOptions options = {});
    [[nodiscard]] DocumentList fuzzySearchByTag(std::string_view tag,
                                                int tolerance = 2,
                                                SearchOptions options = {});
    [[nodiscard]] DocumentList searchByTags(std::span<const std::string> tags,
                                            SearchOptions options = {});
    [[nodiscard]] DocumentList searchByAnyTag(std::span<const std::string> tags,
                                              SearchOptions options = {});
    [[nodiscard]] ScoredDocumentList searchByContent(
        std::string_view query, SearchOptions options = {});
    [[nodiscard]] DocumentList booleanSearch(std::string_view query,
                                             SearchOptions options = {});
    [[nodiscard]] DocumentList phraseSearch(std::string_view phrase,
                                            SearchOptions options = {});
    [[nodiscard]] std::vector<String> autoComplete(std::string_view prefix,
                                                   size_t maxResults = 10);

    // Index Management
    [[nodiscard]] SearchResult<void> saveIndex(std::string_view filename) const;
    [[nodiscard]] SearchResult<void> loadIndex(std::string_view filename);
    void optimize();
    void rebuildIndex();

    // Statistics
    [[nodiscard]] IndexStats getStats() const noexcept;
    [[nodiscard]] const SearchEngineConfig& getConfig() const noexcept {
        return config_;
    }

    // Utility
    [[nodiscard]] int levenshteinDistance(std::string_view s1,
                                          std::string_view s2) const noexcept;

private:
    struct SearchTask {
        std::vector<String> words;
        std::function<void(std::span<const String>)> callback;
    };

    void startWorkerThreads();
    void stopWorkerThreads();
    void workerFunction();

    void addContentToIndex(const DocumentPtr& doc);
    void removeContentFromIndex(const DocumentPtr& doc);

    void searchByContentWorker(std::span<const String> wordChunk,
                               HashMap<String, double>& scoresMap,
                               threading::mutex& scoresMutex);

    [[nodiscard]] ScoredDocumentList getRankedResults(
        const HashMap<String, double>& scores,
        const SearchOptions& options) const;

    SearchEngineConfig config_;
    Tokenizer tokenizer_;

    HashMap<String, DocumentPtr> documents_;
    HashMap<std::string, std::vector<String>> tagIndex_;
    HashMap<String, HashSet<String>> contentIndex_;
    HashMap<String, int> docFrequency_;
    HashMap<String, int> termFrequency_;

    std::atomic<size_t> totalDocs_{0};
    std::atomic<size_t> totalTokens_{0};
    double avgDocLength_{0.0};

    mutable threading::shared_mutex indexMutex_;
    std::vector<std::unique_ptr<threading::thread>> workerThreads_;
    std::unique_ptr<threading::lockfree_queue<SearchTask>> taskQueue_;
    std::atomic<bool> shouldStopWorkers_{false};
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_CORE_SEARCH_ENGINE_HPP
