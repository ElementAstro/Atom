/**
 * @file ascii_table_hdu.cpp
 * @brief Implementation of FITS ASCII Table Extension HDU
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "ascii_table_hdu.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace atom::image::fits {

namespace {

constexpr size_t FITS_BLOCK_SIZE = 2880;

size_t roundToBlock(size_t size) {
    return ((size + FITS_BLOCK_SIZE - 1) / FITS_BLOCK_SIZE) * FITS_BLOCK_SIZE;
}

std::string trimString(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

}  // anonymous namespace

ASCIITableHDU::ASCIITableHDU() = default;

ASCIITableHDU::ASCIITableHDU(int64_t numRows, int rowWidth)
    : numRows_(numRows), rowWidth_(rowWidth) {
    tableData_.resize(numRows * rowWidth, ' ');
}

void ASCIITableHDU::readHDU(
    std::ifstream& file,
    std::function<void(float, const std::string&)> progressCallback) {
    if (!file || !file.good()) {
        throw FileOperationException("Invalid file stream for reading table");
    }

    auto reportProgress = [&progressCallback](float p, const std::string& s) {
        if (progressCallback)
            progressCallback(p, s);
    };

    reportProgress(0.0f, "Reading ASCII table header");

    // Read header
    std::vector<char> headerData(FITSHeader::FITS_HEADER_UNIT_SIZE);
    file.read(headerData.data(), headerData.size());

    if (!file.good()) {
        throw FileOperationException("Failed to read table header");
    }

    header.deserialize(headerData);

    // Parse table dimensions
    try {
        std::string naxis1Str = header.getKeywordValue("NAXIS1");
        rowWidth_ = std::stoi(naxis1Str);

        std::string naxis2Str = header.getKeywordValue("NAXIS2");
        numRows_ = std::stoll(naxis2Str);

        std::string tfieldsStr = header.getKeywordValue("TFIELDS");
        int numCols = std::stoi(tfieldsStr);

        reportProgress(0.1f, "Parsing column definitions");

        // Parse column definitions
        columns_.resize(numCols);
        for (int i = 0; i < numCols; ++i) {
            std::string suffix = std::to_string(i + 1);

            // TFORM (required)
            std::string tformKey = "TFORM" + suffix;
            std::string format = header.getKeywordValue(tformKey);
            columns_[i] = parseFormat(format);
            columns_[i].format = format;

            // TBCOL (required)
            std::string tbcolKey = "TBCOL" + suffix;
            columns_[i].startColumn =
                std::stoi(header.getKeywordValue(tbcolKey));

            // TTYPE (optional)
            try {
                std::string ttypeKey = "TTYPE" + suffix;
                columns_[i].name = header.getKeywordValue(ttypeKey);
                // Trim quotes and whitespace
                auto& name = columns_[i].name;
                while (!name.empty() &&
                       (name.front() == '\'' || name.front() == ' ')) {
                    name.erase(0, 1);
                }
                while (!name.empty() &&
                       (name.back() == '\'' || name.back() == ' ')) {
                    name.pop_back();
                }
            } catch (...) {
                columns_[i].name = "COL" + suffix;
            }

            // TUNIT (optional)
            try {
                std::string tunitKey = "TUNIT" + suffix;
                columns_[i].unit = header.getKeywordValue(tunitKey);
            } catch (...) {
            }

            // TSCAL (optional)
            try {
                std::string tscalKey = "TSCAL" + suffix;
                columns_[i].scale = std::stod(header.getKeywordValue(tscalKey));
            } catch (...) {
                columns_[i].scale = 1.0;
            }

            // TZERO (optional)
            try {
                std::string tzeroKey = "TZERO" + suffix;
                columns_[i].zero = std::stod(header.getKeywordValue(tzeroKey));
            } catch (...) {
                columns_[i].zero = 0.0;
            }

            // TNULL (optional)
            try {
                std::string tnullKey = "TNULL" + suffix;
                columns_[i].nullString = header.getKeywordValue(tnullKey);
            } catch (...) {
            }
        }

        reportProgress(0.2f, "Reading table data");

        // Read table data
        int64_t tableSize = numRows_ * rowWidth_;
        tableData_.resize(tableSize);
        file.read(tableData_.data(), tableSize);

        if (!file.good() && file.gcount() < tableSize) {
            throw FileOperationException("Failed to read table data");
        }

        // Skip padding to next block boundary
        size_t paddedSize = roundToBlock(tableSize);
        if (paddedSize > static_cast<size_t>(tableSize)) {
            file.seekg(paddedSize - tableSize, std::ios::cur);
        }

        reportProgress(1.0f, "ASCII table read complete");

    } catch (const std::exception& e) {
        throw DataFormatException("Failed to parse ASCII table: " +
                                  std::string(e.what()));
    }
}

void ASCIITableHDU::writeHDU(std::ofstream& file) const {
    if (!file || !file.good()) {
        throw FileOperationException("Invalid file stream for writing table");
    }

    // Update header with current dimensions
    FITSHeader writeHeader = header;

    writeHeader.addKeyword("XTENSION", "'TABLE   '");
    writeHeader.addKeyword("BITPIX", "8");
    writeHeader.addKeyword("NAXIS", "2");
    writeHeader.addKeyword("NAXIS1", std::to_string(rowWidth_));
    writeHeader.addKeyword("NAXIS2", std::to_string(numRows_));
    writeHeader.addKeyword("PCOUNT", "0");
    writeHeader.addKeyword("GCOUNT", "1");
    writeHeader.addKeyword("TFIELDS", std::to_string(columns_.size()));

    // Write column definitions
    for (size_t i = 0; i < columns_.size(); ++i) {
        std::string suffix = std::to_string(i + 1);

        writeHeader.addKeyword("TFORM" + suffix, columns_[i].format);
        writeHeader.addKeyword("TBCOL" + suffix,
                               std::to_string(columns_[i].startColumn));

        if (!columns_[i].name.empty()) {
            writeHeader.addKeyword("TTYPE" + suffix,
                                   "'" + columns_[i].name + "'");
        }

        if (!columns_[i].unit.empty()) {
            writeHeader.addKeyword("TUNIT" + suffix,
                                   "'" + columns_[i].unit + "'");
        }

        if (columns_[i].scale != 1.0) {
            writeHeader.addKeyword("TSCAL" + suffix,
                                   std::to_string(columns_[i].scale));
        }

        if (columns_[i].zero != 0.0) {
            writeHeader.addKeyword("TZERO" + suffix,
                                   std::to_string(columns_[i].zero));
        }
    }

    // Serialize and write header
    auto headerData = writeHeader.serialize();
    file.write(headerData.data(), headerData.size());

    // Write table data
    file.write(tableData_.data(), tableData_.size());

    // Write padding
    size_t tableSize = tableData_.size();
    size_t paddedSize = roundToBlock(tableSize);
    if (paddedSize > tableSize) {
        std::string padding(paddedSize - tableSize, ' ');
        file.write(padding.data(), padding.size());
    }

    if (!file.good()) {
        throw FileOperationException("Failed to write ASCII table");
    }
}

bool ASCIITableHDU::isDataValid() const {
    if (numRows_ < 0 || rowWidth_ < 0) {
        return false;
    }

    if (columns_.empty()) {
        return false;
    }

    int64_t expectedSize = numRows_ * rowWidth_;
    if (static_cast<int64_t>(tableData_.size()) != expectedSize) {
        return false;
    }

    return true;
}

const ASCIIColumnDef& ASCIITableHDU::getColumn(int index) const {
    if (index < 0 || index >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }
    return columns_[index];
}

const ASCIIColumnDef& ASCIITableHDU::getColumn(const std::string& name) const {
    int idx = getColumnIndex(name);
    if (idx < 0) {
        throw std::out_of_range("Column not found: " + name);
    }
    return columns_[idx];
}

int ASCIITableHDU::getColumnIndex(const std::string& name) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool ASCIITableHDU::hasColumn(const std::string& name) const {
    return getColumnIndex(name) >= 0;
}

std::vector<std::string> ASCIITableHDU::getColumnNames() const {
    std::vector<std::string> names;
    names.reserve(columns_.size());
    for (const auto& col : columns_) {
        names.push_back(col.name);
    }
    return names;
}

int ASCIITableHDU::addColumn(const ASCIIColumnDef& def) {
    columns_.push_back(def);
    return static_cast<int>(columns_.size() - 1);
}

int ASCIITableHDU::addColumn(const std::string& name, const std::string& format,
                             int startCol, const std::string& unit) {
    ASCIIColumnDef def = parseFormat(format);
    def.name = name;
    def.unit = unit;
    def.format = format;
    def.startColumn = startCol;
    return addColumn(def);
}

void ASCIITableHDU::removeColumn(int index) {
    if (index < 0 || index >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }
    columns_.erase(columns_.begin() + index);
}

void ASCIITableHDU::addRows(int64_t count) {
    if (count <= 0)
        return;

    tableData_.resize((numRows_ + count) * rowWidth_, ' ');
    numRows_ += count;
}

void ASCIITableHDU::removeRows(int64_t firstRow, int64_t count) {
    if (firstRow < 0 || firstRow >= numRows_) {
        throw std::out_of_range("First row out of range");
    }

    count = std::min(count, numRows_ - firstRow);

    size_t startOffset = firstRow * rowWidth_;
    size_t endOffset = (firstRow + count) * rowWidth_;

    tableData_.erase(tableData_.begin() + startOffset,
                     tableData_.begin() + endOffset);

    numRows_ -= count;
}

std::vector<int64_t> ASCIITableHDU::readIntColumn(int colIndex) const {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    std::vector<int64_t> result(numRows_);
    for (int64_t row = 0; row < numRows_; ++row) {
        result[row] = readCellInt(row, colIndex);
    }
    return result;
}

std::vector<double> ASCIITableHDU::readFloatColumn(int colIndex) const {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    std::vector<double> result(numRows_);
    for (int64_t row = 0; row < numRows_; ++row) {
        result[row] = readCellDouble(row, colIndex);
    }
    return result;
}

std::vector<std::string> ASCIITableHDU::readStringColumn(int colIndex) const {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    std::vector<std::string> result(numRows_);
    for (int64_t row = 0; row < numRows_; ++row) {
        result[row] = readCellString(row, colIndex);
    }
    return result;
}

void ASCIITableHDU::writeIntColumn(int colIndex,
                                   const std::vector<int64_t>& data) {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    if (static_cast<int64_t>(data.size()) != numRows_) {
        throw DataFormatException("Data size doesn't match row count");
    }

    for (int64_t row = 0; row < numRows_; ++row) {
        writeCell(row, colIndex, data[row]);
    }
}

void ASCIITableHDU::writeFloatColumn(int colIndex,
                                     const std::vector<double>& data) {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    if (static_cast<int64_t>(data.size()) != numRows_) {
        throw DataFormatException("Data size doesn't match row count");
    }

    for (int64_t row = 0; row < numRows_; ++row) {
        writeCell(row, colIndex, data[row]);
    }
}

void ASCIITableHDU::writeStringColumn(int colIndex,
                                      const std::vector<std::string>& data) {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    if (static_cast<int64_t>(data.size()) != numRows_) {
        throw DataFormatException("Data size doesn't match row count");
    }

    for (int64_t row = 0; row < numRows_; ++row) {
        writeCell(row, colIndex, data[row]);
    }
}

ASCIICellValue ASCIITableHDU::readCell(int64_t row, int col) const {
    const auto& column = columns_[col];

    switch (column.type) {
        case ASCIIColumnType::INTEGER:
            return readCellInt(row, col);
        case ASCIIColumnType::FLOAT:
        case ASCIIColumnType::EXPONENT:
        case ASCIIColumnType::DOUBLE:
            return readCellDouble(row, col);
        case ASCIIColumnType::STRING:
        default:
            return readCellString(row, col);
    }
}

std::string ASCIITableHDU::readCellString(int64_t row, int col) const {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    size_t offset = getCellOffset(row, col);
    const auto& column = columns_[col];

    std::string value = tableData_.substr(offset, column.width);
    return trimString(value);
}

int64_t ASCIITableHDU::readCellInt(int64_t row, int col) const {
    std::string str = readCellString(row, col);

    if (str.empty()) {
        return 0;
    }

    try {
        const auto& column = columns_[col];
        int64_t value = std::stoll(str);
        return static_cast<int64_t>(value * column.scale + column.zero);
    } catch (...) {
        throw DataFormatException("Cannot parse integer: " + str);
    }
}

double ASCIITableHDU::readCellDouble(int64_t row, int col) const {
    std::string str = readCellString(row, col);

    if (str.empty()) {
        return 0.0;
    }

    // Handle FITS 'D' exponent notation
    std::replace(str.begin(), str.end(), 'D', 'E');
    std::replace(str.begin(), str.end(), 'd', 'e');

    try {
        const auto& column = columns_[col];
        double value = std::stod(str);
        return value * column.scale + column.zero;
    } catch (...) {
        throw DataFormatException("Cannot parse float: " + str);
    }
}

void ASCIITableHDU::writeCell(int64_t row, int col, int64_t value) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    std::string formatted = formatInt(value, column);

    size_t offset = getCellOffset(row, col);
    std::memcpy(tableData_.data() + offset, formatted.data(),
                std::min(formatted.size(), static_cast<size_t>(column.width)));
}

void ASCIITableHDU::writeCell(int64_t row, int col, double value) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    std::string formatted = formatFloat(value, column);

    size_t offset = getCellOffset(row, col);
    std::memcpy(tableData_.data() + offset, formatted.data(),
                std::min(formatted.size(), static_cast<size_t>(column.width)));
}

void ASCIITableHDU::writeCell(int64_t row, int col, const std::string& value) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    std::string formatted = formatString(value, column);

    size_t offset = getCellOffset(row, col);
    std::memcpy(tableData_.data() + offset, formatted.data(),
                std::min(formatted.size(), static_cast<size_t>(column.width)));
}

std::string ASCIITableHDU::getRow(int64_t row) const {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }

    size_t offset = row * rowWidth_;
    return tableData_.substr(offset, rowWidth_);
}

void ASCIITableHDU::setRow(int64_t row, const std::string& data) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }

    if (static_cast<int>(data.size()) != rowWidth_) {
        throw DataFormatException("Row data size doesn't match row width");
    }

    size_t offset = row * rowWidth_;
    std::memcpy(tableData_.data() + offset, data.data(), rowWidth_);
}

void ASCIITableHDU::clear() {
    numRows_ = 0;
    rowWidth_ = 0;
    columns_.clear();
    tableData_.clear();
}

ASCIIColumnDef ASCIITableHDU::parseFormat(const std::string& format) const {
    ASCIIColumnDef def;

    if (format.empty()) {
        def.type = ASCIIColumnType::UNKNOWN;
        return def;
    }

    // Parse format: type + width [. precision]
    // Examples: I10, F12.4, E15.7, D25.16, A20

    size_t typePos = 0;
    while (typePos < format.size() && !std::isalpha(format[typePos])) {
        ++typePos;
    }

    if (typePos >= format.size()) {
        def.type = ASCIIColumnType::UNKNOWN;
        return def;
    }

    char typeCode = std::toupper(format[typePos]);

    switch (typeCode) {
        case 'I':
            def.type = ASCIIColumnType::INTEGER;
            break;
        case 'F':
            def.type = ASCIIColumnType::FLOAT;
            break;
        case 'E':
            def.type = ASCIIColumnType::EXPONENT;
            break;
        case 'D':
            def.type = ASCIIColumnType::DOUBLE;
            break;
        case 'A':
            def.type = ASCIIColumnType::STRING;
            break;
        default:
            def.type = ASCIIColumnType::UNKNOWN;
            break;
    }

    // Parse width and precision
    std::string rest = format.substr(typePos + 1);
    size_t dotPos = rest.find('.');

    if (dotPos != std::string::npos) {
        def.width = std::stoi(rest.substr(0, dotPos));
        def.precision = std::stoi(rest.substr(dotPos + 1));
    } else if (!rest.empty()) {
        def.width = std::stoi(rest);
        def.precision = 0;
    }

    return def;
}

std::string ASCIITableHDU::formatInt(int64_t value,
                                     const ASCIIColumnDef& col) const {
    std::ostringstream oss;
    oss << std::setw(col.width) << std::right << value;
    return oss.str();
}

std::string ASCIITableHDU::formatFloat(double value,
                                       const ASCIIColumnDef& col) const {
    std::ostringstream oss;

    switch (col.type) {
        case ASCIIColumnType::FLOAT:
            oss << std::setw(col.width) << std::fixed
                << std::setprecision(col.precision) << value;
            break;
        case ASCIIColumnType::EXPONENT:
            oss << std::setw(col.width) << std::scientific
                << std::setprecision(col.precision) << value;
            break;
        case ASCIIColumnType::DOUBLE:
            oss << std::setw(col.width) << std::scientific
                << std::setprecision(col.precision) << value;
            break;
        default:
            oss << std::setw(col.width) << value;
            break;
    }

    std::string result = oss.str();

    // For DOUBLE type, replace 'e' with 'D'
    if (col.type == ASCIIColumnType::DOUBLE) {
        std::replace(result.begin(), result.end(), 'e', 'D');
        std::replace(result.begin(), result.end(), 'E', 'D');
    }

    return result;
}

std::string ASCIITableHDU::formatString(const std::string& value,
                                        const ASCIIColumnDef& col) const {
    std::string result = value;

    if (static_cast<int>(result.size()) < col.width) {
        // Pad with spaces on the right
        result.append(col.width - result.size(), ' ');
    } else if (static_cast<int>(result.size()) > col.width) {
        // Truncate
        result = result.substr(0, col.width);
    }

    return result;
}

size_t ASCIITableHDU::getCellOffset(int64_t row, int col) const {
    // startColumn is 1-indexed in FITS
    return row * rowWidth_ + (columns_[col].startColumn - 1);
}

// Utility functions

std::tuple<ASCIIColumnType, int, int> parseASCIITFORM(
    const std::string& format) {
    if (format.empty()) {
        return {ASCIIColumnType::UNKNOWN, 0, 0};
    }

    size_t typePos = 0;
    while (typePos < format.size() && !std::isalpha(format[typePos])) {
        ++typePos;
    }

    if (typePos >= format.size()) {
        return {ASCIIColumnType::UNKNOWN, 0, 0};
    }

    ASCIIColumnType type;
    char typeCode = std::toupper(format[typePos]);

    switch (typeCode) {
        case 'I':
            type = ASCIIColumnType::INTEGER;
            break;
        case 'F':
            type = ASCIIColumnType::FLOAT;
            break;
        case 'E':
            type = ASCIIColumnType::EXPONENT;
            break;
        case 'D':
            type = ASCIIColumnType::DOUBLE;
            break;
        case 'A':
            type = ASCIIColumnType::STRING;
            break;
        default:
            type = ASCIIColumnType::UNKNOWN;
            break;
    }

    std::string rest = format.substr(typePos + 1);
    size_t dotPos = rest.find('.');

    int width = 0;
    int precision = 0;

    if (dotPos != std::string::npos) {
        width = std::stoi(rest.substr(0, dotPos));
        precision = std::stoi(rest.substr(dotPos + 1));
    } else if (!rest.empty()) {
        width = std::stoi(rest);
    }

    return {type, width, precision};
}

std::string asciiColumnTypeToString(ASCIIColumnType type) {
    switch (type) {
        case ASCIIColumnType::INTEGER:
            return "Integer";
        case ASCIIColumnType::FLOAT:
            return "Float";
        case ASCIIColumnType::EXPONENT:
            return "Exponent";
        case ASCIIColumnType::DOUBLE:
            return "Double";
        case ASCIIColumnType::STRING:
            return "String";
        case ASCIIColumnType::UNKNOWN:
            return "Unknown";
    }
    return "Unknown";
}

}  // namespace atom::image::fits
