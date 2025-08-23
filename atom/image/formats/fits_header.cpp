#include "fits_header.hpp"

#include <algorithm>
#include <cstring>

// Implementation of the KeywordRecord constructor
FITSHeader::KeywordRecord::KeywordRecord(std::string_view kw,
                                         std::string_view val) noexcept {
    // Initialize with spaces
    std::fill(keyword.begin(), keyword.end(), ' ');
    std::fill(value.begin(), value.end(), ' ');

    // Copy keyword (up to 8 characters)
    const size_t kwLen = std::min(kw.size(), keyword.size());
    std::copy(kw.begin(), kw.begin() + kwLen, keyword.begin());

    // Copy value (up to 72 characters)
    const size_t valLen = std::min(val.size(), value.size());
    std::copy(val.begin(), val.begin() + valLen, value.begin());
}

// Construct FITSHeader from data
FITSHeader::FITSHeader(const std::vector<char>& data) {
    try {
        deserialize(data);
    } catch (const FITSHeaderErrors::BaseException& e) {
        throw FITSHeaderErrors::DeserializationException(e.what());
    } catch (const std::exception& e) {
        throw FITSHeaderErrors::DeserializationException(
            std::string("Unexpected error: ") + e.what());
    }
}

// Find keyword index, using cache to optimize performance
size_t FITSHeader::findKeywordIndex(std::string_view keyword) const noexcept {
    // Try to find from cache
    std::string keywordStr(keyword);
    if (!keywordCache.empty()) {
        auto it = keywordCache.find(keywordStr);
        if (it != keywordCache.end()) {
            return it->second;
        }
    }

    // If cache miss, perform linear search
    for (size_t i = 0; i < records.size(); ++i) {
        std::string_view recordKeyword(records[i].keyword.data(), 8);
        // Remove trailing spaces
        size_t kwLen = recordKeyword.find_first_of(' ');
        if (kwLen == std::string_view::npos) {
            kwLen = 8;
        }

        if (recordKeyword.substr(0, kwLen) == keyword) {
            // Add to cache
            if (keywordCache.size() < 1000) {  // Limit cache size
                keywordCache[keywordStr] = i;
            }
            return i;
        }
    }

    return std::string::npos;
}

// Update keyword cache
void FITSHeader::updateCache() const noexcept {
    keywordCache.clear();
    // Only cache the first 1000 records to prevent excessive cache size
    const size_t maxCacheSize =
        std::min(records.size(), static_cast<size_t>(1000));

    for (size_t i = 0; i < maxCacheSize; ++i) {
        std::string_view recordKeyword(records[i].keyword.data(), 8);
        size_t kwLen = recordKeyword.find_first_of(' ');
        if (kwLen == std::string_view::npos) {
            kwLen = 8;
        }

        std::string keyword(recordKeyword.substr(0, kwLen));
        keywordCache[keyword] = i;
    }
}

// Add keyword, update if it already exists
void FITSHeader::addKeyword(std::string_view keyword,
                            std::string_view value) noexcept {
    size_t index = findKeywordIndex(keyword);

    if (index != std::string::npos) {
        // Update existing keyword
        std::fill(records[index].value.begin(), records[index].value.end(),
                  ' ');
        const size_t valLen =
            std::min(value.size(), records[index].value.size());
        std::copy(value.begin(), value.begin() + valLen,
                  records[index].value.begin());
    } else {
        // Add new keyword
        records.emplace_back(keyword, value);
        // Add to cache
        if (keywordCache.size() < 1000) {
            keywordCache[std::string(keyword)] = records.size() - 1;
        }
    }
}

// Add comment
void FITSHeader::addComment(std::string_view comment) noexcept {
    // Ensure comment fits within the 72-character limit
    std::array<char, 72> commentField{};
    std::fill(commentField.begin(), commentField.end(), ' ');
    const size_t commentLen = std::min(comment.size(), commentField.size());
    std::copy(comment.begin(), comment.begin() + commentLen,
              commentField.begin());

    // Add comment as a special record using the "COMMENT" keyword
    records.emplace_back(
        "COMMENT", std::string_view(commentField.data(), commentField.size()));

    // Comments are not added to the cache
}

// Get keyword value
std::string FITSHeader::getKeywordValue(std::string_view keyword) const {
    size_t index = findKeywordIndex(keyword);

    if (index != std::string::npos) {
        std::string result(records[index].value.data(),
                           records[index].value.size());
        // Remove trailing spaces
        auto lastNonSpace = result.find_last_not_of(' ');
        if (lastNonSpace != std::string::npos) {
            result.erase(lastNonSpace + 1);
        } else {
            result.clear();  // Case where it's all spaces
        }
        return result;
    }

    throw FITSHeaderErrors::KeywordNotFoundException(std::string(keyword));
}

// Try to get keyword value, does not throw exceptions
std::optional<std::string> FITSHeader::tryGetKeywordValue(
    std::string_view keyword) const noexcept {
    try {
        return getKeywordValue(keyword);
    } catch (const FITSHeaderErrors::KeywordNotFoundException&) {
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

// Get all comments
std::vector<std::string> FITSHeader::getComments() const {
    std::vector<std::string> comments;

    for (const auto& record : records) {
        std::string_view recordKeyword(record.keyword.data(), 8);
        if (recordKeyword.substr(0, 7) == "COMMENT") {
            std::string comment(record.value.data(), record.value.size());
            // Remove trailing spaces
            auto lastNonSpace = comment.find_last_not_of(' ');
            if (lastNonSpace != std::string::npos) {
                comment.erase(lastNonSpace + 1);
            } else {
                comment.clear();  // Case where it's all spaces
            }
            comments.push_back(comment);
        }
    }

    return comments;
}

// Serialize FITS header to byte vector
std::vector<char> FITSHeader::serialize() const {
    // Calculate required bytes, including END keyword
    size_t recordCount = records.size();
    bool hasEnd = false;

    // Check if END keyword already exists
    for (const auto& record : records) {
        std::string_view recordKeyword(record.keyword.data(), 8);
        if (recordKeyword.substr(0, 3) == "END") {
            hasEnd = true;
            break;
        }
    }

    // If no END keyword, need extra space
    if (!hasEnd) {
        recordCount++;
    }

    // Calculate required units (each unit is 2880 bytes)
    size_t totalSize = recordCount * FITS_HEADER_CARD_SIZE;
    size_t unitCount =
        (totalSize + FITS_HEADER_UNIT_SIZE - 1) / FITS_HEADER_UNIT_SIZE;
    size_t paddedSize = unitCount * FITS_HEADER_UNIT_SIZE;

    // Create output buffer and fill with spaces
    std::vector<char> data(paddedSize, ' ');

    // Copy all records
    size_t pos = 0;
    for (const auto& record : records) {
        // Copy keyword (8 bytes)
        std::copy(record.keyword.begin(), record.keyword.end(),
                  data.begin() + pos);
        pos += record.keyword.size();

        // Copy value (72 bytes)
        std::copy(record.value.begin(), record.value.end(), data.begin() + pos);
        pos += record.value.size();
    }

    // Add END keyword if needed
    if (!hasEnd) {
        KeywordRecord endRecord("END", "");

        std::copy(endRecord.keyword.begin(), endRecord.keyword.end(),
                  data.begin() + pos);
        pos += endRecord.keyword.size();

        std::copy(endRecord.value.begin(), endRecord.value.end(),
                  data.begin() + pos);
        pos += endRecord.value.size();
    }

    return data;
}

// Deserialize byte vector into FITS header
void FITSHeader::deserialize(const std::vector<char>& data) {
    if (data.empty()) {
        throw FITSHeaderErrors::InvalidDataException("Empty data");
    }

    if (data.size() % FITS_HEADER_CARD_SIZE != 0) {
        throw FITSHeaderErrors::InvalidDataException(
            "Invalid size: not a multiple of 80 bytes");
    }

    records.clear();
    keywordCache.clear();

    for (size_t pos = 0; pos < data.size(); pos += FITS_HEADER_CARD_SIZE) {
        if (pos + FITS_HEADER_CARD_SIZE > data.size()) {
            break;  // Prevent out-of-bounds access
        }

        // Extract keyword
        std::array<char, 8> keyword{};
        std::copy(data.begin() + pos, data.begin() + pos + 8, keyword.begin());

        // Check if it's the END keyword
        std::string_view keywordView(keyword.data(), 8);
        if (keywordView.substr(0, 3) == "END") {
            break;  // End of reading
        }

        // Extract value
        std::array<char, 72> value{};
        std::copy(data.begin() + pos + 8, data.begin() + pos + 80,
                  value.begin());

        KeywordRecord record;
        record.keyword = keyword;
        record.value = value;
        records.push_back(record);
    }

    // Build cache
    updateCache();
}

// Check if keyword exists
bool FITSHeader::hasKeyword(std::string_view keyword) const noexcept {
    return findKeywordIndex(keyword) != std::string::npos;
}

// Remove keyword
bool FITSHeader::removeKeyword(std::string_view keyword) noexcept {
    size_t index = findKeywordIndex(keyword);

    if (index != std::string::npos) {
        records.erase(records.begin() + index);
        // Update cache
        updateCache();
        return true;
    }

    return false;
}

// Clear all comments
size_t FITSHeader::clearComments() noexcept {
    size_t initialSize = records.size();

    records.erase(std::remove_if(records.begin(), records.end(),
                                 [](const KeywordRecord& record) {
                                     std::string_view recordKeyword(
                                         record.keyword.data(), 8);
                                     return recordKeyword.substr(0, 7) ==
                                            "COMMENT";
                                 }),
                  records.end());

    // If records were removed, update the cache
    size_t removed = initialSize - records.size();
    if (removed > 0) {
        updateCache();
    }

    return removed;
}

// Get all keywords
std::vector<std::string> FITSHeader::getAllKeywords() const {
    std::vector<std::string> keywords;
    keywords.reserve(records.size());

    for (const auto& record : records) {
        std::string keyword(record.keyword.data(), record.keyword.size());
        // Remove trailing spaces
        auto lastNonSpace = keyword.find_last_not_of(' ');
        if (lastNonSpace != std::string::npos) {
            keyword.erase(lastNonSpace + 1);
        } else {
            keyword.clear();  // Case where it's all spaces
        }

        if (!keyword.empty()) {
            keywords.push_back(keyword);
        }
    }

    return keywords;
}