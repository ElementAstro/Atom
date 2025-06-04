/*
 * crash_quotes_.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-24

Description: Quote manager for crash report.

**************************************************/

#include "crash_quotes.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <unordered_set>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/random.hpp"

#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace atom::system {

// Quote implementation
auto Quote::toString(bool includeMetadata) const -> std::string {
    std::string result;
    result.reserve(text_.size() + author_.size() + category_.size() + 20);

    result = text_ + " - " + author_;

    if (includeMetadata) {
        if (!category_.empty()) {
            result += " [" + category_ + "]";
        }
        if (year_ > 0) {
            result += " (" + std::to_string(year_) + ")";
        }
    }

    return result;
}

// QuoteManager implementation
QuoteManager::QuoteManager(const std::string &filename) {
    loadQuotesFromJson(filename);
}

bool QuoteManager::addQuote(const Quote &quote) {
    spdlog::info("Adding quote: {} - {}", quote.getText(), quote.getAuthor());

    // Check if quote already exists
    auto it = std::find_if(quotes_.begin(), quotes_.end(),
                           [&quote](const Quote &q) { return q == quote; });

    if (it != quotes_.end()) {
        spdlog::warn("Quote already exists: {} - {}", quote.getText(),
                     quote.getAuthor());
        return false;
    }

    quotes_.push_back(quote);
    cacheValid_ = false;
    spdlog::info("Quote added successfully");
    return true;
}

size_t QuoteManager::addQuotes(const std::vector<Quote> &quotes) {
    spdlog::info("Adding batch of {} quotes", quotes.size());

    size_t addedCount = 0;
    std::unordered_set<std::string> existingQuotes;
    existingQuotes.reserve(quotes_.size());

    // Create a set of existing quotes for faster lookup
    for (const auto &q : quotes_) {
        existingQuotes.insert(q.getText() + "|||" + q.getAuthor());
    }

    quotes_.reserve(quotes_.size() + quotes.size());

    for (const auto &quote : quotes) {
        std::string key = quote.getText() + "|||" + quote.getAuthor();
        if (existingQuotes.find(key) == existingQuotes.end()) {
            quotes_.push_back(quote);
            existingQuotes.insert(std::move(key));
            addedCount++;
        }
    }

    if (addedCount > 0) {
        cacheValid_ = false;
    }

    spdlog::info("Added {} new quotes successfully", addedCount);
    return addedCount;
}

bool QuoteManager::removeQuote(const Quote &quote) {
    spdlog::info("Removing quote: {} - {}", quote.getText(), quote.getAuthor());

    auto initialSize = quotes_.size();
    quotes_.erase(
        std::remove_if(quotes_.begin(), quotes_.end(),
                       [&quote](const Quote &q) { return q == quote; }),
        quotes_.end());

    bool removed = quotes_.size() < initialSize;

    if (removed) {
        cacheValid_ = false;
        spdlog::info("Quote removed successfully");
    } else {
        spdlog::warn("Quote not found: {} - {}", quote.getText(),
                     quote.getAuthor());
    }

    return removed;
}

size_t QuoteManager::removeQuotesByAuthor(const std::string &author) {
    spdlog::info("Removing all quotes by author: {}", author);

    auto initialSize = quotes_.size();
    quotes_.erase(std::remove_if(quotes_.begin(), quotes_.end(),
                                 [&author](const Quote &q) {
                                     return q.getAuthor() == author;
                                 }),
                  quotes_.end());

    size_t removedCount = initialSize - quotes_.size();

    if (removedCount > 0) {
        cacheValid_ = false;
        spdlog::info("Removed {} quotes by author: {}", removedCount, author);
    } else {
        spdlog::warn("No quotes found by author: {}", author);
    }

    return removedCount;
}

#ifdef DEBUG
void QuoteManager::displayQuotes() const {
    LOG_F(INFO, "Displaying all quotes ({})", quotes_.size());
    for (const auto &quote : quotes_) {
        std::cout << quote.toString(true) << std::endl;
    }
    LOG_F(INFO, "Displayed all quotes successfully");
}
#endif

void QuoteManager::shuffleQuotes() {
    spdlog::info("Shuffling quotes");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(quotes_.begin(), quotes_.end(), gen);

    spdlog::info("Quotes shuffled successfully");
}

void QuoteManager::clearQuotes() {
    spdlog::info("Clearing all quotes");
    quotes_.clear();
    authorCache_.clear();
    categoryCache_.clear();
    cacheValid_ = true;  // Empty cache is valid
    spdlog::info("All quotes cleared successfully");
}

bool QuoteManager::loadQuotesFromJson(const std::string &filename,
                                      bool append) {
    spdlog::info("Loading quotes from JSON file: {}", filename);

    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open JSON file: {}", filename);
        return false;
    }

    try {
        if (!append) {
            clearQuotes();
        }

        json data = json::parse(file);
        std::vector<Quote> newQuotes;
        newQuotes.reserve(data.size());

        for (const auto &quoteJson : data) {
            std::string quoteText = quoteJson.value("text", "");
            std::string quoteAuthor = quoteJson.value("author", "");
            std::string quoteCategory = quoteJson.value("category", "");
            int quoteYear = quoteJson.value("year", 0);

            if (!quoteText.empty() && !quoteAuthor.empty()) {
                newQuotes.emplace_back(std::move(quoteText),
                                       std::move(quoteAuthor),
                                       std::move(quoteCategory), quoteYear);
            }
        }

        size_t addedCount = append ? addQuotes(newQuotes) : newQuotes.size();

        if (!append) {
            quotes_ = std::move(newQuotes);
            cacheValid_ = false;
        }

        spdlog::info("Loaded {} quotes successfully from JSON file: {}",
                     addedCount, filename);
        return true;
    } catch (const nlohmann::json::parse_error &e) {
        spdlog::error("Error parsing JSON file: {} - {}", filename, e.what());
        THROW_UNLAWFUL_OPERATION("Error parsing JSON file: " +
                                 std::string(e.what()));
        return false;
    }
}

bool QuoteManager::saveQuotesToJson(const std::string &filename) const {
    spdlog::info("Saving quotes to JSON file: {}", filename);

    std::ofstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open JSON file for writing: {}", filename);
        return false;
    }

    try {
        json data = json::array();

        for (const auto &quote : quotes_) {
            json quoteJson = {{"text", quote.getText()},
                              {"author", quote.getAuthor()}};

            if (!quote.getCategory().empty()) {
                quoteJson["category"] = quote.getCategory();
            }

            if (quote.getYear() > 0) {
                quoteJson["year"] = quote.getYear();
            }

            data.push_back(std::move(quoteJson));
        }

        file << data.dump(4);
        spdlog::info("Quotes saved successfully to JSON file: {}", filename);
        return true;
    } catch (const std::exception &e) {
        spdlog::error("Error saving JSON file: {} - {}", filename, e.what());
        return false;
    }
}

auto QuoteManager::searchQuotes(const std::string &keyword,
                                bool caseSensitive) const
    -> std::vector<Quote> {
    spdlog::info("Searching quotes with keyword: {} (case sensitive: {})",
                 keyword, caseSensitive ? "yes" : "no");

    if (keyword.empty()) {
        spdlog::warn("Empty search keyword provided");
        return {};
    }

    std::vector<Quote> results;
    results.reserve(quotes_.size() / 10);

    if (caseSensitive) {
        for (const auto &quote : quotes_) {
            if (quote.getText().find(keyword) != std::string::npos) {
                results.push_back(quote);
            }
        }
    } else {
        // Case-insensitive search
        std::string lowerKeyword = keyword;
        std::transform(lowerKeyword.begin(), lowerKeyword.end(),
                       lowerKeyword.begin(), ::tolower);

        for (const auto &quote : quotes_) {
            std::string lowerText = quote.getText();
            std::transform(lowerText.begin(), lowerText.end(),
                           lowerText.begin(), ::tolower);

            if (lowerText.find(lowerKeyword) != std::string::npos) {
                results.push_back(quote);
            }
        }
    }

    spdlog::info("Found {} quotes with keyword: {}", results.size(), keyword);
    return results;
}

void QuoteManager::rebuildCache() const {
    if (cacheValid_) {
        return;
    }

    spdlog::info("Rebuilding quote cache");
    authorCache_.clear();
    categoryCache_.clear();

    for (size_t i = 0; i < quotes_.size(); ++i) {
        const auto &quote = quotes_[i];
        authorCache_[quote.getAuthor()].push_back(i);
        if (!quote.getCategory().empty()) {
            categoryCache_[quote.getCategory()].push_back(i);
        }
    }

    cacheValid_ = true;
    spdlog::info("Quote cache rebuilt successfully");
}

bool QuoteManager::needCacheRebuild() const { return !cacheValid_; }

auto QuoteManager::filterQuotesByAuthor(const std::string &author) const
    -> std::vector<Quote> {
    spdlog::info("Filtering quotes by author: {}", author);

    if (needCacheRebuild()) {
        rebuildCache();
    }

    std::vector<Quote> results;

    // Use the cache for faster lookup
    auto it = authorCache_.find(author);
    if (it != authorCache_.end()) {
        results.reserve(it->second.size());
        for (auto index : it->second) {
            results.push_back(quotes_[index]);
        }
    }

    spdlog::info("Found {} quotes by author: {}", results.size(), author);
    return results;
}

auto QuoteManager::filterQuotesByCategory(const std::string &category) const
    -> std::vector<Quote> {
    spdlog::info("Filtering quotes by category: {}", category);

    if (needCacheRebuild()) {
        rebuildCache();
    }

    std::vector<Quote> results;

    // Use the cache for faster lookup
    auto it = categoryCache_.find(category);
    if (it != categoryCache_.end()) {
        results.reserve(it->second.size());
        for (auto index : it->second) {
            results.push_back(quotes_[index]);
        }
    }

    spdlog::info("Found {} quotes in category: {}", results.size(), category);
    return results;
}

auto QuoteManager::filterQuotesByYear(int year) const -> std::vector<Quote> {
    spdlog::info("Filtering quotes by year: {}", year);

    std::vector<Quote> results;
    results.reserve(quotes_.size() / 10);

    for (const auto &quote : quotes_) {
        if (quote.getYear() == year) {
            results.push_back(quote);
        }
    }

    spdlog::info("Found {} quotes from year: {}", results.size(), year);
    return results;
}

auto QuoteManager::filterQuotes(
    std::function<bool(const Quote &)> filterFunc) const -> std::vector<Quote> {
    spdlog::info("Filtering quotes with custom filter function");

    std::vector<Quote> results;
    results.reserve(quotes_.size() / 10);

    for (const auto &quote : quotes_) {
        if (filterFunc(quote)) {
            results.push_back(quote);
        }
    }

    spdlog::info("Found {} quotes matching custom filter", results.size());
    return results;
}

auto QuoteManager::getRandomQuote() const -> std::string {
    spdlog::info("Getting a random quote");

    auto quoteOpt = getRandomQuoteObject();
    if (!quoteOpt) {
        spdlog::warn("No quotes available");
        return "";
    }

    std::string randomQuote = quoteOpt->toString();
    spdlog::info("Random quote: {}", randomQuote);
    return randomQuote;
}

auto QuoteManager::getRandomQuoteObject() const -> std::optional<Quote> {
    if (quotes_.empty()) {
        spdlog::warn("No quotes available");
        return std::nullopt;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, quotes_.size() - 1);

    size_t quoteId = dis(gen);
    return quotes_[quoteId];
}

}  // namespace atom::system
