/**
 * @file fits_header.hpp
 * @brief FITS file header handling functionality
 *
 * This file provides classes and functions for handling FITS (Flexible Image
 * Transport System) file headers, which contain metadata about astronomical
 * image data.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_IMAGE_FITS_HEADER_HPP
#define ATOM_IMAGE_FITS_HEADER_HPP

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>

/**
 * @namespace FITSHeaderErrors
 * @brief Namespace containing exceptions for FITS header operations
 */
namespace FITSHeaderErrors {

/**
 * @class BaseException
 * @brief Base exception class for all FITS header exceptions
 */
class BaseException : public std::runtime_error {
public:
    /**
     * @brief Constructor with error message
     * @param message The error message describing the exception
     */
    explicit BaseException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @class KeywordNotFoundException
 * @brief Exception thrown when a requested keyword is not found
 */
class KeywordNotFoundException : public BaseException {
public:
    /**
     * @brief Constructor with the name of the keyword not found
     * @param keyword The name of the keyword that was not found
     */
    explicit KeywordNotFoundException(const std::string& keyword)
        : BaseException("Keyword not found: " + keyword), keyword_(keyword) {}

    /**
     * @brief Get the name of the keyword that was not found
     * @return The name of the missing keyword
     */
    [[nodiscard]] const std::string& getKeyword() const noexcept {
        return keyword_;
    }

private:
    std::string keyword_;
};

/**
 * @class InvalidDataException
 * @brief Exception thrown when FITS data is malformed or invalid
 */
class InvalidDataException : public BaseException {
public:
    /**
     * @brief Constructor with error message
     * @param message The error message describing the exception
     */
    explicit InvalidDataException(const std::string& message)
        : BaseException("Invalid FITS data: " + message) {}
};

/**
 * @class DeserializationException
 * @brief Exception thrown during header deserialization errors
 */
class DeserializationException : public BaseException {
public:
    /**
     * @brief Constructor with error message
     * @param message The error message describing the exception
     */
    explicit DeserializationException(const std::string& message)
        : BaseException("FITS header deserialization error: " + message) {}
};

} // namespace FITSHeaderErrors

// 保持向后兼容
using FITSHeaderException = FITSHeaderErrors::BaseException;

/**
 * @class FITSHeader
 * @brief Class for handling FITS file headers
 *
 * Provides functionality to create, modify, and access FITS header data,
 * following the FITS standard for astronomical data storage.
 */
class FITSHeader {
public:
    /**
     * @brief Size of a complete FITS header unit in bytes
     *
     * According to the FITS standard, a header unit must be a multiple of 2880
     * bytes.
     */
    static constexpr int FITS_HEADER_UNIT_SIZE = 2880;

    /**
     * @brief Size of a FITS header card (single keyword record) in bytes
     *
     * Each FITS keyword record consists of exactly 80 bytes.
     */
    static constexpr int FITS_HEADER_CARD_SIZE = 80;

    /**
     * @struct KeywordRecord
     * @brief Structure representing a single FITS header keyword record
     *
     * Each FITS header consists of keyword records containing a keyword and its
     * value. The keyword is limited to 8 characters, and the value (including
     * any comments) is limited to 72 characters, for a total of 80 bytes per
     * record.
     */
    struct KeywordRecord {
        std::array<char, 8> keyword{}; /**< The keyword name (8 bytes) */
        std::array<char, 72>
            value{}; /**< The keyword value and comment (72 bytes) */

        /**
         * @brief Default constructor
         */
        KeywordRecord() = default;

        /**
         * @brief Constructor with keyword and value
         * @param kw The keyword (up to 8 characters)
         * @param val The value and optional comment (up to 72 characters)
         */
        KeywordRecord(std::string_view kw, std::string_view val) noexcept;
    };

    /**
     * @brief Default constructor
     */
    FITSHeader() = default;

    /**
     * @brief Construct a FITSHeader from raw data
     * @param data The raw FITS header data
     * @throws FITSHeaderErrors::DeserializationException if deserialization fails
     */
    explicit FITSHeader(const std::vector<char>& data);

    /**
     * @brief Adds a keyword with its value to the FITS header
     *
     * If the keyword already exists, its value will be updated.
     *
     * @param keyword The keyword name (up to 8 characters)
     * @param value The value to associate with the keyword
     */
    void addKeyword(std::string_view keyword, std::string_view value) noexcept;

    /**
     * @brief Gets the value associated with a keyword
     *
     * @param keyword The keyword to look up
     * @return The value associated with the keyword as a string
     * @throws FITSHeaderErrors::KeywordNotFoundException if the keyword is not found
     */
    [[nodiscard]] std::string getKeywordValue(std::string_view keyword) const;

    /**
     * @brief Tries to get the value associated with a keyword
     *
     * @param keyword The keyword to look up
     * @return An optional containing the value if the keyword exists, or empty if not found
     */
    [[nodiscard]] std::optional<std::string> tryGetKeywordValue(std::string_view keyword) const noexcept;

    /**
     * @brief Serializes the FITS header to a byte vector
     *
     * Converts the header to a vector of bytes that can be written to a file.
     * The size of the vector will be a multiple of FITS_HEADER_UNIT_SIZE.
     *
     * @return A vector of bytes representing the FITS header
     */
    [[nodiscard]] std::vector<char> serialize() const;

    /**
     * @brief Deserializes a byte vector into a FITS header
     *
     * Parses a byte vector containing FITS header data and populates this
     * object.
     *
     * @param data The vector of bytes to parse
     * @throws FITSHeaderErrors::DeserializationException if the data is invalid
     * @throws FITSHeaderErrors::InvalidDataException if the data format is wrong
     */
    void deserialize(const std::vector<char>& data);

    /**
     * @brief Checks if a keyword exists in the header
     *
     * @param keyword The keyword to check
     * @return true if the keyword exists, false otherwise
     */
    [[nodiscard]] bool hasKeyword(std::string_view keyword) const noexcept;

    /**
     * @brief Removes a keyword and its value from the header
     *
     * @param keyword The keyword to remove
     * @return true if the keyword was removed, false if it didn't exist
     */
    bool removeKeyword(std::string_view keyword) noexcept;

    /**
     * @brief Gets a list of all keywords in the header
     *
     * @return A vector containing all keywords as strings
     */
    [[nodiscard]] std::vector<std::string> getAllKeywords() const;

    /**
     * @brief Adds a comment to the FITS header
     *
     * Comments are stored as special COMMENT keyword records.
     *
     * @param comment The comment text to add
     */
    void addComment(std::string_view comment) noexcept;

    /**
     * @brief Gets all comments from the FITS header
     *
     * @return A vector containing all comments as strings
     */
    [[nodiscard]] std::vector<std::string> getComments() const;

    /**
     * @brief Removes all comments from the header
     * 
     * @return The number of comments removed
     */
    size_t clearComments() noexcept;

    /**
     * @brief Get the number of records in the header
     * 
     * @return The number of keyword records
     */
    [[nodiscard]] size_t size() const noexcept { return records.size(); }

    /**
     * @brief Check if the header is empty
     * 
     * @return true if there are no records, false otherwise
     */
    [[nodiscard]] bool empty() const noexcept { return records.empty(); }

    /**
     * @brief Clear all records from the header
     */
    void clear() noexcept { records.clear(); keywordCache.clear(); }

private:
    std::vector<KeywordRecord> records; /**< Storage for all keyword records */
    mutable std::unordered_map<std::string, size_t> keywordCache; /**< Cache for keyword lookups */

    /**
     * @brief Updates the keyword cache after modifications
     */
    void updateCache() const noexcept;

    /**
     * @brief Finds a keyword in the records
     * 
     * @param keyword The keyword to find
     * @return The index of the keyword record, or std::string::npos if not found
     */
    [[nodiscard]] size_t findKeywordIndex(std::string_view keyword) const noexcept;
};

#endif  // ATOM_IMAGE_FITS_HEADER_HPP