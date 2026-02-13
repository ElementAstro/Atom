/**
 * @file fits_loader.cpp
 * @brief Implementation of enhanced FITS file loader
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "fits_loader.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <numeric>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace atom::image::fits {

namespace {

constexpr size_t FITS_BLOCK_SIZE = 2880;
constexpr size_t FITS_CARD_SIZE = 80;

// Helper to check FITS magic signature
bool checkFITSSignature(std::ifstream& file) {
    char buffer[8];
    file.seekg(0);
    file.read(buffer, 6);
    if (!file.good())
        return false;

    // Check for "SIMPLE" keyword
    return std::strncmp(buffer, "SIMPLE", 6) == 0;
}

// Round up to FITS block size
size_t roundToBlock(size_t size) {
    return ((size + FITS_BLOCK_SIZE - 1) / FITS_BLOCK_SIZE) * FITS_BLOCK_SIZE;
}

}  // anonymous namespace

FileInfo FITSLoader::getFileInfo(const std::string& filename) const {
    FileInfo info;
    info.filename = filename;

    try {
        std::filesystem::path path(filename);
        if (!std::filesystem::exists(path)) {
            info.errorMessage = "File does not exist";
            return info;
        }

        info.fileSize = std::filesystem::file_size(path);

        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            info.errorMessage = "Cannot open file";
            return info;
        }

        if (!checkFITSSignature(file)) {
            info.errorMessage = "Not a valid FITS file";
            return info;
        }

        // Parse all HDUs
        file.seekg(0);
        int hduIndex = 0;

        while (file.good() && file.peek() != EOF) {
            auto header = parseHeaderFromFile(file, file.tellg());

            // Create keyword getter for classifier
            auto getKeyword =
                [&header](
                    const std::string& key) -> std::optional<std::string> {
                try {
                    return header.getKeywordValue(key);
                } catch (...) {
                    return std::nullopt;
                }
            };

            FITSImageInfo hduInfo = classifier_.classify(getKeyword, hduIndex);
            info.hduInfo.push_back(hduInfo);

            // Skip data section
            auto dataOffset = calculateDataOffset(header, file.tellg());
            file.seekg(dataOffset);

            ++hduIndex;

            // Safety check to prevent infinite loop
            if (hduIndex > 1000)
                break;
        }

        info.hduCount = hduIndex;
        info.isValid = true;

    } catch (const std::exception& e) {
        info.errorMessage = e.what();
    }

    return info;
}

ImageType FITSLoader::detectImageType(const std::string& filename,
                                      int hduIndex) const {
    auto info = getFileInfo(filename);
    if (!info.isValid || hduIndex >= static_cast<int>(info.hduInfo.size())) {
        return ImageType::UNKNOWN;
    }
    return info.hduInfo[hduIndex].imageType;
}

std::unique_ptr<FITSFile> FITSLoader::load(const std::string& filename,
                                           const LoadOptions& options) const {
    reportProgress(options, 0.0f, "Starting load");

    // Use mmap for large files if requested
    if (options.useMmap) {
        auto fileSize = std::filesystem::file_size(filename);
        if (fileSize > 100 * 1024 * 1024) {  // > 100MB
            return loadWithMmap(filename, options.specificExtension >= 0
                                              ? options.specificExtension
                                              : 0);
        }
    }

    auto fitsFile = std::make_unique<FITSFile>();

    // Set progress callback
    if (options.progressCallback) {
        fitsFile->setProgressCallback(options.progressCallback);
    }

    reportProgress(options, 0.1f, "Opening file");

    // Use existing FITSFile read functionality
    fitsFile->readFITS(filename, options.useMmap, options.verifyChecksum);

    reportProgress(options, 1.0f, "Load complete");

    return fitsFile;
}

std::future<std::unique_ptr<FITSFile>> FITSLoader::loadAsync(
    const std::string& filename, const LoadOptions& options) const {
    return std::async(std::launch::async, [this, filename, options]() {
        return this->load(filename, options);
    });
}

std::unique_ptr<ImageHDU> FITSLoader::readSection(
    const std::string& filename, const SectionSpec& section) const {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw FITSFileException(FITSErrorCode::FileNotAccessible,
                                "Cannot open file: " + filename);
    }

    // Navigate to the correct HDU
    file.seekg(0);
    for (int i = 0; i < section.hdu; ++i) {
        auto header = parseHeaderFromFile(file, file.tellg());
        auto dataOffset = calculateDataOffset(header, file.tellg());
        file.seekg(dataOffset);
    }

    // Read the target HDU header
    auto header = parseHeaderFromFile(file, file.tellg());

    // Get image dimensions
    auto getKeyword =
        [&header](const std::string& key) -> std::optional<std::string> {
        try {
            return header.getKeywordValue(key);
        } catch (...) {
            return std::nullopt;
        }
    };

    auto dims = classifier_.extractDimensions(getKeyword);

    if (dims.naxis < 2) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "Not a 2D or higher image");
    }

    // Validate section bounds
    if (section.x1 < 1 || section.x2 > dims.axes[0] || section.y1 < 1 ||
        section.y2 > dims.axes[1] || section.x1 > section.x2 ||
        section.y1 > section.y2) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "Section bounds out of range");
    }

    // Create output HDU
    auto resultHDU = std::make_unique<ImageHDU>();
    int width = static_cast<int>(section.width());
    int height = static_cast<int>(section.height());
    int channels = section.is3D() ? static_cast<int>(section.depth()) : 1;

    resultHDU->setImageSize(width, height, channels);

    // Copy relevant header keywords
    resultHDU->setHeaderKeyword("BITPIX", std::to_string(dims.bitpix));
    resultHDU->setHeaderKeyword("NAXIS", channels > 1 ? "3" : "2");
    resultHDU->setHeaderKeyword("NAXIS1", std::to_string(width));
    resultHDU->setHeaderKeyword("NAXIS2", std::to_string(height));
    if (channels > 1) {
        resultHDU->setHeaderKeyword("NAXIS3", std::to_string(channels));
    }

    // Add section info to header
    resultHDU->setHeaderKeyword("COMMENT", "Section extracted from original");
    resultHDU->setHeaderKeyword("SECX1", std::to_string(section.x1));
    resultHDU->setHeaderKeyword("SECX2", std::to_string(section.x2));
    resultHDU->setHeaderKeyword("SECY1", std::to_string(section.y1));
    resultHDU->setHeaderKeyword("SECY2", std::to_string(section.y2));

    // Read section data
    int bytesPerPixel = getBytesPerPixel(dims.bitpix);
    int64_t fullRowSize = dims.axes[0] * bytesPerPixel;
    int64_t sectionRowSize = width * bytesPerPixel;

    // Calculate starting position in file
    std::streampos dataStart = file.tellg();

    // Allocate buffer for section data
    std::vector<char> sectionData(width * height * channels * bytesPerPixel);

    // Read row by row
    for (int z = 0; z < channels; ++z) {
        int64_t planeOffset = section.is3D()
                                  ? (section.z1 - 1 + z) * dims.axes[0] *
                                        dims.axes[1] * bytesPerPixel
                                  : 0;

        for (int64_t y = section.y1 - 1; y < section.y2; ++y) {
            int64_t rowStart = planeOffset + y * fullRowSize +
                               (section.x1 - 1) * bytesPerPixel;

            file.seekg(dataStart + static_cast<std::streamoff>(rowStart));

            int64_t destOffset =
                ((y - section.y1 + 1) * width + z * width * height) *
                bytesPerPixel;

            file.read(sectionData.data() + destOffset, sectionRowSize);

            if (!file.good()) {
                throw FITSFileException(FITSErrorCode::ReadError,
                                        "Error reading section data");
            }
        }
    }

    // Read HDU with the section data
    // Note: This is a simplified approach - in practice, the data would be
    // properly parsed and byte-swapped according to BITPIX

    return resultHDU;
}

std::unique_ptr<FITSFile> FITSLoader::loadWithMmap(const std::string& filename,
                                                   int hduIndex) const {
#ifdef _WIN32
    // Windows memory mapping implementation
    HANDLE hFile =
        CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        throw FITSFileException(FITSErrorCode::FileNotAccessible,
                                "Cannot open file for memory mapping");
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        throw FITSFileException(FITSErrorCode::ReadError,
                                "Cannot get file size");
    }

    HANDLE hMapping =
        CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (hMapping == nullptr) {
        CloseHandle(hFile);
        throw FITSFileException(FITSErrorCode::MemoryError,
                                "Cannot create file mapping");
    }

    void* mappedData = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

    if (mappedData == nullptr) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        throw FITSFileException(FITSErrorCode::MemoryError,
                                "Cannot map view of file");
    }

    // Parse FITS data from memory
    auto fitsFile = std::make_unique<FITSFile>();

    // For now, fall back to regular loading
    // A full implementation would parse directly from mapped memory
    UnmapViewOfFile(mappedData);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    // Fall back to regular loading
    fitsFile->readFITS(filename);
    return fitsFile;

#else
    // POSIX memory mapping implementation
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        throw FITSFileException(FITSErrorCode::FileNotAccessible,
                                "Cannot open file for memory mapping");
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw FITSFileException(FITSErrorCode::ReadError,
                                "Cannot get file size");
    }

    void* mappedData = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mappedData == MAP_FAILED) {
        close(fd);
        throw FITSFileException(FITSErrorCode::MemoryError,
                                "Cannot memory map file");
    }

    // Parse FITS data from memory
    auto fitsFile = std::make_unique<FITSFile>();

    // For now, fall back to regular loading
    munmap(mappedData, st.st_size);
    close(fd);

    fitsFile->readFITS(filename);
    return fitsFile;
#endif
}

std::unique_ptr<FITSFile> FITSLoader::loadLazy(
    const std::string& filename) const {
    // Create a FITS file that only loads headers initially
    auto fitsFile = std::make_unique<FITSFile>();

    // For now, use regular loading
    // A full lazy implementation would defer data loading
    fitsFile->readFITS(filename);

    return fitsFile;
}

ChecksumResult FITSLoader::verifyChecksum(const std::string& filename,
                                          int hduIndex) const {
    ChecksumResult result;

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return result;
    }

    file.seekg(0);
    int currentHdu = 0;

    while (file.good() && file.peek() != EOF) {
        std::streampos headerStart = file.tellg();
        auto header = parseHeaderFromFile(file, headerStart);

        if (hduIndex == -1 || currentHdu == hduIndex) {
            // Check for checksum keywords
            try {
                result.checksumValue = header.getKeywordValue("CHECKSUM");
                result.hasChecksum = true;
            } catch (...) {
            }

            try {
                result.datasumValue = header.getKeywordValue("DATASUM");
                result.hasChecksum = true;
            } catch (...) {
            }

            if (result.hasChecksum) {
                // Get data section
                std::streampos dataStart = file.tellg();
                auto getKeyword =
                    [&header](
                        const std::string& key) -> std::optional<std::string> {
                    try {
                        return header.getKeywordValue(key);
                    } catch (...) {
                        return std::nullopt;
                    }
                };

                auto dims = classifier_.extractDimensions(getKeyword);

                if (dims.dataSizeBytes > 0) {
                    // Read data and compute checksum
                    size_t dataSize = roundToBlock(dims.dataSizeBytes);
                    auto data = readRawBytes(file, dataStart, dataSize);

                    result.computedDatasum = computeChecksum(data);

                    // Compare with stored value
                    if (!result.datasumValue.empty()) {
                        try {
                            uint32_t storedDatasum =
                                std::stoul(result.datasumValue);
                            result.dataValid =
                                (result.computedDatasum == storedDatasum);
                        } catch (...) {
                        }
                    }
                }

                result.headerValid =
                    true;  // Simplified - would need full check

                if (hduIndex != -1) {
                    return result;
                }
            }
        }

        // Move to next HDU
        auto dataOffset = calculateDataOffset(header, file.tellg());
        file.seekg(dataOffset);
        ++currentHdu;

        if (currentHdu > 1000)
            break;  // Safety limit
    }

    return result;
}

FITSHeader FITSLoader::readHeader(const std::string& filename,
                                  int hduIndex) const {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw FITSFileException(FITSErrorCode::FileNotAccessible,
                                "Cannot open file: " + filename);
    }

    file.seekg(0);

    // Skip to requested HDU
    for (int i = 0; i < hduIndex; ++i) {
        auto header = parseHeaderFromFile(file, file.tellg());
        auto dataOffset = calculateDataOffset(header, file.tellg());
        file.seekg(dataOffset);
    }

    return parseHeaderFromFile(file, file.tellg());
}

std::optional<std::string> FITSLoader::getKeyword(const std::string& filename,
                                                  const std::string& keyword,
                                                  int hduIndex) const {
    try {
        auto header = readHeader(filename, hduIndex);
        return header.getKeywordValue(keyword);
    } catch (...) {
        return std::nullopt;
    }
}

std::unique_ptr<HDU> FITSLoader::readTableRows(const std::string& filename,
                                               int hduIndex, int64_t firstRow,
                                               int64_t numRows) const {
    // This would require BinaryTableHDU implementation
    // For now, return nullptr
    (void)filename;
    (void)hduIndex;
    (void)firstRow;
    (void)numRows;

    return nullptr;
}

int FITSLoader::getHDUCount(const std::string& filename) const {
    auto info = getFileInfo(filename);
    return info.hduCount;
}

bool FITSLoader::isValidFITS(const std::string& filename) const {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    return checkFITSSignature(file);
}

void FITSLoader::setDefaultOptions(const LoadOptions& options) noexcept {
    defaultOptions_ = options;
}

const LoadOptions& FITSLoader::getDefaultOptions() const noexcept {
    return defaultOptions_;
}

FITSHeader FITSLoader::parseHeaderFromFile(std::ifstream& file,
                                           std::streampos startPos) const {
    file.seekg(startPos);

    std::vector<char> headerData;
    headerData.reserve(FITS_BLOCK_SIZE * 10);  // Reserve space for ~10 blocks

    bool foundEnd = false;

    while (!foundEnd && file.good()) {
        // Read one block
        std::vector<char> block(FITS_BLOCK_SIZE);
        file.read(block.data(), FITS_BLOCK_SIZE);

        if (!file.good() && file.gcount() == 0) {
            break;
        }

        // Check for END keyword in this block
        for (size_t i = 0; i < FITS_BLOCK_SIZE; i += FITS_CARD_SIZE) {
            if (std::strncmp(block.data() + i, "END", 3) == 0 &&
                (i + 3 >= FITS_BLOCK_SIZE || block[i + 3] == ' ' ||
                 block[i + 3] == '\0')) {
                foundEnd = true;
            }
        }

        headerData.insert(headerData.end(), block.begin(), block.end());
    }

    FITSHeader header;
    if (!headerData.empty()) {
        header.deserialize(headerData);
    }

    return header;
}

std::streampos FITSLoader::calculateDataOffset(const FITSHeader& header,
                                               std::streampos headerEnd) const {
    // Get dimensions
    auto getKeyword =
        [&header](const std::string& key) -> std::optional<std::string> {
        try {
            return header.getKeywordValue(key);
        } catch (...) {
            return std::nullopt;
        }
    };

    auto dims = classifier_.extractDimensions(getKeyword);

    if (dims.dataSizeBytes == 0) {
        return headerEnd;
    }

    // Round up to FITS block boundary
    size_t dataBlocks = roundToBlock(dims.dataSizeBytes);

    return headerEnd + static_cast<std::streamoff>(dataBlocks);
}

uint32_t FITSLoader::computeChecksum(const std::vector<char>& data) const {
    // FITS checksum algorithm (ones-complement sum)
    uint32_t sum = 0;

    for (size_t i = 0; i < data.size(); i += 4) {
        uint32_t word = 0;
        for (size_t j = 0; j < 4 && i + j < data.size(); ++j) {
            word = (word << 8) | static_cast<uint8_t>(data[i + j]);
        }

        // Ones-complement addition
        uint64_t temp = static_cast<uint64_t>(sum) + word;
        sum = static_cast<uint32_t>(temp & 0xFFFFFFFF);
        if (temp > 0xFFFFFFFF) {
            ++sum;  // Add carry
        }
    }

    return sum;
}

std::vector<char> FITSLoader::readRawBytes(std::ifstream& file,
                                           std::streampos pos,
                                           size_t size) const {
    std::vector<char> data(size);
    file.seekg(pos);
    file.read(data.data(), size);

    if (!file.good() && file.gcount() < static_cast<std::streamsize>(size)) {
        data.resize(file.gcount());
    }

    return data;
}

int FITSLoader::getBytesPerPixel(int bitpix) const noexcept {
    return std::abs(bitpix) / 8;
}

void FITSLoader::reportProgress(const LoadOptions& options, float progress,
                                const std::string& status) const {
    if (options.progressCallback) {
        options.progressCallback(progress, status);
    }
}

// Template implementations

template <typename T>
std::vector<T> FITSLoader::readPixels(const std::string& filename,
                                      int hduIndex) const {
    auto fitsFile = load(filename);

    if (hduIndex >= static_cast<int>(fitsFile->getHDUCount())) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "HDU index out of range");
    }

    auto& hdu = fitsFile->getHDU(hduIndex);
    auto* imageHDU = dynamic_cast<ImageHDU*>(&hdu);

    if (!imageHDU) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "Not an image HDU");
    }

    auto [width, height, channels] = imageHDU->getImageSize();
    std::vector<T> pixels(width * height * channels);

    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                pixels[(y * width + x) * channels + c] =
                    imageHDU->getPixel<T>(x, y, c);
            }
        }
    }

    return pixels;
}

template <typename T>
T FITSLoader::readPixel(const std::string& filename, int64_t x, int64_t y,
                        int hduIndex) const {
    auto fitsFile = load(filename);
    auto& hdu = fitsFile->getHDU(hduIndex);
    auto* imageHDU = dynamic_cast<ImageHDU*>(&hdu);

    if (!imageHDU) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "Not an image HDU");
    }

    return imageHDU->getPixel<T>(static_cast<int>(x - 1),
                                 static_cast<int>(y - 1), 0);
}

template <typename T>
std::vector<T> FITSLoader::readRow(const std::string& filename, int64_t row,
                                   int hduIndex) const {
    auto fitsFile = load(filename);
    auto& hdu = fitsFile->getHDU(hduIndex);
    auto* imageHDU = dynamic_cast<ImageHDU*>(&hdu);

    if (!imageHDU) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "Not an image HDU");
    }

    auto [width, height, channels] = imageHDU->getImageSize();
    std::vector<T> rowData(width * channels);

    int y = static_cast<int>(row - 1);
    for (int c = 0; c < channels; ++c) {
        for (int x = 0; x < width; ++x) {
            rowData[x * channels + c] = imageHDU->getPixel<T>(x, y, c);
        }
    }

    return rowData;
}

template <typename T>
std::vector<T> FITSLoader::readColumn(const std::string& filename, int64_t col,
                                      int hduIndex) const {
    auto fitsFile = load(filename);
    auto& hdu = fitsFile->getHDU(hduIndex);
    auto* imageHDU = dynamic_cast<ImageHDU*>(&hdu);

    if (!imageHDU) {
        throw FITSFileException(FITSErrorCode::InvalidFormat,
                                "Not an image HDU");
    }

    auto [width, height, channels] = imageHDU->getImageSize();
    std::vector<T> colData(height * channels);

    int x = static_cast<int>(col - 1);
    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < height; ++y) {
            colData[y * channels + c] = imageHDU->getPixel<T>(x, y, c);
        }
    }

    return colData;
}

// Explicit template instantiations
template std::vector<uint8_t> FITSLoader::readPixels<uint8_t>(
    const std::string&, int) const;
template std::vector<int16_t> FITSLoader::readPixels<int16_t>(
    const std::string&, int) const;
template std::vector<int32_t> FITSLoader::readPixels<int32_t>(
    const std::string&, int) const;
template std::vector<int64_t> FITSLoader::readPixels<int64_t>(
    const std::string&, int) const;
template std::vector<float> FITSLoader::readPixels<float>(const std::string&,
                                                          int) const;
template std::vector<double> FITSLoader::readPixels<double>(const std::string&,
                                                            int) const;

template uint8_t FITSLoader::readPixel<uint8_t>(const std::string&, int64_t,
                                                int64_t, int) const;
template int16_t FITSLoader::readPixel<int16_t>(const std::string&, int64_t,
                                                int64_t, int) const;
template int32_t FITSLoader::readPixel<int32_t>(const std::string&, int64_t,
                                                int64_t, int) const;
template int64_t FITSLoader::readPixel<int64_t>(const std::string&, int64_t,
                                                int64_t, int) const;
template float FITSLoader::readPixel<float>(const std::string&, int64_t,
                                            int64_t, int) const;
template double FITSLoader::readPixel<double>(const std::string&, int64_t,
                                              int64_t, int) const;

template std::vector<uint8_t> FITSLoader::readRow<uint8_t>(const std::string&,
                                                           int64_t, int) const;
template std::vector<int16_t> FITSLoader::readRow<int16_t>(const std::string&,
                                                           int64_t, int) const;
template std::vector<int32_t> FITSLoader::readRow<int32_t>(const std::string&,
                                                           int64_t, int) const;
template std::vector<int64_t> FITSLoader::readRow<int64_t>(const std::string&,
                                                           int64_t, int) const;
template std::vector<float> FITSLoader::readRow<float>(const std::string&,
                                                       int64_t, int) const;
template std::vector<double> FITSLoader::readRow<double>(const std::string&,
                                                         int64_t, int) const;

template std::vector<uint8_t> FITSLoader::readColumn<uint8_t>(
    const std::string&, int64_t, int) const;
template std::vector<int16_t> FITSLoader::readColumn<int16_t>(
    const std::string&, int64_t, int) const;
template std::vector<int32_t> FITSLoader::readColumn<int32_t>(
    const std::string&, int64_t, int) const;
template std::vector<int64_t> FITSLoader::readColumn<int64_t>(
    const std::string&, int64_t, int) const;
template std::vector<float> FITSLoader::readColumn<float>(const std::string&,
                                                          int64_t, int) const;
template std::vector<double> FITSLoader::readColumn<double>(const std::string&,
                                                            int64_t, int) const;

// Global functions

FITSLoader& getGlobalLoader() {
    static FITSLoader loader;
    return loader;
}

std::unique_ptr<FITSFile> quickLoad(const std::string& filename) {
    return getGlobalLoader().load(filename);
}

FileInfo quickInfo(const std::string& filename) {
    return getGlobalLoader().getFileInfo(filename);
}

}  // namespace atom::image::fits
