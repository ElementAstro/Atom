/*
 * crash_quotes.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-24

Description: Quote manager for crash report.

**************************************************/

#ifndef ATOM_SYSTEM_CRASH_QUOTES_HPP
#define ATOM_SYSTEM_CRASH_QUOTES_HPP

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace atom::system {
/**
 * @brief Represents a quote with its text and author.
 */
class Quote {
public:
    /**
     * @brief Constructs a new Quote object.
     *
     * @param text The text of the quote.
     * @param author The author of the quote.
     * @param category The category of the quote (optional).
     * @param year The year the quote was made (optional).
     */
    explicit Quote(std::string text, std::string author,
                   std::string category = "", int year = 0)
        : text_(std::move(text)),
          author_(std::move(author)),
          category_(std::move(category)),
          year_(year) {}

    /**
     * @brief Gets the text of the quote.
     *
     * @return The text of the quote.
     */
    [[nodiscard]] auto getText() const -> const std::string& { return text_; }

    /**
     * @brief Gets the author of the quote.
     *
     * @return The author of the quote.
     */
    [[nodiscard]] auto getAuthor() const -> const std::string& {
        return author_;
    }

    /**
     * @brief Gets the category of the quote.
     *
     * @return The category of the quote.
     */
    [[nodiscard]] auto getCategory() const -> const std::string& {
        return category_;
    }

    /**
     * @brief Gets the year of the quote.
     *
     * @return The year of the quote.
     */
    [[nodiscard]] auto getYear() const -> int { return year_; }

    /**
     * @brief Sets the category of the quote.
     *
     * @param category The new category.
     */
    void setCategory(std::string category) { category_ = std::move(category); }

    /**
     * @brief Sets the year of the quote.
     *
     * @param year The new year.
     */
    void setYear(int year) { year_ = year; }

    /**
     * @brief Creates a formatted string representation of the quote.
     *
     * @param includeMetadata Whether to include category and year in the
     * output.
     * @return Formatted quote string.
     */
    [[nodiscard]] auto toString(bool includeMetadata = false) const
        -> std::string;

    /**
     * @brief Compares two quotes for equality.
     *
     * @param other The quote to compare with.
     * @return True if quotes are equal, false otherwise.
     */
    bool operator==(const Quote& other) const {
        return text_ == other.text_ && author_ == other.author_;
    }

private:
    std::string text_;
    std::string author_;
    std::string category_;
    int year_;
};

/**
 * @brief Manages a collection of quotes.
 */
class QuoteManager {
public:
    /**
     * @brief Default constructor.
     */
    QuoteManager() = default;

    /**
     * @brief Constructor that loads quotes from a file.
     *
     * @param filename The file to load quotes from.
     */
    explicit QuoteManager(const std::string& filename);

    /**
     * @brief Adds a quote to the collection.
     *
     * @param quote The quote to add.
     * @return True if added successfully, false otherwise.
     */
    bool addQuote(const Quote& quote);

    /**
     * @brief Adds multiple quotes to the collection.
     *
     * @param quotes Vector of quotes to add.
     * @return Number of quotes successfully added.
     */
    size_t addQuotes(const std::vector<Quote>& quotes);

    /**
     * @brief Removes a quote from the collection.
     *
     * @param quote The quote to remove.
     * @return True if removed successfully, false if not found.
     */
    bool removeQuote(const Quote& quote);

    /**
     * @brief Removes quotes by author.
     *
     * @param author The author whose quotes should be removed.
     * @return Number of quotes removed.
     */
    size_t removeQuotesByAuthor(const std::string& author);

    /**
     * @brief Shuffles the quotes in the collection.
     */
    void shuffleQuotes();

    /**
     * @brief Clears all quotes in the collection.
     */
    void clearQuotes();

    /**
     * @brief Loads quotes from a JSON file.
     *
     * @param filename The JSON file to load quotes from.
     * @param append Whether to append to existing quotes or replace them.
     * @return True if loaded successfully, false otherwise.
     */
    bool loadQuotesFromJson(const std::string& filename, bool append = false);

    /**
     * @brief Saves quotes to a JSON file.
     *
     * @param filename The JSON file to save quotes to.
     * @return True if saved successfully, false otherwise.
     */
    bool saveQuotesToJson(const std::string& filename) const;

    /**
     * @brief Searches for quotes containing a keyword.
     *
     * @param keyword The keyword to search for.
     * @param caseSensitive Whether the search should be case-sensitive.
     * @return A vector of quotes containing the keyword.
     */
    [[nodiscard]] auto searchQuotes(const std::string& keyword,
                                    bool caseSensitive = false) const
        -> std::vector<Quote>;

    /**
     * @brief Filters quotes by author.
     *
     * @param author The name of the author to filter by.
     * @return A vector of quotes by the specified author.
     */
    [[nodiscard]] auto filterQuotesByAuthor(const std::string& author) const
        -> std::vector<Quote>;

    /**
     * @brief Filters quotes by category.
     *
     * @param category The category to filter by.
     * @return A vector of quotes in the specified category.
     */
    [[nodiscard]] auto filterQuotesByCategory(const std::string& category) const
        -> std::vector<Quote>;

    /**
     * @brief Filters quotes by year.
     *
     * @param year The year to filter by.
     * @return A vector of quotes from the specified year.
     */
    [[nodiscard]] auto filterQuotesByYear(int year) const -> std::vector<Quote>;

    /**
     * @brief Filters quotes using a custom filter function.
     *
     * @param filterFunc The function to use for filtering.
     * @return A vector of quotes that pass the filter.
     */
    [[nodiscard]] auto filterQuotes(
        std::function<bool(const Quote&)> filterFunc) const
        -> std::vector<Quote>;

    /**
     * @brief Gets a random quote from the collection.
     *
     * @return A random quote formatted as string, or empty string if no quotes.
     */
    [[nodiscard]] auto getRandomQuote() const -> std::string;

    /**
     * @brief Gets a random quote from the collection as a Quote object.
     *
     * @return An optional containing a random Quote, or empty if no quotes.
     */
    [[nodiscard]] auto getRandomQuoteObject() const -> std::optional<Quote>;

    /**
     * @brief Gets the number of quotes in the collection.
     *
     * @return The number of quotes.
     */
    [[nodiscard]] auto size() const -> size_t { return quotes_.size(); }

    /**
     * @brief Checks if the collection is empty.
     *
     * @return True if empty, false otherwise.
     */
    [[nodiscard]] auto empty() const -> bool { return quotes_.empty(); }

    /**
     * @brief Gets all quotes in the collection.
     *
     * @return A vector containing all quotes.
     */
    [[nodiscard]] auto getAllQuotes() const -> const std::vector<Quote>& {
        return quotes_;
    }

private:
    std::vector<Quote> quotes_;
    mutable std::unordered_map<std::string, std::vector<size_t>> authorCache_;
    mutable std::unordered_map<std::string, std::vector<size_t>> categoryCache_;

    /**
     * @brief Rebuilds internal caches for quick lookups.
     */
    void rebuildCache() const;

    /**
     * @brief Checks if caches need to be rebuilt.
     */
    [[nodiscard]] bool needCacheRebuild() const;

    mutable bool cacheValid_ = false;
};
}  // namespace atom::system

#endif
