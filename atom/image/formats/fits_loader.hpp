/**
 * @file fits_loader.hpp
 * @brief Enhanced FITS file loading with smart detection and optimizations
 *
 * This file provides an advanced FITS loader that mimics cfitsio functionality
 * with support for:
 * - Smart image type detection
 * - Section/tile reading for large files
 * - Memory mapping for efficient large file access
 * - Lazy loading
 * - Checksum verification
 * - Multi-extension file handling
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_FITS_LOADER_HPP
#define ATOM_IMAGE_FITS_LOADER_HPP

#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "fits_file.hpp"
#include "fits_image_types.hpp"
#include "hdu.hpp"

namespace atom::image::fits {

/**
 * @struct LoadOptions
 * @brief Options for loading FITS files
 */
struct LoadOptions {
    bool useMmap = false;            ///< Use memory-mapped I/O
    bool lazyLoad = false;           ///< Delay data loading until access
    bool verifyChecksum = true;      ///< Verify checksum if present
    bool loadAllExtensions = true;   ///< Load all extensions or just primary
    int specificExtension = -1;      ///< Load specific extension (-1 = all)
    size_t chunkSize = 1024 * 1024;  ///< Chunk size for reading (1MB default)

    // Progress callback
    std::function<void(float, const std::string&)> progressCallback;
};

/**
 * @struct SectionSpec
 * @brief Specification for reading a section of an image
 */
struct SectionSpec {
    int64_t x1 = 0;  ///< First pixel in X (1-indexed, inclusive)
    int64_t x2 = 0;  ///< Last pixel in X (1-indexed, inclusive)
    int64_t y1 = 0;  ///< First pixel in Y (1-indexed, inclusive)
    int64_t y2 = 0;  ///< Last pixel in Y (1-indexed, inclusive)
    int64_t z1 = 0;  ///< First pixel in Z (for 3D, 1-indexed)
    int64_t z2 = 0;  ///< Last pixel in Z (for 3D, 1-indexed)
    int hdu = 0;     ///< HDU index (0 = primary)

    [[nodiscard]] int64_t width() const noexcept { return x2 - x1 + 1; }
    [[nodiscard]] int64_t height() const noexcept { return y2 - y1 + 1; }
    [[nodiscard]] int64_t depth() const noexcept { return z2 - z1 + 1; }
    [[nodiscard]] bool is3D() const noexcept { return z1 > 0 && z2 > 0; }
};

/**
 * @struct FileInfo
 * @brief Information about a FITS file without loading data
 */
struct FileInfo {
    std::string filename;
    size_t fileSize = 0;
    int hduCount = 0;
    std::vector<FITSImageInfo> hduInfo;
    bool isValid = false;
    std::string errorMessage;
};

/**
 * @struct ChecksumResult
 * @brief Result of checksum verification
 */
struct ChecksumResult {
    bool hasChecksum = false;      ///< File has checksum keywords
    bool headerValid = false;      ///< Header checksum is valid
    bool dataValid = false;        ///< Data checksum is valid
    std::string checksumValue;     ///< CHECKSUM keyword value
    std::string datasumValue;      ///< DATASUM keyword value
    uint32_t computedDatasum = 0;  ///< Computed data checksum
};

/**
 * @class FITSLoader
 * @brief Advanced FITS file loader with cfitsio-like functionality
 *
 * This class provides comprehensive FITS file loading capabilities including
 * smart type detection, efficient memory usage, and support for various
 * FITS features.
 */
class FITSLoader {
public:
    /**
     * @brief Default constructor
     */
    FITSLoader() = default;

    /**
     * @brief Virtual destructor
     */
    virtual ~FITSLoader() = default;

    // Delete copy operations
    FITSLoader(const FITSLoader&) = delete;
    FITSLoader& operator=(const FITSLoader&) = delete;

    // Allow move operations
    FITSLoader(FITSLoader&&) noexcept = default;
    FITSLoader& operator=(FITSLoader&&) noexcept = default;

    /**
     * @brief Get file information without loading data
     * @param filename Path to FITS file
     * @return File information structure
     */
    [[nodiscard]] FileInfo getFileInfo(const std::string& filename) const;

    /**
     * @brief Detect image type of a FITS file
     * @param filename Path to FITS file
     * @param hduIndex HDU index (0 = primary)
     * @return Detected image type
     */
    [[nodiscard]] ImageType detectImageType(const std::string& filename,
                                            int hduIndex = 0) const;

    /**
     * @brief Load a FITS file with options
     * @param filename Path to FITS file
     * @param options Loading options
     * @return Loaded FITS file
     * @throws FITSFileException on load errors
     */
    [[nodiscard]] std::unique_ptr<FITSFile> load(
        const std::string& filename, const LoadOptions& options = {}) const;

    /**
     * @brief Load FITS file asynchronously
     * @param filename Path to FITS file
     * @param options Loading options
     * @return Future with loaded FITS file
     */
    [[nodiscard]] std::future<std::unique_ptr<FITSFile>> loadAsync(
        const std::string& filename, const LoadOptions& options = {}) const;

    /**
     * @brief Read a section/region of a FITS image
     * @param filename Path to FITS file
     * @param section Section specification
     * @return ImageHDU containing the section data
     * @throws FITSFileException on read errors
     */
    [[nodiscard]] std::unique_ptr<ImageHDU> readSection(
        const std::string& filename, const SectionSpec& section) const;

    /**
     * @brief Read image data using memory mapping
     * @param filename Path to FITS file
     * @param hduIndex HDU index
     * @return Loaded FITS file with memory-mapped data
     */
    [[nodiscard]] std::unique_ptr<FITSFile> loadWithMmap(
        const std::string& filename, int hduIndex = 0) const;

    /**
     * @brief Create a lazy-loading FITS file handle
     * @param filename Path to FITS file
     * @return FITS file with lazy data loading
     */
    [[nodiscard]] std::unique_ptr<FITSFile> loadLazy(
        const std::string& filename) const;

    /**
     * @brief Verify FITS file checksum
     * @param filename Path to FITS file
     * @param hduIndex HDU index (-1 for all HDUs)
     * @return Checksum verification result
     */
    [[nodiscard]] ChecksumResult verifyChecksum(const std::string& filename,
                                                int hduIndex = -1) const;

    /**
     * @brief Read only the header of an HDU
     * @param filename Path to FITS file
     * @param hduIndex HDU index
     * @return FITSHeader object
     */
    [[nodiscard]] FITSHeader readHeader(const std::string& filename,
                                        int hduIndex = 0) const;

    /**
     * @brief Get value of a specific keyword
     * @param filename Path to FITS file
     * @param keyword Keyword name
     * @param hduIndex HDU index
     * @return Keyword value or empty if not found
     */
    [[nodiscard]] std::optional<std::string> getKeyword(
        const std::string& filename, const std::string& keyword,
        int hduIndex = 0) const;

    /**
     * @brief Read a subset of rows from a table extension
     * @param filename Path to FITS file
     * @param hduIndex HDU index of table
     * @param firstRow First row to read (1-indexed)
     * @param numRows Number of rows to read
     * @return Table data as HDU
     */
    [[nodiscard]] std::unique_ptr<HDU> readTableRows(
        const std::string& filename, int hduIndex, int64_t firstRow,
        int64_t numRows) const;

    /**
     * @brief Get the number of HDUs in a file
     * @param filename Path to FITS file
     * @return Number of HDUs
     */
    [[nodiscard]] int getHDUCount(const std::string& filename) const;

    /**
     * @brief Check if file is a valid FITS file
     * @param filename Path to file
     * @return True if valid FITS file
     */
    [[nodiscard]] bool isValidFITS(const std::string& filename) const;

    /**
     * @brief Read pixel data as a specific type
     * @tparam T Data type to read as
     * @param filename Path to FITS file
     * @param hduIndex HDU index
     * @return Vector of pixel values
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readPixels(const std::string& filename,
                                            int hduIndex = 0) const;

    /**
     * @brief Read a single pixel value
     * @tparam T Data type
     * @param filename Path to FITS file
     * @param x X coordinate (1-indexed)
     * @param y Y coordinate (1-indexed)
     * @param hduIndex HDU index
     * @return Pixel value
     */
    template <typename T>
    [[nodiscard]] T readPixel(const std::string& filename, int64_t x, int64_t y,
                              int hduIndex = 0) const;

    /**
     * @brief Read a row of pixels
     * @tparam T Data type
     * @param filename Path to FITS file
     * @param row Row number (1-indexed)
     * @param hduIndex HDU index
     * @return Vector of pixel values for the row
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readRow(const std::string& filename,
                                         int64_t row, int hduIndex = 0) const;

    /**
     * @brief Read a column of pixels
     * @tparam T Data type
     * @param filename Path to FITS file
     * @param col Column number (1-indexed)
     * @param hduIndex HDU index
     * @return Vector of pixel values for the column
     */
    template <typename T>
    [[nodiscard]] std::vector<T> readColumn(const std::string& filename,
                                            int64_t col,
                                            int hduIndex = 0) const;

    /**
     * @brief Set default loading options
     * @param options Default options to use
     */
    void setDefaultOptions(const LoadOptions& options) noexcept;

    /**
     * @brief Get current default options
     * @return Current default options
     */
    [[nodiscard]] const LoadOptions& getDefaultOptions() const noexcept;

private:
    LoadOptions defaultOptions_;
    FITSImageClassifier classifier_;

    /**
     * @brief Internal method to parse header from file
     */
    [[nodiscard]] FITSHeader parseHeaderFromFile(std::ifstream& file,
                                                 std::streampos startPos) const;

    /**
     * @brief Calculate data offset for an HDU
     */
    [[nodiscard]] std::streampos calculateDataOffset(
        const FITSHeader& header, std::streampos headerStart) const;

    /**
     * @brief Compute FITS checksum for a data block
     */
    [[nodiscard]] uint32_t computeChecksum(const std::vector<char>& data) const;

    /**
     * @brief Read raw bytes from file at specific position
     */
    [[nodiscard]] std::vector<char> readRawBytes(std::ifstream& file,
                                                 std::streampos pos,
                                                 size_t size) const;

    /**
     * @brief Get byte size for BITPIX value
     */
    [[nodiscard]] int getBytesPerPixel(int bitpix) const noexcept;

    /**
     * @brief Report progress if callback is set
     */
    void reportProgress(const LoadOptions& options, float progress,
                        const std::string& status) const;
};

/**
 * @brief Global FITS loader instance for convenience
 * @return Reference to global loader
 */
FITSLoader& getGlobalLoader();

/**
 * @brief Quick load a FITS file
 * @param filename Path to FITS file
 * @return Loaded FITS file
 */
[[nodiscard]] std::unique_ptr<FITSFile> quickLoad(const std::string& filename);

/**
 * @brief Quick get file info
 * @param filename Path to FITS file
 * @return File information
 */
[[nodiscard]] FileInfo quickInfo(const std::string& filename);

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_FITS_LOADER_HPP
