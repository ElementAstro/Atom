#include "fits_header.hpp"
#include <algorithm>
#include <cstring>

FITSHeader::KeywordRecord::KeywordRecord(std::string_view kw,
                                         std::string_view val) noexcept {
    // Initialize with zeros
    std::fill(keyword.begin(), keyword.end(), ' ');
    std::fill(value.begin(), value.end(), ' ');

    // Copy keyword (up to 8 chars)
    const size_t kwLen = std::min(kw.size(), keyword.size());
    std::copy(kw.begin(), kw.begin() + kwLen, keyword.begin());

    // Copy value (up to 72 chars)
    const size_t valLen = std::min(val.size(), value.size());
    std::copy(val.begin(), val.begin() + valLen, value.begin());
}

void FITSHeader::addKeyword(std::string_view keyword,
                            std::string_view value) noexcept {
    // Check if keyword already exists, update if it does
    for (auto& record : records) {
        std::string_view existing(record.keyword.data(), 8);
        if (existing.substr(0, keyword.size()) == keyword) {
            // Update existing keyword
            std::fill(record.value.begin(), record.value.end(), ' ');
            const size_t valLen = std::min(value.size(), record.value.size());
            std::copy(value.begin(), value.begin() + valLen,
                      record.value.begin());
            return;
        }
    }

    // Add new keyword
    records.emplace_back(keyword, value);
}

void FITSHeader::addComment(std::string_view comment) noexcept {
    // Ensure comment fits within 72 characters
    std::array<char, 72> commentField{};
    std::fill(commentField.begin(), commentField.end(), ' ');
    const size_t commentLen = std::min(comment.size(), commentField.size());
    std::copy(comment.begin(), comment.begin() + commentLen,
              commentField.begin());

    // Add comment as a special record with "COMMENT" keyword
    records.emplace_back(
        "COMMENT", std::string_view(commentField.data(), commentField.size()));
}

std::string FITSHeader::getKeywordValue(std::string_view keyword) const {
    for (const auto& record : records) {
        std::string_view recordKeyword(record.keyword.data(), 8);
        // Trim trailing spaces
        size_t kwLen = recordKeyword.find_first_of(' ');
        if (kwLen == std::string_view::npos)
            kwLen = 8;

        if (recordKeyword.substr(0, kwLen) == keyword) {
            std::string result(record.value.data(), record.value.size());
            // Trim trailing spaces
            result.erase(result.find_last_not_of(' ') + 1);
            return result;
        }
    }

    throw FITSHeaderException("Keyword not found: " + std::string(keyword));
}

std::vector<std::string> FITSHeader::getComments() const {
    std::vector<std::string> comments;
    for (const auto& record : records) {
        std::string_view recordKeyword(record.keyword.data(), 8);
        if (recordKeyword.substr(0, 7) == "COMMENT") {
            std::string comment(record.value.data(), record.value.size());
            // Trim trailing spaces
            comment.erase(comment.find_last_not_of(' ') + 1);
            comments.push_back(comment);
        }
    }
    return comments;
}

std::vector<char> FITSHeader::serialize() const {
    std::vector<char> data(FITS_HEADER_UNIT_SIZE, ' ');

    size_t pos = 0;
    for (const auto& record : records) {
        if (pos + FITS_HEADER_CARD_SIZE > data.size()) {
            // Extend the data vector if needed
            data.resize(data.size() + FITS_HEADER_UNIT_SIZE, ' ');
        }

        // Copy keyword (8 bytes)
        std::copy(record.keyword.begin(), record.keyword.end(),
                  data.begin() + pos);
        pos += record.keyword.size();

        // Copy value (72 bytes)
        std::copy(record.value.begin(), record.value.end(), data.begin() + pos);
        pos += record.value.size();
    }

    // Ensure END keyword is always present
    if (std::none_of(records.begin(), records.end(),
                     [](const KeywordRecord& record) {
                         return std::string_view(record.keyword.data(), 8)
                                    .substr(0, 3) == "END";
                     })) {
        // 修复：使用显式创建的KeywordRecord对象而不是使用emplace_back
        KeywordRecord endRecord("END", "");
        // records.push_back(endRecord);

        // 将END记录添加到序列化数据中
        if (pos + FITS_HEADER_CARD_SIZE <= data.size()) {
            std::copy(endRecord.keyword.begin(), endRecord.keyword.end(),
                      data.begin() + pos);
            pos += endRecord.keyword.size();

            std::copy(endRecord.value.begin(), endRecord.value.end(),
                      data.begin() + pos);
            pos += endRecord.value.size();
        } else {
            // 扩展数据如果需要
            data.resize(pos + FITS_HEADER_CARD_SIZE, ' ');
            std::copy(endRecord.keyword.begin(), endRecord.keyword.end(),
                      data.begin() + pos);
            pos += endRecord.keyword.size();

            std::copy(endRecord.value.begin(), endRecord.value.end(),
                      data.begin() + pos);
            pos += endRecord.value.size();
        }
    }

    return data;
}

void FITSHeader::deserialize(const std::vector<char>& data) {
    if (data.size() % FITS_HEADER_CARD_SIZE != 0) {
        throw FITSHeaderException("Invalid FITS header data size");
    }

    records.clear();

    for (size_t pos = 0; pos < data.size(); pos += FITS_HEADER_CARD_SIZE) {
        if (pos + FITS_HEADER_CARD_SIZE > data.size()) {
            break;
        }

        // Extract keyword
        std::array<char, 8> keyword{};
        std::copy(data.begin() + pos, data.begin() + pos + 8, keyword.begin());

        // Check if this is the END keyword
        std::string keywordStr(keyword.data(), 8);
        if (keywordStr.substr(0, 3) == "END") {
            break;
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
}

bool FITSHeader::hasKeyword(std::string_view keyword) const noexcept {
    for (const auto& record : records) {
        std::string_view recordKeyword(record.keyword.data(), 8);
        // Trim trailing spaces
        size_t kwLen = recordKeyword.find_first_of(' ');
        if (kwLen == std::string_view::npos)
            kwLen = 8;

        if (recordKeyword.substr(0, kwLen) == keyword) {
            return true;
        }
    }
    return false;
}

void FITSHeader::removeKeyword(std::string_view keyword) noexcept {
    records.erase(
        std::remove_if(records.begin(), records.end(),
                       [keyword](const KeywordRecord& record) {
                           std::string_view recordKeyword(record.keyword.data(),
                                                          8);
                           // Trim trailing spaces
                           size_t kwLen = recordKeyword.find_first_of(' ');
                           if (kwLen == std::string_view::npos)
                               kwLen = 8;
                           return recordKeyword.substr(0, kwLen) == keyword;
                       }),
        records.end());
}

void FITSHeader::clearComments() noexcept {
    records.erase(std::remove_if(records.begin(), records.end(),
                                 [](const KeywordRecord& record) {
                                     std::string_view recordKeyword(
                                         record.keyword.data(), 8);
                                     return recordKeyword.substr(0, 7) ==
                                            "COMMENT";
                                 }),
                  records.end());
}

std::vector<std::string> FITSHeader::getAllKeywords() const {
    std::vector<std::string> keywords;
    keywords.reserve(records.size());

    for (const auto& record : records) {
        std::string keyword(record.keyword.data(), record.keyword.size());
        // Trim trailing spaces
        keyword.erase(keyword.find_last_not_of(' ') + 1);
        keywords.push_back(keyword);
    }

    return keywords;
}