/**
 * @file binary_table_hdu.cpp
 * @brief Implementation of FITS Binary Table Extension HDU
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "binary_table_hdu.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstring>
#include <stdexcept>

namespace atom::image::fits {

namespace {

constexpr size_t FITS_BLOCK_SIZE = 2880;

size_t roundToBlock(size_t size) {
    return ((size + FITS_BLOCK_SIZE - 1) / FITS_BLOCK_SIZE) * FITS_BLOCK_SIZE;
}

}  // anonymous namespace

BinaryTableHDU::BinaryTableHDU() = default;

BinaryTableHDU::BinaryTableHDU(int64_t numRows, int numCols)
    : numRows_(numRows) {
    columns_.reserve(numCols);
}

void BinaryTableHDU::readHDU(
    std::ifstream& file,
    std::function<void(float, const std::string&)> progressCallback) {
    if (!file || !file.good()) {
        throw FileOperationException("Invalid file stream for reading table");
    }

    auto reportProgress = [&progressCallback](float p, const std::string& s) {
        if (progressCallback)
            progressCallback(p, s);
    };

    reportProgress(0.0f, "Reading binary table header");

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
        rowWidth_ = std::stoll(naxis1Str);

        std::string naxis2Str = header.getKeywordValue("NAXIS2");
        numRows_ = std::stoll(naxis2Str);

        std::string tfieldsStr = header.getKeywordValue("TFIELDS");
        int numCols = std::stoi(tfieldsStr);

        // Get heap size (PCOUNT)
        try {
            std::string pcountStr = header.getKeywordValue("PCOUNT");
            heapSize_ = std::stoll(pcountStr);
        } catch (...) {
            heapSize_ = 0;
        }

        // Get heap offset (THEAP)
        try {
            std::string theapStr = header.getKeywordValue("THEAP");
            heapOffset_ = std::stoll(theapStr);
        } catch (...) {
            heapOffset_ = numRows_ * rowWidth_;
        }

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
                columns_[i].nullValue = header.getKeywordValue(tnullKey);
            } catch (...) {
            }

            // TDISP (optional)
            try {
                std::string tdispKey = "TDISP" + suffix;
                columns_[i].displayFormat = header.getKeywordValue(tdispKey);
            } catch (...) {
            }

            // TDIM (optional - for multidimensional arrays)
            try {
                std::string tdimKey = "TDIM" + suffix;
                std::string tdim = header.getKeywordValue(tdimKey);
                // Parse (dim1,dim2,...) format
                if (!tdim.empty() && tdim.front() == '(') {
                    tdim = tdim.substr(1);
                    if (!tdim.empty() && tdim.back() == ')') {
                        tdim.pop_back();
                    }
                    size_t pos = 0;
                    while (pos < tdim.size()) {
                        size_t comma = tdim.find(',', pos);
                        if (comma == std::string::npos)
                            comma = tdim.size();
                        std::string dimStr = tdim.substr(pos, comma - pos);
                        columns_[i].dimensions.push_back(std::stoll(dimStr));
                        pos = comma + 1;
                    }
                }
            } catch (...) {
            }
        }

        updateColumnOffsets();

        reportProgress(0.2f, "Reading table data");

        // Read main table data
        int64_t tableSize = numRows_ * rowWidth_;
        tableData_.resize(tableSize);
        file.read(tableData_.data(), tableSize);

        if (!file.good() && file.gcount() < tableSize) {
            throw FileOperationException("Failed to read table data");
        }

        reportProgress(0.8f, "Reading heap data");

        // Read heap if present
        if (heapSize_ > 0) {
            heapData_.resize(heapSize_);
            file.read(heapData_.data(), heapSize_);
        }

        // Skip padding to next block boundary
        size_t totalDataSize = tableSize + heapSize_;
        size_t paddedSize = roundToBlock(totalDataSize);
        if (paddedSize > totalDataSize) {
            file.seekg(paddedSize - totalDataSize, std::ios::cur);
        }

        reportProgress(1.0f, "Binary table read complete");

    } catch (const std::exception& e) {
        throw DataFormatException("Failed to parse binary table: " +
                                  std::string(e.what()));
    }
}

void BinaryTableHDU::writeHDU(std::ofstream& file) const {
    if (!file || !file.good()) {
        throw FileOperationException("Invalid file stream for writing table");
    }

    // Update header with current dimensions
    FITSHeader writeHeader = header;

    writeHeader.addKeyword("XTENSION", "'BINTABLE'");
    writeHeader.addKeyword("BITPIX", "8");
    writeHeader.addKeyword("NAXIS", "2");
    writeHeader.addKeyword("NAXIS1", std::to_string(rowWidth_));
    writeHeader.addKeyword("NAXIS2", std::to_string(numRows_));
    writeHeader.addKeyword("PCOUNT", std::to_string(heapSize_));
    writeHeader.addKeyword("GCOUNT", "1");
    writeHeader.addKeyword("TFIELDS", std::to_string(columns_.size()));

    // Write column definitions
    for (size_t i = 0; i < columns_.size(); ++i) {
        std::string suffix = std::to_string(i + 1);

        writeHeader.addKeyword("TFORM" + suffix, columns_[i].format);

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

        if (!columns_[i].dimensions.empty()) {
            std::string tdim = "(";
            for (size_t d = 0; d < columns_[i].dimensions.size(); ++d) {
                if (d > 0)
                    tdim += ",";
                tdim += std::to_string(columns_[i].dimensions[d]);
            }
            tdim += ")";
            writeHeader.addKeyword("TDIM" + suffix, tdim);
        }
    }

    // Serialize and write header
    auto headerData = writeHeader.serialize();
    file.write(headerData.data(), headerData.size());

    // Write table data
    file.write(tableData_.data(), tableData_.size());

    // Write heap
    if (!heapData_.empty()) {
        file.write(heapData_.data(), heapData_.size());
    }

    // Write padding
    size_t totalDataSize = tableData_.size() + heapData_.size();
    size_t paddedSize = roundToBlock(totalDataSize);
    if (paddedSize > totalDataSize) {
        std::vector<char> padding(paddedSize - totalDataSize, 0);
        file.write(padding.data(), padding.size());
    }

    if (!file.good()) {
        throw FileOperationException("Failed to write binary table");
    }
}

bool BinaryTableHDU::isDataValid() const {
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

const ColumnDefinition& BinaryTableHDU::getColumn(int index) const {
    if (index < 0 || index >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }
    return columns_[index];
}

const ColumnDefinition& BinaryTableHDU::getColumn(
    const std::string& name) const {
    int idx = getColumnIndex(name);
    if (idx < 0) {
        throw std::out_of_range("Column not found: " + name);
    }
    return columns_[idx];
}

int BinaryTableHDU::getColumnIndex(const std::string& name) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool BinaryTableHDU::hasColumn(const std::string& name) const {
    return getColumnIndex(name) >= 0;
}

std::vector<std::string> BinaryTableHDU::getColumnNames() const {
    std::vector<std::string> names;
    names.reserve(columns_.size());
    for (const auto& col : columns_) {
        names.push_back(col.name);
    }
    return names;
}

int BinaryTableHDU::addColumn(const ColumnDefinition& def) {
    columns_.push_back(def);
    updateColumnOffsets();

    // Resize table data to accommodate new column
    int64_t newRowWidth = 0;
    for (const auto& col : columns_) {
        newRowWidth += col.width;
    }

    if (newRowWidth != rowWidth_) {
        // Need to reallocate and copy data
        std::vector<char> newData(numRows_ * newRowWidth, 0);

        // Copy existing data row by row
        for (int64_t row = 0; row < numRows_; ++row) {
            std::memcpy(newData.data() + row * newRowWidth,
                        tableData_.data() + row * rowWidth_, rowWidth_);
        }

        tableData_ = std::move(newData);
        rowWidth_ = newRowWidth;
    }

    return static_cast<int>(columns_.size() - 1);
}

int BinaryTableHDU::addColumn(const std::string& name,
                              const std::string& format,
                              const std::string& unit) {
    ColumnDefinition def = parseFormat(format);
    def.name = name;
    def.unit = unit;
    def.format = format;
    return addColumn(def);
}

void BinaryTableHDU::removeColumn(int index) {
    if (index < 0 || index >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    int64_t colOffset = columnOffsets_[index];
    int colWidth = columns_[index].width;

    // Create new table without this column
    int64_t newRowWidth = rowWidth_ - colWidth;
    std::vector<char> newData(numRows_ * newRowWidth);

    for (int64_t row = 0; row < numRows_; ++row) {
        char* srcRow = tableData_.data() + row * rowWidth_;
        char* dstRow = newData.data() + row * newRowWidth;

        // Copy data before the removed column
        if (colOffset > 0) {
            std::memcpy(dstRow, srcRow, colOffset);
        }

        // Copy data after the removed column
        int64_t afterOffset = colOffset + colWidth;
        if (afterOffset < rowWidth_) {
            std::memcpy(dstRow + colOffset, srcRow + afterOffset,
                        rowWidth_ - afterOffset);
        }
    }

    tableData_ = std::move(newData);
    rowWidth_ = newRowWidth;
    columns_.erase(columns_.begin() + index);
    updateColumnOffsets();
}

void BinaryTableHDU::removeColumn(const std::string& name) {
    int idx = getColumnIndex(name);
    if (idx < 0) {
        throw std::out_of_range("Column not found: " + name);
    }
    removeColumn(idx);
}

void BinaryTableHDU::addRows(int64_t count) {
    if (count <= 0)
        return;

    tableData_.resize((numRows_ + count) * rowWidth_, 0);
    numRows_ += count;
}

void BinaryTableHDU::removeRows(int64_t firstRow, int64_t count) {
    if (firstRow < 0 || firstRow >= numRows_) {
        throw std::out_of_range("First row out of range");
    }

    count = std::min(count, numRows_ - firstRow);

    // Shift remaining rows
    char* dst = tableData_.data() + firstRow * rowWidth_;
    char* src = tableData_.data() + (firstRow + count) * rowWidth_;
    size_t remaining = (numRows_ - firstRow - count) * rowWidth_;

    if (remaining > 0) {
        std::memmove(dst, src, remaining);
    }

    numRows_ -= count;
    tableData_.resize(numRows_ * rowWidth_);
}

void BinaryTableHDU::insertRows(int64_t position, int64_t count) {
    if (position < 0 || position > numRows_) {
        throw std::out_of_range("Insert position out of range");
    }

    if (count <= 0)
        return;

    tableData_.resize((numRows_ + count) * rowWidth_);

    // Shift existing rows
    if (position < numRows_) {
        char* src = tableData_.data() + position * rowWidth_;
        char* dst = tableData_.data() + (position + count) * rowWidth_;
        size_t toMove = (numRows_ - position) * rowWidth_;
        std::memmove(dst, src, toMove);
    }

    // Zero the new rows
    std::memset(tableData_.data() + position * rowWidth_, 0, count * rowWidth_);

    numRows_ += count;
}

std::vector<std::string> BinaryTableHDU::readStringColumn(int colIndex) const {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& col = columns_[colIndex];
    if (col.type != ColumnType::STRING) {
        throw DataFormatException("Column is not a string type");
    }

    std::vector<std::string> result(numRows_);
    int64_t offset = columnOffsets_[colIndex];

    for (int64_t row = 0; row < numRows_; ++row) {
        const char* ptr = tableData_.data() + row * rowWidth_ + offset;
        result[row] = std::string(ptr, col.width);

        // Trim trailing spaces
        while (!result[row].empty() && result[row].back() == ' ') {
            result[row].pop_back();
        }
    }

    return result;
}

void BinaryTableHDU::writeStringColumn(int colIndex,
                                       const std::vector<std::string>& data) {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    if (static_cast<int64_t>(data.size()) != numRows_) {
        throw DataFormatException("Data size doesn't match row count");
    }

    const auto& col = columns_[colIndex];
    int64_t offset = columnOffsets_[colIndex];

    for (int64_t row = 0; row < numRows_; ++row) {
        char* ptr = tableData_.data() + row * rowWidth_ + offset;
        std::memset(ptr, ' ', col.width);  // Pad with spaces
        size_t copyLen =
            std::min(data[row].size(), static_cast<size_t>(col.width));
        std::memcpy(ptr, data[row].data(), copyLen);
    }
}

std::string BinaryTableHDU::readStringCell(int64_t row, int col) const {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    int64_t offset = columnOffsets_[col];
    const char* ptr = tableData_.data() + row * rowWidth_ + offset;

    std::string result(ptr, column.width);

    // Trim trailing spaces
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

void BinaryTableHDU::writeStringCell(int64_t row, int col,
                                     const std::string& value) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    int64_t offset = columnOffsets_[col];
    char* ptr = tableData_.data() + row * rowWidth_ + offset;

    std::memset(ptr, ' ', column.width);
    size_t copyLen = std::min(value.size(), static_cast<size_t>(column.width));
    std::memcpy(ptr, value.data(), copyLen);
}

double BinaryTableHDU::applyScaling(double value, int colIndex) const {
    const auto& col = columns_[colIndex];
    return value * col.scale + col.zero;
}

double BinaryTableHDU::removeScaling(double value, int colIndex) const {
    const auto& col = columns_[colIndex];
    return (value - col.zero) / col.scale;
}

void BinaryTableHDU::clear() {
    numRows_ = 0;
    rowWidth_ = 0;
    heapSize_ = 0;
    columns_.clear();
    columnOffsets_.clear();
    tableData_.clear();
    heapData_.clear();
}

void BinaryTableHDU::reserve(int64_t numRows) {
    tableData_.reserve(numRows * rowWidth_);
}

ColumnDefinition BinaryTableHDU::parseFormat(const std::string& format) const {
    ColumnDefinition def;

    if (format.empty()) {
        def.type = ColumnType::UNKNOWN;
        return def;
    }

    // Parse repeat count
    size_t typePos = 0;
    while (typePos < format.size() && std::isdigit(format[typePos])) {
        ++typePos;
    }

    if (typePos > 0) {
        def.repeatCount = std::stoi(format.substr(0, typePos));
    } else {
        def.repeatCount = 1;
    }

    if (typePos >= format.size()) {
        def.type = ColumnType::UNKNOWN;
        return def;
    }

    // Parse type code
    char typeCode = format[typePos];
    def.type = codeToType(typeCode);

    // Calculate width
    int typeSize = getTypeSize(def.type);

    if (def.type == ColumnType::STRING) {
        // For strings, repeat count is the string length
        def.stringLength = def.repeatCount;
        def.width = def.repeatCount;
        def.repeatCount = 1;
    } else if (def.type == ColumnType::BIT) {
        // Bits are packed
        def.width = (def.repeatCount + 7) / 8;
    } else {
        def.width = def.repeatCount * typeSize;
    }

    def.isArray = (def.repeatCount > 1 && def.type != ColumnType::STRING);
    def.isVariable = (def.type == ColumnType::VARIABLE ||
                      def.type == ColumnType::VARIABLE64);

    return def;
}

std::string BinaryTableHDU::buildFormat(const ColumnDefinition& def) const {
    std::string result;

    if (def.type == ColumnType::STRING) {
        result = std::to_string(def.stringLength) + "A";
    } else if (def.repeatCount > 1 || def.isArray) {
        result = std::to_string(def.repeatCount) + typeToCode(def.type);
    } else {
        result = std::string(1, typeToCode(def.type));
    }

    return result;
}

int64_t BinaryTableHDU::getColumnOffset(int colIndex) const {
    if (colIndex < 0 || colIndex >= static_cast<int>(columnOffsets_.size())) {
        throw std::out_of_range("Column index out of range");
    }
    return columnOffsets_[colIndex];
}

int BinaryTableHDU::getTypeSize(ColumnType type) const noexcept {
    switch (type) {
        case ColumnType::LOGICAL:
        case ColumnType::BYTE:
            return 1;
        case ColumnType::INT16:
            return 2;
        case ColumnType::INT32:
        case ColumnType::FLOAT32:
            return 4;
        case ColumnType::INT64:
        case ColumnType::FLOAT64:
        case ColumnType::COMPLEX64:
        case ColumnType::VARIABLE:
            return 8;
        case ColumnType::COMPLEX128:
        case ColumnType::VARIABLE64:
            return 16;
        case ColumnType::BIT:
            return 1;  // Per 8 bits
        case ColumnType::STRING:
            return 1;  // Per character
        default:
            return 0;
    }
}

ColumnType BinaryTableHDU::codeToType(char code) const noexcept {
    switch (code) {
        case 'L':
            return ColumnType::LOGICAL;
        case 'X':
            return ColumnType::BIT;
        case 'B':
            return ColumnType::BYTE;
        case 'I':
            return ColumnType::INT16;
        case 'J':
            return ColumnType::INT32;
        case 'K':
            return ColumnType::INT64;
        case 'E':
            return ColumnType::FLOAT32;
        case 'D':
            return ColumnType::FLOAT64;
        case 'C':
            return ColumnType::COMPLEX64;
        case 'M':
            return ColumnType::COMPLEX128;
        case 'A':
            return ColumnType::STRING;
        case 'P':
            return ColumnType::VARIABLE;
        case 'Q':
            return ColumnType::VARIABLE64;
        default:
            return ColumnType::UNKNOWN;
    }
}

char BinaryTableHDU::typeToCode(ColumnType type) const noexcept {
    switch (type) {
        case ColumnType::LOGICAL:
            return 'L';
        case ColumnType::BIT:
            return 'X';
        case ColumnType::BYTE:
            return 'B';
        case ColumnType::INT16:
            return 'I';
        case ColumnType::INT32:
            return 'J';
        case ColumnType::INT64:
            return 'K';
        case ColumnType::FLOAT32:
            return 'E';
        case ColumnType::FLOAT64:
            return 'D';
        case ColumnType::COMPLEX64:
            return 'C';
        case ColumnType::COMPLEX128:
            return 'M';
        case ColumnType::STRING:
            return 'A';
        case ColumnType::VARIABLE:
            return 'P';
        case ColumnType::VARIABLE64:
            return 'Q';
        default:
            return '?';
    }
}

void BinaryTableHDU::updateColumnOffsets() {
    columnOffsets_.resize(columns_.size());
    int64_t offset = 0;
    for (size_t i = 0; i < columns_.size(); ++i) {
        columnOffsets_[i] = offset;
        offset += columns_[i].width;
    }
    rowWidth_ = offset;
}

std::vector<char> BinaryTableHDU::readRawCell(int64_t row, int col) const {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    int64_t offset = columnOffsets_[col];

    std::vector<char> data(column.width);
    std::memcpy(data.data(), tableData_.data() + row * rowWidth_ + offset,
                column.width);

    return data;
}

void BinaryTableHDU::writeRawCell(int64_t row, int col,
                                  const std::vector<char>& data) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    const auto& column = columns_[col];
    int64_t offset = columnOffsets_[col];

    size_t copyLen = std::min(data.size(), static_cast<size_t>(column.width));
    std::memcpy(tableData_.data() + row * rowWidth_ + offset, data.data(),
                copyLen);
}

template <typename T>
void BinaryTableHDU::swapEndian(T& value) noexcept {
    if constexpr (sizeof(T) > 1) {
        auto* bytes = reinterpret_cast<uint8_t*>(&value);
        std::reverse(bytes, bytes + sizeof(T));
    }
}

// Template implementations for reading columns

template <typename T>
std::vector<T> BinaryTableHDU::readColumn(int colIndex) const {
    return readColumn<T>(colIndex, 0, numRows_);
}

template <typename T>
std::vector<T> BinaryTableHDU::readColumn(const std::string& name) const {
    int idx = getColumnIndex(name);
    if (idx < 0) {
        throw std::out_of_range("Column not found: " + name);
    }
    return readColumn<T>(idx);
}

template <typename T>
std::vector<T> BinaryTableHDU::readColumn(int colIndex, int64_t firstRow,
                                          int64_t numRows) const {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    if (firstRow < 0 || firstRow >= numRows_) {
        throw std::out_of_range("First row out of range");
    }

    numRows = std::min(numRows, numRows_ - firstRow);

    const auto& col = columns_[colIndex];
    int64_t offset = columnOffsets_[colIndex];

    std::vector<T> result(numRows);

    for (int64_t i = 0; i < numRows; ++i) {
        const char* ptr =
            tableData_.data() + (firstRow + i) * rowWidth_ + offset;
        T value;
        std::memcpy(&value, ptr, sizeof(T));

        // FITS uses big-endian, swap if needed
        if constexpr (std::endian::native == std::endian::little) {
            swapEndian(value);
        }

        result[i] = value;
    }

    return result;
}

template <typename T>
void BinaryTableHDU::writeColumn(int colIndex, const std::vector<T>& data) {
    if (colIndex < 0 || colIndex >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    if (static_cast<int64_t>(data.size()) != numRows_) {
        throw DataFormatException("Data size doesn't match row count");
    }

    int64_t offset = columnOffsets_[colIndex];

    for (int64_t i = 0; i < numRows_; ++i) {
        T value = data[i];

        // Convert to big-endian
        if constexpr (std::endian::native == std::endian::little) {
            swapEndian(value);
        }

        char* ptr = tableData_.data() + i * rowWidth_ + offset;
        std::memcpy(ptr, &value, sizeof(T));
    }
}

template <typename T>
void BinaryTableHDU::writeColumn(const std::string& name,
                                 const std::vector<T>& data) {
    int idx = getColumnIndex(name);
    if (idx < 0) {
        throw std::out_of_range("Column not found: " + name);
    }
    writeColumn<T>(idx, data);
}

template <typename T>
T BinaryTableHDU::readCell(int64_t row, int col) const {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    int64_t offset = columnOffsets_[col];
    const char* ptr = tableData_.data() + row * rowWidth_ + offset;

    T value;
    std::memcpy(&value, ptr, sizeof(T));

    if constexpr (std::endian::native == std::endian::little) {
        swapEndian(value);
    }

    return value;
}

template <typename T>
void BinaryTableHDU::writeCell(int64_t row, int col, const T& value) {
    if (row < 0 || row >= numRows_) {
        throw std::out_of_range("Row index out of range");
    }
    if (col < 0 || col >= static_cast<int>(columns_.size())) {
        throw std::out_of_range("Column index out of range");
    }

    T writeValue = value;
    if constexpr (std::endian::native == std::endian::little) {
        swapEndian(writeValue);
    }

    int64_t offset = columnOffsets_[col];
    char* ptr = tableData_.data() + row * rowWidth_ + offset;
    std::memcpy(ptr, &writeValue, sizeof(T));
}

// Explicit template instantiations
template std::vector<int16_t> BinaryTableHDU::readColumn<int16_t>(int) const;
template std::vector<int32_t> BinaryTableHDU::readColumn<int32_t>(int) const;
template std::vector<int64_t> BinaryTableHDU::readColumn<int64_t>(int) const;
template std::vector<float> BinaryTableHDU::readColumn<float>(int) const;
template std::vector<double> BinaryTableHDU::readColumn<double>(int) const;

template std::vector<int16_t> BinaryTableHDU::readColumn<int16_t>(
    const std::string&) const;
template std::vector<int32_t> BinaryTableHDU::readColumn<int32_t>(
    const std::string&) const;
template std::vector<int64_t> BinaryTableHDU::readColumn<int64_t>(
    const std::string&) const;
template std::vector<float> BinaryTableHDU::readColumn<float>(
    const std::string&) const;
template std::vector<double> BinaryTableHDU::readColumn<double>(
    const std::string&) const;

template void BinaryTableHDU::writeColumn<int16_t>(int,
                                                   const std::vector<int16_t>&);
template void BinaryTableHDU::writeColumn<int32_t>(int,
                                                   const std::vector<int32_t>&);
template void BinaryTableHDU::writeColumn<int64_t>(int,
                                                   const std::vector<int64_t>&);
template void BinaryTableHDU::writeColumn<float>(int,
                                                 const std::vector<float>&);
template void BinaryTableHDU::writeColumn<double>(int,
                                                  const std::vector<double>&);

template int16_t BinaryTableHDU::readCell<int16_t>(int64_t, int) const;
template int32_t BinaryTableHDU::readCell<int32_t>(int64_t, int) const;
template int64_t BinaryTableHDU::readCell<int64_t>(int64_t, int) const;
template float BinaryTableHDU::readCell<float>(int64_t, int) const;
template double BinaryTableHDU::readCell<double>(int64_t, int) const;

template void BinaryTableHDU::writeCell<int16_t>(int64_t, int, const int16_t&);
template void BinaryTableHDU::writeCell<int32_t>(int64_t, int, const int32_t&);
template void BinaryTableHDU::writeCell<int64_t>(int64_t, int, const int64_t&);
template void BinaryTableHDU::writeCell<float>(int64_t, int, const float&);
template void BinaryTableHDU::writeCell<double>(int64_t, int, const double&);

// Utility functions

std::tuple<ColumnType, int, int> parseTFORM(const std::string& format) {
    if (format.empty()) {
        return {ColumnType::UNKNOWN, 0, 0};
    }

    size_t typePos = 0;
    while (typePos < format.size() && std::isdigit(format[typePos])) {
        ++typePos;
    }

    int repeatCount = 1;
    if (typePos > 0) {
        repeatCount = std::stoi(format.substr(0, typePos));
    }

    if (typePos >= format.size()) {
        return {ColumnType::UNKNOWN, repeatCount, 0};
    }

    ColumnType type;
    int width = 0;

    switch (format[typePos]) {
        case 'L':
            type = ColumnType::LOGICAL;
            width = repeatCount;
            break;
        case 'X':
            type = ColumnType::BIT;
            width = (repeatCount + 7) / 8;
            break;
        case 'B':
            type = ColumnType::BYTE;
            width = repeatCount;
            break;
        case 'I':
            type = ColumnType::INT16;
            width = repeatCount * 2;
            break;
        case 'J':
            type = ColumnType::INT32;
            width = repeatCount * 4;
            break;
        case 'K':
            type = ColumnType::INT64;
            width = repeatCount * 8;
            break;
        case 'E':
            type = ColumnType::FLOAT32;
            width = repeatCount * 4;
            break;
        case 'D':
            type = ColumnType::FLOAT64;
            width = repeatCount * 8;
            break;
        case 'C':
            type = ColumnType::COMPLEX64;
            width = repeatCount * 8;
            break;
        case 'M':
            type = ColumnType::COMPLEX128;
            width = repeatCount * 16;
            break;
        case 'A':
            type = ColumnType::STRING;
            width = repeatCount;
            break;
        case 'P':
            type = ColumnType::VARIABLE;
            width = 8;
            break;
        case 'Q':
            type = ColumnType::VARIABLE64;
            width = 16;
            break;
        default:
            type = ColumnType::UNKNOWN;
            width = 0;
            break;
    }

    return {type, repeatCount, width};
}

std::string columnTypeToString(ColumnType type) {
    switch (type) {
        case ColumnType::LOGICAL:
            return "Logical";
        case ColumnType::BIT:
            return "Bit";
        case ColumnType::BYTE:
            return "Byte";
        case ColumnType::INT16:
            return "Int16";
        case ColumnType::INT32:
            return "Int32";
        case ColumnType::INT64:
            return "Int64";
        case ColumnType::FLOAT32:
            return "Float32";
        case ColumnType::FLOAT64:
            return "Float64";
        case ColumnType::COMPLEX64:
            return "Complex64";
        case ColumnType::COMPLEX128:
            return "Complex128";
        case ColumnType::STRING:
            return "String";
        case ColumnType::VARIABLE:
            return "Variable";
        case ColumnType::VARIABLE64:
            return "Variable64";
        case ColumnType::UNKNOWN:
            return "Unknown";
    }
    return "Unknown";
}

}  // namespace atom::image::fits
