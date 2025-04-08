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
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/random.hpp"

using json = nlohmann::json;

namespace atom::system {

// Quote implementation
auto Quote::toString(bool includeMetadata) const -> std::string {
    std::string result = text_ + " - " + author_;

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
    LOG_F(INFO, "Adding quote: {} - {}", quote.getText(), quote.getAuthor());

    // Check if quote already exists
    auto it = std::find_if(quotes_.begin(), quotes_.end(),
                           [&quote](const Quote &q) { return q == quote; });

    if (it != quotes_.end()) {
        LOG_F(WARNING, "Quote already exists: {} - {}", quote.getText(),
              quote.getAuthor());
        return false;
    }

    quotes_.push_back(quote);
    cacheValid_ = false;
    LOG_F(INFO, "Quote added successfully");
    return true;
}

size_t QuoteManager::addQuotes(const std::vector<Quote> &quotes) {
    LOG_F(INFO, "Adding batch of {} quotes", quotes.size());

    size_t addedCount = 0;
    std::unordered_set<std::string> existingQuotes;

    // Create a set of existing quotes for faster lookup
    for (const auto &q : quotes_) {
        existingQuotes.insert(q.getText() + "|||" + q.getAuthor());
    }

    for (const auto &quote : quotes) {
        std::string key = quote.getText() + "|||" + quote.getAuthor();
        if (existingQuotes.find(key) == existingQuotes.end()) {
            quotes_.push_back(quote);
            existingQuotes.insert(key);
            addedCount++;
        }
    }

    if (addedCount > 0) {
        cacheValid_ = false;
    }

    LOG_F(INFO, "Added {} new quotes successfully", addedCount);
    return addedCount;
}

bool QuoteManager::removeQuote(const Quote &quote) {
    LOG_F(INFO, "Removing quote: {} - {}", quote.getText(), quote.getAuthor());

    auto initialSize = quotes_.size();
    quotes_.erase(
        std::remove_if(quotes_.begin(), quotes_.end(),
                       [&quote](const Quote &q) { return q == quote; }),
        quotes_.end());

    bool removed = quotes_.size() < initialSize;

    if (removed) {
        cacheValid_ = false;
        LOG_F(INFO, "Quote removed successfully");
    } else {
        LOG_F(WARNING, "Quote not found: {} - {}", quote.getText(),
              quote.getAuthor());
    }

    return removed;
}

size_t QuoteManager::removeQuotesByAuthor(const std::string &author) {
    LOG_F(INFO, "Removing all quotes by author: {}", author);

    auto initialSize = quotes_.size();
    quotes_.erase(std::remove_if(quotes_.begin(), quotes_.end(),
                                 [&author](const Quote &q) {
                                     return q.getAuthor() == author;
                                 }),
                  quotes_.end());

    size_t removedCount = initialSize - quotes_.size();

    if (removedCount > 0) {
        cacheValid_ = false;
        LOG_F(INFO, "Removed {} quotes by author: {}", removedCount, author);
    } else {
        LOG_F(WARNING, "No quotes found by author: {}", author);
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
    LOG_F(INFO, "Shuffling quotes");
    atom::utils::Random<std::mt19937, std::uniform_int_distribution<>> random(
        std::random_device{}());
    std::shuffle(quotes_.begin(), quotes_.end(), random.engine());
    LOG_F(INFO, "Quotes shuffled successfully");
}

void QuoteManager::clearQuotes() {
    LOG_F(INFO, "Clearing all quotes");
    quotes_.clear();
    authorCache_.clear();
    categoryCache_.clear();
    cacheValid_ = true;  // Empty cache is valid
    LOG_F(INFO, "All quotes cleared successfully");
}

bool QuoteManager::loadQuotesFromJson(const std::string &filename,
                                      bool append) {
    LOG_F(INFO, "Loading quotes from JSON file: {}", filename);
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open JSON file: {}", filename);
        return false;
    }

    try {
        if (!append) {
            clearQuotes();
        }

        json data = json::parse(file);
        std::vector<Quote> newQuotes;

        for (const auto &quoteJson : data) {
            std::string quoteText = quoteJson.value("text", "");
            std::string quoteAuthor = quoteJson.value("author", "");
            std::string quoteCategory = quoteJson.value("category", "");
            int quoteYear = quoteJson.value("year", 0);

            if (!quoteText.empty() && !quoteAuthor.empty()) {
                newQuotes.emplace_back(quoteText, quoteAuthor, quoteCategory,
                                       quoteYear);
            }
        }

        size_t addedCount = append ? addQuotes(newQuotes) : newQuotes.size();

        if (!append) {
            quotes_ = std::move(newQuotes);
            cacheValid_ = false;
        }

        LOG_F(INFO, "Loaded {} quotes successfully from JSON file: {}",
              addedCount, filename);
        return true;
    } catch (const nlohmann::json::parse_error &e) {
        LOG_F(ERROR, "Error parsing JSON file: {} - {}", filename, e.what());
        THROW_UNLAWFUL_OPERATION("Error parsing JSON file: " +
                                 std::string(e.what()));
        return false;
    }
}

bool QuoteManager::saveQuotesToJson(const std::string &filename) const {
    LOG_F(INFO, "Saving quotes to JSON file: {}", filename);
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_F(ERROR, "Failed to open JSON file for writing: {}", filename);
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

            data.push_back(quoteJson);
        }

        file << data.dump(4);
        LOG_F(INFO, "Quotes saved successfully to JSON file: {}", filename);
        file.close();
        return true;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Error saving JSON file: {} - {}", filename, e.what());
        file.close();
        return false;
    }
}

auto QuoteManager::searchQuotes(const std::string &keyword,
                                bool caseSensitive) const
    -> std::vector<Quote> {
    LOG_F(INFO, "Searching quotes with keyword: {} (case sensitive: {})",
          keyword, caseSensitive ? "yes" : "no");

    if (keyword.empty()) {
        LOG_F(WARNING, "Empty search keyword provided");
        return {};
    }

    std::vector<Quote> results;

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

    LOG_F(INFO, "Found {} quotes with keyword: {}", results.size(), keyword);
    return results;
}

void QuoteManager::rebuildCache() const {
    if (cacheValid_)
        return;

    LOG_F(INFO, "Rebuilding quote cache");
    authorCache_.clear();
    categoryCache_.clear();

    for (size_t i = 0; i < quotes_.size(); ++i) {
        const auto &quote = quotes_[i];
        authorCache_[quote.getAuthor()].push_back(i);
        categoryCache_[quote.getCategory()].push_back(i);
    }

    cacheValid_ = true;
    LOG_F(INFO, "Quote cache rebuilt successfully");
}

bool QuoteManager::needCacheRebuild() const { return !cacheValid_; }

auto QuoteManager::filterQuotesByAuthor(const std::string &author) const
    -> std::vector<Quote> {
    LOG_F(INFO, "Filtering quotes by author: {}", author);

    // Rebuild cache if needed
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

    LOG_F(INFO, "Found {} quotes by author: {}", results.size(), author);
    return results;
}

auto QuoteManager::filterQuotesByCategory(const std::string &category) const
    -> std::vector<Quote> {
    LOG_F(INFO, "Filtering quotes by category: {}", category);

    // Rebuild cache if needed
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

    LOG_F(INFO, "Found {} quotes in category: {}", results.size(), category);
    return results;
}

auto QuoteManager::filterQuotesByYear(int year) const -> std::vector<Quote> {
    LOG_F(INFO, "Filtering quotes by year: {}", year);

    std::vector<Quote> results;
    for (const auto &quote : quotes_) {
        if (quote.getYear() == year) {
            results.push_back(quote);
        }
    }

    LOG_F(INFO, "Found {} quotes from year: {}", results.size(), year);
    return results;
}

auto QuoteManager::filterQuotes(
    std::function<bool(const Quote &)> filterFunc) const -> std::vector<Quote> {
    LOG_F(INFO, "Filtering quotes with custom filter function");

    std::vector<Quote> results;
    for (const auto &quote : quotes_) {
        if (filterFunc(quote)) {
            results.push_back(quote);
        }
    }

    LOG_F(INFO, "Found {} quotes matching custom filter", results.size());
    return results;
}

auto QuoteManager::getRandomQuote() const -> std::string {
    LOG_F(INFO, "Getting a random quote");

    auto quoteOpt = getRandomQuoteObject();
    if (!quoteOpt) {
        LOG_F(WARNING, "No quotes available");
        return "";
    }

    std::string randomQuote = quoteOpt->toString();
    LOG_F(INFO, "Random quote: {}", randomQuote);
    return randomQuote;
}

auto QuoteManager::getRandomQuoteObject() const -> std::optional<Quote> {
    if (quotes_.empty()) {
        LOG_F(WARNING, "No quotes available");
        return std::nullopt;
    }

    std::random_device rd;
    int quoteId =
        utils::Random<std::mt19937, std::uniform_int_distribution<int>>(
            rd(), 0, quotes_.size() - 1)();

    return quotes_[quoteId];
}

}  // namespace atom::system
