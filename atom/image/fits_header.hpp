#ifndef ATOM_IMAGE_FITS_HEADER_HPP
#define ATOM_IMAGE_FITS_HEADER_HPP

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class FITSHeaderException : public std::runtime_error {
public:
    explicit FITSHeaderException(const std::string& message)
        : std::runtime_error(message) {}
};

class FITSHeader {
public:
    static constexpr int FITS_HEADER_UNIT_SIZE = 2880;
    static constexpr int FITS_HEADER_CARD_SIZE = 80;

    struct KeywordRecord {
        std::array<char, 8> keyword{};
        std::array<char, 72> value{};

        // Helper constructor for easy initialization
        KeywordRecord() = default;
        KeywordRecord(std::string_view kw, std::string_view val) noexcept;
    };

    void addKeyword(std::string_view keyword, std::string_view value) noexcept;
    [[nodiscard]] std::string getKeywordValue(std::string_view keyword) const;
    [[nodiscard]] std::vector<char> serialize() const;
    void deserialize(const std::vector<char>& data);

    // Additional helper methods
    [[nodiscard]] bool hasKeyword(std::string_view keyword) const noexcept;
    void removeKeyword(std::string_view keyword) noexcept;
    [[nodiscard]] std::vector<std::string> getAllKeywords() const;

private:
    std::vector<KeywordRecord> records;
};

#endif
