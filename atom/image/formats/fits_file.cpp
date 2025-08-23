#include "fits_file.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <stdexcept>

// Error category implementation
const char* FITSErrorCategory::name() const noexcept { return "fits_error"; }

std::string FITSErrorCategory::message(int ev) const {
    switch (static_cast<FITSErrorCode>(ev)) {
        case FITSErrorCode::Success:
            return "Success";
        case FITSErrorCode::FileNotExist:
            return "File does not exist";
        case FITSErrorCode::FileNotAccessible:
            return "File cannot be accessed";
        case FITSErrorCode::InvalidFormat:
            return "Invalid FITS file format";
        case FITSErrorCode::ReadError:
            return "Error reading FITS file";
        case FITSErrorCode::WriteError:
            return "Error writing FITS file";
        case FITSErrorCode::MemoryError:
            return "Memory allocation error";
        case FITSErrorCode::CompressionError:
            return "Compression or decompression error";
        case FITSErrorCode::CorruptedData:
            return "FITS data is corrupted";
        case FITSErrorCode::UnsupportedFeature:
            return "Unsupported FITS feature";
        case FITSErrorCode::InternalError:
            return "Internal FITS processing error";
        default:
            return "Unknown FITS error";
    }
}

const FITSErrorCategory& FITSErrorCategory::instance() noexcept {
    static FITSErrorCategory instance;
    return instance;
}

std::error_code make_error_code(FITSErrorCode code) {
    return {static_cast<int>(code), FITSErrorCategory::instance()};
}

// FITSFile implementation
FITSFile::FITSFile(const std::string& filename) {
    readFITS(filename, false, true);
}

void FITSFile::setProgressCallback(ProgressCallback callback) noexcept {
    progressCallback = std::move(callback);
}

void FITSFile::reportProgress(float progress, const std::string& status) const {
    if (progressCallback) {
        progressCallback(progress, status);
    }
}

// Original implementation for backward compatibility
void FITSFile::readFITS(const std::string& filename) {
    readFITS(filename, false, true);
}

void FITSFile::readFITS(const std::string& filename, bool useMmap,
                        bool validateData) {
    using namespace std::chrono;
    auto startTime = high_resolution_clock::now();

    reportProgress(0.0f, "Checking file existence");
    std::filesystem::path filePath(filename);
    if (!std::filesystem::exists(filePath)) {
        throw FITSFileException(FITSErrorCode::FileNotExist,
                                "File does not exist: " + filename);
    }

    reportProgress(0.05f, "Opening file");
    auto fileSize = std::filesystem::file_size(filePath);

    // Use memory mapping for large files if requested
    if (useMmap && fileSize > 100 * 1024 * 1024) {  // Files larger than 100MB
        try {
            readFITSWithMmap(filename, validateData);
            return;
        } catch (const std::exception& e) {
            // Fall back to standard I/O if memory mapping fails
            reportProgress(
                0.1f, "Memory mapping failed, falling back to standard I/O");
        }
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw FITSFileException(FITSErrorCode::FileNotAccessible,
                                "Cannot open file: " + filename);
    }

    try {
        hdus.clear();
        size_t currentPos = 0;
        size_t hduIndex = 0;

        reportProgress(0.1f, "Starting to read HDUs");
        while (file && file.peek() != EOF) {
            auto hdu = std::make_unique<ImageHDU>();

            // Provide progress updates during HDU reading - fixed lambda
            // capture
            auto hduProgress = [this, &currentPos, fileSize, hduIndex, &hdu](
                                   float hduReadProgress,
                                   const std::string& status) {
                auto sizeTuple = hdu->getImageSize();
                size_t estimatedSize = std::get<0>(sizeTuple) *
                                       std::get<1>(sizeTuple) *
                                       std::get<2>(sizeTuple);
                float overallProgress =
                    0.1f +
                    0.8f * ((currentPos + hduReadProgress * estimatedSize) /
                            static_cast<float>(fileSize));
                reportProgress(
                    overallProgress,
                    "Reading HDU " + std::to_string(hduIndex) + ": " + status);
            };

            // 直接使用进度回调进行读取，而不是设置回调
            hdu->readHDU(file, hduProgress);

            // 修改验证逻辑以适应现有接口
            if (validateData) {
                reportProgress(
                    0.9f + 0.05f * (hduIndex + 1) / (hdus.size() + 1),
                    "Validating HDU " + std::to_string(hduIndex));
                // 执行基本验证
                if (!hdu->isDataValid()) {
                    throw FITSFileException(FITSErrorCode::CorruptedData,
                                            "HDU data validation failed");
                }
            }

            hdus.push_back(std::move(hdu));
            currentPos = file.tellg();
            hduIndex++;

            // Calculate approximate progress
            float progress =
                0.1f + 0.8f * (static_cast<float>(currentPos) / fileSize);
            reportProgress(progress,
                           "Read HDU " + std::to_string(hduIndex - 1));
        }

        auto endTime = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(endTime - startTime);
        reportProgress(1.0f, "Completed reading " +
                                 std::to_string(hdus.size()) + " HDUs in " +
                                 std::to_string(duration.count()) + " ms");
    } catch (const std::exception& e) {
        throw FITSFileException(
            FITSErrorCode::ReadError,
            "Error reading FITS file: " + std::string(e.what()));
    }
}

void FITSFile::readFITSWithMmap(const std::string& filename,
                                bool validateData) {
    // Memory-mapped file implementation for large files
    // This is a placeholder - the actual implementation would depend on
    // platform-specific code or a library like Boost.Interprocess for memory
    // mapping
    throw FITSFileException(FITSErrorCode::UnsupportedFeature,
                            "Memory-mapped file reading not implemented yet");
}

std::future<void> FITSFile::readFITSAsync(const std::string& filename) {
    return readFITSAsync(filename, false, true);
}

std::future<void> FITSFile::readFITSAsync(const std::string& filename,
                                          bool useMmap, bool validateData) {
    return std::async(std::launch::async,
                      [this, filename, useMmap, validateData]() {
                          this->readFITS(filename, useMmap, validateData);
                      });
}

void FITSFile::writeFITS(const std::string& filename) const {
    reportProgress(0.0f, "Opening file for writing");
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw FITSFileException(FITSErrorCode::FileNotAccessible,
                                "Cannot create file: " + filename);
    }

    try {
        for (size_t i = 0; i < hdus.size(); ++i) {
            float progress = static_cast<float>(i) / hdus.size();
            reportProgress(progress, "Writing HDU " + std::to_string(i));
            hdus[i]->writeHDU(file);
        }
        reportProgress(1.0f, "File successfully written");
    } catch (const std::exception& e) {
        throw FITSFileException(
            FITSErrorCode::WriteError,
            "Error writing FITS file: " + std::string(e.what()));
    }
}

std::future<void> FITSFile::writeFITSAsync(const std::string& filename) const {
    return std::async(std::launch::async,
                      [this, filename]() { this->writeFITS(filename); });
}

size_t FITSFile::getHDUCount() const noexcept { return hdus.size(); }

bool FITSFile::isEmpty() const noexcept { return hdus.empty(); }

const HDU& FITSFile::getHDU(size_t index) const {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }
    return *hdus[index];
}

HDU& FITSFile::getHDU(size_t index) {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }
    return *hdus[index];
}

void FITSFile::addHDU(std::unique_ptr<HDU> hdu) {
    if (!hdu) {
        throw std::invalid_argument("Cannot add null HDU");
    }
    hdus.push_back(std::move(hdu));
}

void FITSFile::removeHDU(size_t index) {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }
    hdus.erase(hdus.begin() + static_cast<std::ptrdiff_t>(index));
}

ImageHDU& FITSFile::createImageHDU(int width, int height, int channels) {
    if (width <= 0 || height <= 0 || channels <= 0) {
        throw std::invalid_argument("Invalid image dimensions");
    }

    auto hdu = std::make_unique<ImageHDU>();
    hdu->setImageSize(width, height, channels);

    // Standard FITS header keywords
    hdu->setHeaderKeyword("SIMPLE", "T");
    hdu->setHeaderKeyword("BITPIX", "16");  // Default to 16-bit
    hdu->setHeaderKeyword("NAXIS", channels > 1 ? "3" : "2");
    hdu->setHeaderKeyword("NAXIS1", std::to_string(width));
    hdu->setHeaderKeyword("NAXIS2", std::to_string(height));
    if (channels > 1) {
        hdu->setHeaderKeyword("NAXIS3", std::to_string(channels));
    }

    // Keep reference before moving
    ImageHDU& imageHDURef = *hdu;
    hdus.push_back(std::move(hdu));

    return imageHDURef;
}
