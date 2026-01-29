/**
 * @file binary_table_hdu.hpp
 * @brief FITS Binary Table Extension HDU support
 *
 * This file provides comprehensive support for FITS binary table extensions,
 * following the FITS standard and cfitsio conventions.
 *
 * Binary tables can store:
 * - Scalar values (integers, floats, strings, complex)
 * - Fixed-length arrays
 * - Variable-length arrays (using heap)
 * - Bit arrays
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_BINARY_TABLE_HDU_HPP
#define ATOM_IMAGE_BINARY_TABLE_HDU_HPP

#include <complex>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "fits_header.hpp"
#include "hdu.hpp"

namespace atom::image::fits {

/**
 * @enum ColumnType
 * @brief FITS binary table column data types
 */
enum class ColumnType {
    LOGICAL,     ///< 'L' - Logical (T/F)
    BIT,         ///< 'X' - Bit array
    BYTE,        ///< 'B' - Unsigned byte
    INT16,       ///< 'I' - 16-bit integer
    INT32,       ///< 'J' - 32-bit integer
    INT64,       ///< 'K' - 64-bit integer
    FLOAT32,     ///< 'E' - 32-bit float
    FLOAT64,     ///< 'D' - 64-bit float
    COMPLEX64,   ///< 'C' - Complex float (2x32-bit)
    COMPLEX128,  ///< 'M' - Complex double (2x64-bit)
    STRING,      ///< 'A' - ASCII string
    VARIABLE,    ///< 'P' - Variable-length array descriptor
    VARIABLE64,  ///< 'Q' - 64-bit variable-length array descriptor
    UNKNOWN      ///< Unknown type
};

/**
 * @struct ColumnDefinition
 * @brief Definition of a binary table column
 */
struct ColumnDefinition {
    std::string name;           ///< Column name (TTYPEn)
    std::string unit;           ///< Physical unit (TUNITn)
    std::string format;         ///< TFORM format string
    std::string displayFormat;  ///< Display format (TDISPn)
    std::string nullValue;      ///< Null value representation (TNULLn)

    ColumnType type = ColumnType::UNKNOWN;
    int repeatCount = 1;   ///< Number of elements per cell
    int width = 0;         ///< Width in bytes
    int stringLength = 0;  ///< For strings, max length

    double scale = 1.0;  ///< Scale factor (TSCALn)
    double zero = 0.0;   ///< Zero offset (TZEROn)

    bool isVariable = false;  ///< Variable-length array
    bool isArray = false;     ///< Fixed-length array (repeatCount > 1)

    // For multidimensional arrays (TDIM)
    std::vector<int64_t> dimensions;
};

/**
 * @brief Variant type for cell values
 */
using CellValue =
    std::variant<bool,                  // Logical
                 std::vector<uint8_t>,  // Bit/Byte array
                 int16_t, int32_t, int64_t, float, double, std::complex<float>,
                 std::complex<double>, std::string, std::vector<int16_t>,
                 std::vector<int32_t>, std::vector<int64_t>, std::vector<float>,
                 std::vector<double>, std::vector<std::complex<float>>,
                 std::vector<std::complex<double>>>;

/**
 * @class BinaryTableHDU
 * @brief FITS Binary Table Extension HDU
 *
 * Provides full support for reading and writing FITS binary table extensions,
 * including variable-length arrays stored in the heap area.
 */
class BinaryTableHDU : public HDU {
public:
    /**
     * @brief Default constructor
     */
    BinaryTableHDU();

    /**
     * @brief Constructor with dimensions
     * @param numRows Number of rows
     * @param numCols Number of columns
     */
    BinaryTableHDU(int64_t numRows, int numCols);

    /**
     * @brief Virtual destructor
     */
    ~BinaryTableHDU() override = default;

    /**
     * @brief Read the binary table from a file stream
     * @param file Input file stream
     * @param progressCallback Optional progress callback
     */
    void readHDU(std::ifstream& file,
                 std::function<void(float, const std::string&)>
                     progressCallback = nullptr) override;

    /**
     * @brief Write the binary table to a file stream
     * @param file Output file stream
     */
    void writeHDU(std::ofstream& file) const override;

    /**
     * @brief Validate the table data
     * @return True if valid
     */
    [[nodiscard]] bool isDataValid() const override;

    // Table structure access

    /**
     * @brief Get number of rows
     * @return Number of rows (NAXIS2)
     */
    [[nodiscard]] int64_t getRowCount() const noexcept { return numRows_; }

    /**
     * @brief Get number of columns
     * @return Number of columns (TFIELDS)
     */
    [[nodiscard]] int getColumnCount() const noexcept {
        return static_cast<int>(columns_.size());
    }

    /**
     * @brief Get row width in bytes
     * @return Bytes per row (NAXIS1)
     */
    [[nodiscard]] int64_t getRowWidth() const noexcept { return rowWidth_; }

    /**
     * @brief Get column definition by index
     * @param index Column index (0-based)
     * @return Column definition
     */
    [[nodiscard]] const ColumnDefinition& getColumn(int index) const;

    /**
     * @brief Get column definition by name
     * @param name Column name
     * @return Column definition
     */
    [[nodiscard]] const ColumnDefinition& getColumn(
        const std::string& name) const;

    /**
     * @brief Get column index by name
     * @param name Column name
     * @return Column index or -1 if not found
     */
    [[nodiscard]] int getColumnIndex(const std::string& name) const;

    /**
     * @brief Check if column exists
     * @param name Column name
     * @return True if exists
     */
    [[nodiscard]] bool hasColumn(const std::string& name) const;

    /**
     * @brief Get all column names
     * @return Vector of column names
     */
    [[nodiscard]] std::vector<std::string> getColumnNames() const;

    // Column operations

    /**
     * @brief Add a new column
     * @param def Column definition
     * @return Index of new column
     */
    int addColumn(const ColumnDefinition& def);

    /**
     * @brief Add a simple column
     * @param name Column name
     * @param format TFORM format string
     * @param unit Optional unit
     * @return Index of new column
     */
    int addColumn(const std::string& name, const std::string& format,
                  const std::string& unit = "");

    /**
     * @brief Remove a column
     * @param index Column index
     */
    void removeColumn(int index);

    /**
     * @brief Remove a column by name
     * @param name Column name
     */
    void removeColumn(const std::string& name);

    // Row operations

    /**
     * @brief Add rows to the table
     * @param count Number of rows to add
     */
    void addRows(int64_t count);

    /**
     * @brief Remove rows from the table
     * @param firstRow First row to remove (0-based)
     * @param count Number of rows to remove
     */
    void removeRows(int64_t firstRow, int64_t count);

    /**
     * @brief Insert rows at a position
     * @param position Position to insert (0-based)
     * @param count Number of rows to insert
     */
    void insertRows(int64_t position, int64_t count);

    // Data access - typed methods

    /**
     * @brief Read a column as a specific type
     * @tparam T Data type
     * @param colIndex Column index
     * @return Vector of values
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readColumn(int colIndex) const;

    /**
     * @brief Read a column by name
     * @tparam T Data type
     * @param name Column name
     * @return Vector of values
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readColumn(const std::string& name) const;

    /**
     * @brief Read a range of rows from a column
     * @tparam T Data type
     * @param colIndex Column index
     * @param firstRow First row (0-based)
     * @param numRows Number of rows
     * @return Vector of values
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readColumn(int colIndex, int64_t firstRow,
                                            int64_t numRows) const;

    /**
     * @brief Write a column
     * @tparam T Data type
     * @param colIndex Column index
     * @param data Data to write
     */
    template <typename T>
    void writeColumn(int colIndex, const std::vector<T>& data);

    /**
     * @brief Write a column by name
     * @tparam T Data type
     * @param name Column name
     * @param data Data to write
     */
    template <typename T>
    void writeColumn(const std::string& name, const std::vector<T>& data);

    /**
     * @brief Read a single cell value
     * @tparam T Data type
     * @param row Row index (0-based)
     * @param col Column index (0-based)
     * @return Cell value
     */
    template <typename T>
    [[nodiscard]] T readCell(int64_t row, int col) const;

    /**
     * @brief Write a single cell value
     * @tparam T Data type
     * @param row Row index (0-based)
     * @param col Column index (0-based)
     * @param value Value to write
     */
    template <typename T>
    void writeCell(int64_t row, int col, const T& value);

    /**
     * @brief Read a cell as variant
     * @param row Row index
     * @param col Column index
     * @return Cell value as variant
     */
    [[nodiscard]] CellValue readCellVariant(int64_t row, int col) const;

    /**
     * @brief Read an array cell
     * @tparam T Element type
     * @param row Row index
     * @param col Column index
     * @return Array values
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readArrayCell(int64_t row, int col) const;

    /**
     * @brief Write an array cell
     * @tparam T Element type
     * @param row Row index
     * @param col Column index
     * @param values Array values
     */
    template <typename T>
    void writeArrayCell(int64_t row, int col, const std::vector<T>& values);

    // Variable-length array support

    /**
     * @brief Read a variable-length array cell
     * @tparam T Element type
     * @param row Row index
     * @param col Column index
     * @return Array values
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readVariableArray(int64_t row, int col) const;

    /**
     * @brief Write a variable-length array cell
     * @tparam T Element type
     * @param row Row index
     * @param col Column index
     * @param values Array values
     */
    template <typename T>
    void writeVariableArray(int64_t row, int col, const std::vector<T>& values);

    /**
     * @brief Get the length of a variable-length array
     * @param row Row index
     * @param col Column index
     * @return Array length
     */
    [[nodiscard]] int64_t getVariableArrayLength(int64_t row, int col) const;

    // String column support

    /**
     * @brief Read a string column
     * @param colIndex Column index
     * @return Vector of strings
     */
    [[nodiscard]] std::vector<std::string> readStringColumn(int colIndex) const;

    /**
     * @brief Write a string column
     * @param colIndex Column index
     * @param data Strings to write
     */
    void writeStringColumn(int colIndex, const std::vector<std::string>& data);

    /**
     * @brief Read a string cell
     * @param row Row index
     * @param col Column index
     * @return String value
     */
    [[nodiscard]] std::string readStringCell(int64_t row, int col) const;

    /**
     * @brief Write a string cell
     * @param row Row index
     * @param col Column index
     * @param value String value
     */
    void writeStringCell(int64_t row, int col, const std::string& value);

    // Utility methods

    /**
     * @brief Apply scale and zero to a value
     * @param value Raw value
     * @param colIndex Column index
     * @return Scaled value
     */
    [[nodiscard]] double applyScaling(double value, int colIndex) const;

    /**
     * @brief Remove scale and zero from a value
     * @param value Scaled value
     * @param colIndex Column index
     * @return Raw value
     */
    [[nodiscard]] double removeScaling(double value, int colIndex) const;

    /**
     * @brief Get heap size
     * @return Size of heap area in bytes
     */
    [[nodiscard]] int64_t getHeapSize() const noexcept { return heapSize_; }

    /**
     * @brief Clear all data
     */
    void clear();

    /**
     * @brief Reserve space for rows
     * @param numRows Number of rows to reserve
     */
    void reserve(int64_t numRows);

protected:
    /**
     * @brief Parse TFORM format string
     * @param format Format string
     * @return Column definition with type info
     */
    [[nodiscard]] ColumnDefinition parseFormat(const std::string& format) const;

    /**
     * @brief Build TFORM format string
     * @param def Column definition
     * @return Format string
     */
    [[nodiscard]] std::string buildFormat(const ColumnDefinition& def) const;

    /**
     * @brief Calculate byte offset for a column
     * @param colIndex Column index
     * @return Byte offset from row start
     */
    [[nodiscard]] int64_t getColumnOffset(int colIndex) const;

    /**
     * @brief Get the size in bytes for a column type
     * @param type Column type
     * @return Size in bytes
     */
    [[nodiscard]] int getTypeSize(ColumnType type) const noexcept;

    /**
     * @brief Convert type code to ColumnType
     * @param code FITS type code character
     * @return ColumnType enum
     */
    [[nodiscard]] ColumnType codeToType(char code) const noexcept;

    /**
     * @brief Convert ColumnType to type code
     * @param type ColumnType enum
     * @return FITS type code character
     */
    [[nodiscard]] char typeToCode(ColumnType type) const noexcept;

private:
    int64_t numRows_ = 0;     ///< Number of rows
    int64_t rowWidth_ = 0;    ///< Bytes per row
    int64_t heapSize_ = 0;    ///< Heap size (PCOUNT)
    int64_t heapOffset_ = 0;  ///< Offset to heap (THEAP)

    std::vector<ColumnDefinition> columns_;
    std::vector<int64_t> columnOffsets_;

    std::vector<char> tableData_;  ///< Main table data
    std::vector<char> heapData_;   ///< Heap for variable-length arrays

    /**
     * @brief Update column offsets after modification
     */
    void updateColumnOffsets();

    /**
     * @brief Read raw bytes from table
     */
    [[nodiscard]] std::vector<char> readRawCell(int64_t row, int col) const;

    /**
     * @brief Write raw bytes to table
     */
    void writeRawCell(int64_t row, int col, const std::vector<char>& data);

    /**
     * @brief Swap endianness of data
     */
    template <typename T>
    static void swapEndian(T& value) noexcept;
};

/**
 * @brief Parse TFORM format string to get column info
 * @param format TFORM string
 * @return Tuple of (type, repeat_count, width)
 */
std::tuple<ColumnType, int, int> parseTFORM(const std::string& format);

/**
 * @brief Get human-readable name for column type
 * @param type Column type
 * @return Type name string
 */
[[nodiscard]] std::string columnTypeToString(ColumnType type);

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_BINARY_TABLE_HDU_HPP
