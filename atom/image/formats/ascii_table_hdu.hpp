/**
 * @file ascii_table_hdu.hpp
 * @brief FITS ASCII Table Extension HDU support
 *
 * ASCII tables store tabular data in human-readable ASCII format.
 * While less efficient than binary tables, they are portable and
 * can store arbitrary precision numeric data.
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_ASCII_TABLE_HDU_HPP
#define ATOM_IMAGE_ASCII_TABLE_HDU_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "fits_header.hpp"
#include "hdu.hpp"

namespace atom::image::fits {

/**
 * @enum ASCIIColumnType
 * @brief ASCII table column data types
 */
enum class ASCIIColumnType {
    INTEGER,   ///< 'I' - Integer
    FLOAT,     ///< 'F' - Fixed-point float
    EXPONENT,  ///< 'E' - Exponential float
    DOUBLE,    ///< 'D' - Double precision
    STRING,    ///< 'A' - Character string
    UNKNOWN    ///< Unknown type
};

/**
 * @struct ASCIIColumnDef
 * @brief Definition of an ASCII table column
 */
struct ASCIIColumnDef {
    std::string name;           ///< Column name (TTYPEn)
    std::string unit;           ///< Physical unit (TUNITn)
    std::string format;         ///< TFORM format string
    std::string displayFormat;  ///< Display format
    std::string nullString;     ///< Null value representation (TNULLn)

    ASCIIColumnType type = ASCIIColumnType::UNKNOWN;
    int startColumn = 0;  ///< Starting column position (TBCOLn, 1-indexed)
    int width = 0;        ///< Field width in characters
    int precision = 0;    ///< Decimal precision for floats

    double scale = 1.0;  ///< Scale factor (TSCALn)
    double zero = 0.0;   ///< Zero offset (TZEROn)
};

/**
 * @brief Variant type for ASCII table cell values
 */
using ASCIICellValue = std::variant<int64_t,     // Integer
                                    double,      // Float/Double
                                    std::string  // String
                                    >;

/**
 * @class ASCIITableHDU
 * @brief FITS ASCII Table Extension HDU
 *
 * Provides support for reading and writing FITS ASCII table extensions.
 */
class ASCIITableHDU : public HDU {
public:
    /**
     * @brief Default constructor
     */
    ASCIITableHDU();

    /**
     * @brief Constructor with dimensions
     * @param numRows Number of rows
     * @param rowWidth Width of each row in characters
     */
    ASCIITableHDU(int64_t numRows, int rowWidth);

    /**
     * @brief Virtual destructor
     */
    ~ASCIITableHDU() override = default;

    /**
     * @brief Read the ASCII table from a file stream
     * @param file Input file stream
     * @param progressCallback Optional progress callback
     */
    void readHDU(std::ifstream& file,
                 std::function<void(float, const std::string&)>
                     progressCallback = nullptr) override;

    /**
     * @brief Write the ASCII table to a file stream
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
     * @brief Get row width in characters
     * @return Characters per row (NAXIS1)
     */
    [[nodiscard]] int getRowWidth() const noexcept { return rowWidth_; }

    /**
     * @brief Get column definition by index
     * @param index Column index (0-based)
     * @return Column definition
     */
    [[nodiscard]] const ASCIIColumnDef& getColumn(int index) const;

    /**
     * @brief Get column definition by name
     * @param name Column name
     * @return Column definition
     */
    [[nodiscard]] const ASCIIColumnDef& getColumn(
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
    int addColumn(const ASCIIColumnDef& def);

    /**
     * @brief Add a simple column
     * @param name Column name
     * @param format TFORM format string (e.g., "I10", "F12.4", "A20")
     * @param startCol Starting column position
     * @param unit Optional unit
     * @return Index of new column
     */
    int addColumn(const std::string& name, const std::string& format,
                  int startCol, const std::string& unit = "");

    /**
     * @brief Remove a column
     * @param index Column index
     */
    void removeColumn(int index);

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

    // Data access - typed methods

    /**
     * @brief Read an integer column
     * @param colIndex Column index
     * @return Vector of integer values
     */
    [[nodiscard]] std::vector<int64_t> readIntColumn(int colIndex) const;

    /**
     * @brief Read a float column
     * @param colIndex Column index
     * @return Vector of double values
     */
    [[nodiscard]] std::vector<double> readFloatColumn(int colIndex) const;

    /**
     * @brief Read a string column
     * @param colIndex Column index
     * @return Vector of strings
     */
    [[nodiscard]] std::vector<std::string> readStringColumn(int colIndex) const;

    /**
     * @brief Write an integer column
     * @param colIndex Column index
     * @param data Integer values to write
     */
    void writeIntColumn(int colIndex, const std::vector<int64_t>& data);

    /**
     * @brief Write a float column
     * @param colIndex Column index
     * @param data Float values to write
     */
    void writeFloatColumn(int colIndex, const std::vector<double>& data);

    /**
     * @brief Write a string column
     * @param colIndex Column index
     * @param data String values to write
     */
    void writeStringColumn(int colIndex, const std::vector<std::string>& data);

    /**
     * @brief Read a cell as variant
     * @param row Row index (0-based)
     * @param col Column index (0-based)
     * @return Cell value as variant
     */
    [[nodiscard]] ASCIICellValue readCell(int64_t row, int col) const;

    /**
     * @brief Read a cell as string (raw)
     * @param row Row index
     * @param col Column index
     * @return Raw string value
     */
    [[nodiscard]] std::string readCellString(int64_t row, int col) const;

    /**
     * @brief Read a cell as integer
     * @param row Row index
     * @param col Column index
     * @return Integer value
     */
    [[nodiscard]] int64_t readCellInt(int64_t row, int col) const;

    /**
     * @brief Read a cell as double
     * @param row Row index
     * @param col Column index
     * @return Double value
     */
    [[nodiscard]] double readCellDouble(int64_t row, int col) const;

    /**
     * @brief Write a cell value
     * @param row Row index
     * @param col Column index
     * @param value Value to write (integer)
     */
    void writeCell(int64_t row, int col, int64_t value);

    /**
     * @brief Write a cell value
     * @param row Row index
     * @param col Column index
     * @param value Value to write (double)
     */
    void writeCell(int64_t row, int col, double value);

    /**
     * @brief Write a cell value
     * @param row Row index
     * @param col Column index
     * @param value Value to write (string)
     */
    void writeCell(int64_t row, int col, const std::string& value);

    // Utility methods

    /**
     * @brief Get raw table data as string
     * @return Table data
     */
    [[nodiscard]] const std::string& getRawData() const noexcept {
        return tableData_;
    }

    /**
     * @brief Get a specific row as string
     * @param row Row index
     * @return Row string
     */
    [[nodiscard]] std::string getRow(int64_t row) const;

    /**
     * @brief Set a row from string
     * @param row Row index
     * @param data Row data (must be rowWidth_ characters)
     */
    void setRow(int64_t row, const std::string& data);

    /**
     * @brief Clear all data
     */
    void clear();

protected:
    /**
     * @brief Parse TFORM format string
     * @param format Format string (e.g., "I10", "F12.4", "A20")
     * @return Column definition with type info
     */
    [[nodiscard]] ASCIIColumnDef parseFormat(const std::string& format) const;

    /**
     * @brief Format an integer value
     * @param value Integer value
     * @param col Column definition
     * @return Formatted string
     */
    [[nodiscard]] std::string formatInt(int64_t value,
                                        const ASCIIColumnDef& col) const;

    /**
     * @brief Format a float value
     * @param value Float value
     * @param col Column definition
     * @return Formatted string
     */
    [[nodiscard]] std::string formatFloat(double value,
                                          const ASCIIColumnDef& col) const;

    /**
     * @brief Format a string value
     * @param value String value
     * @param col Column definition
     * @return Formatted string
     */
    [[nodiscard]] std::string formatString(const std::string& value,
                                           const ASCIIColumnDef& col) const;

private:
    int64_t numRows_ = 0;  ///< Number of rows
    int rowWidth_ = 0;     ///< Characters per row

    std::vector<ASCIIColumnDef> columns_;
    std::string tableData_;  ///< Raw table data

    /**
     * @brief Get character offset for a cell
     */
    [[nodiscard]] size_t getCellOffset(int64_t row, int col) const;
};

/**
 * @brief Parse ASCII table TFORM format
 * @param format Format string
 * @return Tuple of (type, width, precision)
 */
std::tuple<ASCIIColumnType, int, int> parseASCIITFORM(
    const std::string& format);

/**
 * @brief Get human-readable name for column type
 * @param type Column type
 * @return Type name string
 */
[[nodiscard]] std::string asciiColumnTypeToString(ASCIIColumnType type);

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_ASCII_TABLE_HDU_HPP
