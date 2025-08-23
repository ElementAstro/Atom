#include "fits_data.hpp"

#include <zlib.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <execution>
#include <fstream>
#include <future>
#include <string>
#include <vector>

// Error category implementation
class FITSDataErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "fits_data_error";
    }

    [[nodiscard]] std::string message(int ev) const override {
        switch (static_cast<FITSDataErrorCode>(ev)) {
            case FITSDataErrorCode::Success:
                return "Success";
            case FITSDataErrorCode::InvalidDataType:
                return "Invalid data type";
            case FITSDataErrorCode::InvalidDataSize:
                return "Invalid data size";
            case FITSDataErrorCode::StreamError:
                return "Stream error";
            case FITSDataErrorCode::DataReadError:
                return "Error reading data";
            case FITSDataErrorCode::DataWriteError:
                return "Error writing data";
            case FITSDataErrorCode::InvalidOperation:
                return "Invalid operation";
            case FITSDataErrorCode::CompressionError:
                return "Compression or decompression error";
            case FITSDataErrorCode::DataValidationError:
                return "Data validation error";
            case FITSDataErrorCode::MemoryAllocationError:
                return "Memory allocation error";
            case FITSDataErrorCode::InternalError:
                return "Internal error";
            default:
                return "Unknown error";
        }
    }

    [[nodiscard]] static const FITSDataErrorCategory& instance() noexcept {
        static FITSDataErrorCategory instance;
        return instance;
    }
};

std::error_code make_error_code(FITSDataErrorCode code) {
    return {static_cast<int>(code), FITSDataErrorCategory::instance()};
}
namespace {
constexpr size_t FITS_BLOCK_SIZE = 2880;

// Helper function to calculate padding
[[nodiscard]] size_t calculatePadding(size_t dataSize) noexcept {
    return (FITS_BLOCK_SIZE - (dataSize % FITS_BLOCK_SIZE)) % FITS_BLOCK_SIZE;
}

// Get human-readable size string
[[nodiscard]] std::string getHumanReadableSize(size_t bytes) {
    constexpr size_t KB = 1024;
    constexpr size_t MB = 1024 * KB;
    constexpr size_t GB = 1024 * MB;

    char buffer[64];  // Sufficient buffer size

    if (bytes >= GB) {
        snprintf(buffer, sizeof(buffer), "%.1f GB",
                 static_cast<double>(bytes) / GB);
    } else if (bytes >= MB) {
        snprintf(buffer, sizeof(buffer), "%.1f MB",
                 static_cast<double>(bytes) / MB);
    } else if (bytes >= KB) {
        snprintf(buffer, sizeof(buffer), "%.1f KB",
                 static_cast<double>(bytes) / KB);
    } else {
        snprintf(buffer, sizeof(buffer), "%zu bytes", bytes);
    }
    return std::string(buffer);
}

// Helper function to get the size of a type in bytes
// Moved definition before first use
size_t getTypeSize(DataType type) {
    switch (type) {
        case DataType::BYTE:
            return sizeof(uint8_t);
        case DataType::SHORT:
            return sizeof(int16_t);
        case DataType::INT:
            return sizeof(int32_t);
        case DataType::LONG:
            return sizeof(int64_t);
        case DataType::FLOAT:
            return sizeof(float);
        case DataType::DOUBLE:
            return sizeof(double);
        default:
            // Consider returning 0 or throwing a more specific exception if
            // needed Throwing here matches the original logic
            throw FITSDataException(FITSDataErrorCode::InvalidDataType,
                                    "Unsupported data type");
    }
}

}  // namespace

std::unique_ptr<FITSData> FITSData::createData(DataType type) {
    switch (type) {
        case DataType::BYTE:
            return std::make_unique<TypedFITSData<uint8_t>>();
        case DataType::SHORT:
            return std::make_unique<TypedFITSData<int16_t>>();
        case DataType::INT:
            return std::make_unique<TypedFITSData<int32_t>>();
        case DataType::LONG:
            return std::make_unique<TypedFITSData<int64_t>>();
        case DataType::FLOAT:
            return std::make_unique<TypedFITSData<float>>();
        case DataType::DOUBLE:
            return std::make_unique<TypedFITSData<double>>();
        default:
            throw FITSDataException(FITSDataErrorCode::InvalidDataType,
                                    "Unsupported data type");
    }
}

std::unique_ptr<FITSData> FITSData::createData(DataType type, size_t size) {
    try {
        switch (type) {
            case DataType::BYTE:
                return std::make_unique<TypedFITSData<uint8_t>>(size);
            case DataType::SHORT:
                return std::make_unique<TypedFITSData<int16_t>>(size);
            case DataType::INT:
                return std::make_unique<TypedFITSData<int32_t>>(size);
            case DataType::LONG:
                return std::make_unique<TypedFITSData<int64_t>>(size);
            case DataType::FLOAT:
                return std::make_unique<TypedFITSData<float>>(size);
            case DataType::DOUBLE:
                return std::make_unique<TypedFITSData<double>>(size);
            default:
                throw FITSDataException(FITSDataErrorCode::InvalidDataType,
                                        "Unsupported data type");
        }
    } catch (const std::bad_alloc&) {
        // Call getTypeSize safely now as it's defined above
        throw FITSDataException(
            FITSDataErrorCode::MemoryAllocationError,
            "Failed to allocate memory for FITS data of size " +
                getHumanReadableSize(size * getTypeSize(type)));
    }
}

template <FitsNumericType T>
TypedFITSData<T>::TypedFITSData(size_t initialSize)
    : isOptimized(false), compressed(false) {
    try {
        data.resize(initialSize);
    } catch (const std::bad_alloc&) {
        throw FITSDataException(
            FITSDataErrorCode::MemoryAllocationError,
            "Failed to allocate memory during TypedFITSData construction for "
            "size " +
                getHumanReadableSize(initialSize * sizeof(T)));
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::readData(std::ifstream& file, int64_t dataSize) {
    using namespace std::chrono;
    auto startTime = high_resolution_clock::now();

    if (dataSize <= 0) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidDataSize,
            "Invalid data size for reading: " + std::to_string(dataSize));
    }

    if (!file.good()) {
        throw FITSDataException(FITSDataErrorCode::StreamError,
                                "Invalid file stream for reading data");
    }

    reportProgress(0.0f, "Starting data read");

    const size_t elemCount = dataSize / sizeof(T);

    // Optimization: Use chunked reading for large datasets
    if (dataSize > 50 * 1024 * 1024) {  // If data is larger than 50MB
        readDataChunked(file, dataSize);
        return;
    }

    try {
        reportProgress(0.1f, "Allocating memory");
        data.resize(elemCount);

        reportProgress(0.2f, "Reading data");
        file.read(reinterpret_cast<char*>(data.data()), dataSize);

        if (!file.good()) {
            throw FITSDataException(FITSDataErrorCode::DataReadError,
                                    "Failed to read FITS data from file");
        }

        // FITS files store data in big-endian format
        reportProgress(0.7f, "Processing data format (endian swap if needed)");
        if constexpr (sizeof(T) > 1) {
            if (std::endian::native == std::endian::little) {
                std::for_each(std::execution::par_unseq, data.begin(),
                              data.end(), [](T& value) { swapEndian(value); });
            }
        }

        // Skip padding
        size_t padding = calculatePadding(dataSize);
        if (padding > 0) {
            file.ignore(padding);
        }

        auto endTime = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(endTime - startTime);
        reportProgress(1.0f, "Completed data read in " +
                                 std::to_string(duration.count()) + " ms");
    } catch (const std::bad_alloc&) {
        throw FITSDataException(
            FITSDataErrorCode::MemoryAllocationError,
            "Failed to allocate memory for FITS data of size " +
                getHumanReadableSize(dataSize));
    } catch (const std::exception& e) {
        // Catch specific exceptions if possible, rethrow others
        throw FITSDataException(FITSDataErrorCode::DataReadError,
                                "Error reading data: " + std::string(e.what()));
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::readChunk(std::ifstream& file, char* buffer,
                                 size_t size) {
    if (!buffer) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Invalid buffer provided for reading chunk");
    }

    file.read(buffer, static_cast<std::streamsize>(size));

    // Check for read errors specifically (failbit but not eofbit)
    if (file.fail() && !file.eof()) {
        throw FITSDataException(FITSDataErrorCode::DataReadError,
                                "Error reading data chunk from file stream");
    }
    // Note: Reaching EOF before reading 'size' bytes might be valid for the
    // last chunk. The caller (readDataChunked) handles the actual amount read
    // based on gcount(). However, the current implementation reads exactly
    // currentChunkSize, so failbit without eofbit indicates an error.
}

template <FitsNumericType T>
void TypedFITSData<T>::readDataChunked(std::ifstream& file, int64_t dataSize,
                                       size_t chunkSize) {
    using namespace std::chrono;
    auto startTime = high_resolution_clock::now();

    if (dataSize <= 0) {
        throw FITSDataException(FITSDataErrorCode::InvalidDataSize,
                                "Invalid data size for chunked reading: " +
                                    std::to_string(dataSize));
    }

    if (!file.good()) {
        throw FITSDataException(FITSDataErrorCode::StreamError,
                                "Invalid file stream for chunked reading");
    }

    // Ensure chunk size is a multiple of the element size T
    chunkSize = (chunkSize / sizeof(T)) * sizeof(T);
    if (chunkSize == 0) {
        // Use a default minimum chunk size if the provided one is too small
        chunkSize = std::max(static_cast<size_t>(4096),
                             sizeof(T));  // Example: 4KB or element size
        chunkSize =
            (chunkSize / sizeof(T)) * sizeof(T);  // Ensure multiple again
    }

    const size_t elemCount = dataSize / sizeof(T);
    const size_t chunkElemCount = chunkSize / sizeof(T);
    const size_t numChunks = (elemCount + chunkElemCount - 1) / chunkElemCount;

    reportProgress(0.0f, "Starting chunked data read");

    try {
        reportProgress(
            0.05f, "Allocating memory for " + getHumanReadableSize(dataSize));
        data.resize(elemCount);  // Pre-allocate the full size

        reportProgress(0.1f, "Reading data in " + std::to_string(numChunks) +
                                 " chunks of size " +
                                 getHumanReadableSize(chunkSize));

        std::vector<char> chunkBuffer(chunkSize);
        size_t totalBytesRead = 0;

        for (size_t i = 0; i < numChunks; ++i) {
            // Calculate the size of the current chunk
            size_t currentChunkSize =
                (i == numChunks - 1)
                    ? (static_cast<size_t>(dataSize) -
                       totalBytesRead)  // Last chunk might be smaller
                    : chunkSize;

            if (currentChunkSize == 0)
                continue;  // Should not happen with proper calculation, but
                           // safe check

            float progress = 0.1f + 0.7f * (static_cast<float>(i) / numChunks);
            reportProgress(progress, "Reading chunk " + std::to_string(i + 1) +
                                         "/" + std::to_string(numChunks));

            // Read the chunk data into the buffer
            readChunk(file, chunkBuffer.data(), currentChunkSize);
            // Check how many bytes were actually read (important for the last
            // chunk or if errors occurred)
            std::streamsize bytesReadInChunk = file.gcount();
            if (bytesReadInChunk !=
                static_cast<std::streamsize>(currentChunkSize)) {
                // If not EOF, it's an unexpected read error
                if (!file.eof()) {
                    throw FITSDataException(FITSDataErrorCode::DataReadError,
                                            "Failed to read the expected "
                                            "number of bytes for chunk " +
                                                std::to_string(i + 1));
                }
                // If it is EOF, update currentChunkSize to what was actually
                // read
                currentChunkSize = static_cast<size_t>(bytesReadInChunk);
                if (currentChunkSize == 0 &&
                    totalBytesRead < static_cast<size_t>(dataSize)) {
                    throw FITSDataException(
                        FITSDataErrorCode::DataReadError,
                        "Reached EOF prematurely while reading chunk " +
                            std::to_string(i + 1));
                }
            }

            // Calculate the number of elements in this chunk (based on actual
            // bytes read) size_t currentChunkElems = currentChunkSize /
            // sizeof(T); // Unused variable removed

            // Calculate the destination offset in the main data vector
            // size_t destOffset = i * chunkElemCount; // Unused variable
            // removed

            // Copy data from the chunk buffer to the correct position in the
            // main data vector Ensure we don't write past the allocated buffer
            if (totalBytesRead + currentChunkSize > data.size() * sizeof(T)) {
                throw FITSDataException(
                    FITSDataErrorCode::InternalError,
                    "Buffer overflow detected during chunked read. Offset: " +
                        std::to_string(totalBytesRead) +
                        ", Chunk Size: " + std::to_string(currentChunkSize));
            }
            std::memcpy(reinterpret_cast<char*>(data.data()) + totalBytesRead,
                        chunkBuffer.data(), currentChunkSize);

            totalBytesRead += currentChunkSize;
        }

        // Verify total bytes read match expected data size
        if (totalBytesRead != static_cast<size_t>(dataSize)) {
            throw FITSDataException(FITSDataErrorCode::DataReadError,
                                    "Mismatch between expected data size (" +
                                        std::to_string(dataSize) +
                                        ") and total bytes read (" +
                                        std::to_string(totalBytesRead) + ")");
        }

        // FITS files store data in big-endian format
        reportProgress(0.85f, "Processing data format (endian swap if needed)");
        if constexpr (sizeof(T) > 1) {
            if (std::endian::native == std::endian::little) {
                // Process endianness in parallel after all data is read
                std::for_each(std::execution::par_unseq, data.begin(),
                              data.end(), [](T& value) { swapEndian(value); });
            }
        }

        // Skip padding after reading all data
        size_t padding = calculatePadding(dataSize);
        if (padding > 0) {
            file.ignore(padding);
            if (!file.good()) {  // Check stream state after ignoring padding
                reportProgress(
                    0.95f,
                    "Warning: Stream error occurred after skipping padding.");
                // Decide if this is critical, maybe throw depending on
                // requirements
            }
        }

        auto endTime = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(endTime - startTime);
        reportProgress(1.0f, "Completed chunked data read in " +
                                 std::to_string(duration.count()) + " ms");
    } catch (const std::bad_alloc&) {
        throw FITSDataException(
            FITSDataErrorCode::MemoryAllocationError,
            "Failed to allocate memory for FITS data (chunked read) of size " +
                getHumanReadableSize(dataSize));
    } catch (const FITSDataException& e) {
        // Re-throw specific FITS exceptions
        throw;
    } catch (const std::exception& e) {
        // Wrap other standard exceptions
        throw FITSDataException(
            FITSDataErrorCode::DataReadError,
            "Error during chunked data read: " + std::string(e.what()));
    }
}

template <FitsNumericType T>
std::future<void> TypedFITSData<T>::readDataAsync(std::ifstream& file,
                                                  int64_t dataSize) {
    // Ensure the file stream object lives long enough for the async operation.
    // Capturing by reference is okay if the caller guarantees lifetime.
    // Consider passing file path and opening within the async task if lifetime
    // is an issue.
    return std::async(std::launch::async, [this, &file, dataSize]() {
        // Note: Exception handling within the async task is important.
        // The exception will be stored in the future and thrown when
        // future.get() is called.
        this->readData(file, dataSize);
    });
}

template <FitsNumericType T>
void TypedFITSData<T>::writeData(std::ofstream& file) const {
    using namespace std::chrono;
    auto startTime = high_resolution_clock::now();

    reportProgress(0.0f, "Starting data write");

    if (!file.good()) {
        throw FITSDataException(FITSDataErrorCode::StreamError,
                                "Invalid file stream for writing data");
    }

    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot write compressed data directly. Decompress first.");
    }

    if (data.empty()) {
        // Write empty data block (just padding to FITS_BLOCK_SIZE)
        reportProgress(0.2f, "Writing empty data block");
        std::vector<char> emptyBlock(FITS_BLOCK_SIZE,
                                     '\0');  // FITS standard uses 0 for padding
        // Note: An END card typically belongs to the header, not the data unit.
        // Writing just padding for an empty data unit is usually correct.
        file.write(emptyBlock.data(), emptyBlock.size());
        if (!file.good()) {
            throw FITSDataException(FITSDataErrorCode::DataWriteError,
                                    "Failed to write empty data block padding");
        }
        reportProgress(1.0f, "Completed writing empty data block");
        return;
    }

    try {
        const size_t dataSize = data.size() * sizeof(T);
        const T* dataPtr = data.data();
        std::vector<T> tempData;  // Used only if endian swap is needed

        // FITS files store data in big-endian format
        reportProgress(0.1f, "Preparing data format (endian swap if needed)");

        if constexpr (sizeof(T) > 1) {
            if (std::endian::native == std::endian::little) {
                // Create a temporary copy for byte swapping
                tempData =
                    data;  // Make a copy to avoid modifying original data
                std::for_each(std::execution::par_unseq, tempData.begin(),
                              tempData.end(),
                              [](T& value) { swapEndian(value); });
                dataPtr = tempData.data();  // Point to the swapped data
            }
            // If native is big-endian, dataPtr already points to the correct
            // data.
        }
        // For single-byte types, no swap is needed, dataPtr points to original
        // data.

        reportProgress(
            0.4f, "Writing " + getHumanReadableSize(dataSize) + " of data");

        file.write(reinterpret_cast<const char*>(dataPtr),
                   static_cast<std::streamsize>(dataSize));

        if (!file.good()) {
            throw FITSDataException(
                FITSDataErrorCode::DataWriteError,
                "Failed to write FITS data content to file");
        }

        // Pad the data to a multiple of FITS_BLOCK_SIZE bytes
        size_t padding = calculatePadding(dataSize);
        if (padding > 0) {
            reportProgress(0.8f, "Writing " + std::to_string(padding) +
                                     " bytes of padding");
            std::vector<char> paddingData(
                padding, '\0');  // FITS padding is typically null bytes
            file.write(paddingData.data(),
                       static_cast<std::streamsize>(padding));
            if (!file.good()) {
                throw FITSDataException(
                    FITSDataErrorCode::DataWriteError,
                    "Failed to write FITS data padding to file");
            }
        }

        auto endTime = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(endTime - startTime);
        reportProgress(1.0f, "Completed data write in " +
                                 std::to_string(duration.count()) + " ms");
    } catch (const std::bad_alloc&) {
        throw FITSDataException(FITSDataErrorCode::MemoryAllocationError,
                                "Memory allocation failed during data write "
                                "preparation (endian swap)");
    } catch (const FITSDataException& e) {
        // Re-throw specific FITS exceptions
        throw;
    } catch (const std::exception& e) {
        // Wrap other standard exceptions
        throw FITSDataException(FITSDataErrorCode::DataWriteError,
                                "Error writing data: " + std::string(e.what()));
    }
}

template <FitsNumericType T>
std::future<void> TypedFITSData<T>::writeDataAsync(std::ofstream& file) const {
    // Similar lifetime considerations as readDataAsync
    return std::async(std::launch::async, [this, &file]() {
        // Exceptions thrown here will be captured by the future
        this->writeData(file);
    });
}

template <FitsNumericType T>
DataType TypedFITSData<T>::getDataType() const noexcept {
    if constexpr (std::is_same_v<T, uint8_t>) {
        return DataType::BYTE;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return DataType::SHORT;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return DataType::INT;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return DataType::LONG;
    } else if constexpr (std::is_same_v<T, float>) {
        return DataType::FLOAT;
    } else if constexpr (std::is_same_v<T, double>) {
        return DataType::DOUBLE;
    } else {
        // This should be unreachable due to the FitsNumericType concept
        // constraint static_assert(false, "Unsupported data type used with
        // TypedFITSData");
        return {};  // Return a default or invalid value, though static_assert
                    // is better
    }
}

template <FitsNumericType T>
size_t TypedFITSData<T>::getElementCount() const noexcept {
    if (compressed) {
        // Return 0 or throw? Returning 0 might be misleading.
        // Throwing an exception seems more appropriate as the data isn't
        // directly accessible. However, the original didn't throw, let's return
        // 0 for now, but document this. Consider throwing
        // FITSDataErrorCode::InvalidOperation in future versions.
        return 0;  // Data is compressed, element count is not directly
                   // available
    }
    return data.size();
}

template <FitsNumericType T>
size_t TypedFITSData<T>::getDataSizeBytes() const noexcept {
    if (compressed) {
        return compressedData.size();  // Return the size of the compressed data
    }
    return data.size() * sizeof(T);  // Return the size of the uncompressed data
}

template <FitsNumericType T>
size_t TypedFITSData<T>::getCompressedSize() const noexcept {
    if (compressed) {
        return compressedData.size();  // Return the size of the compressed data
    }
    return 0;  // Return 0 if data is not compressed
}

template <FitsNumericType T>
template <typename U>
void TypedFITSData<T>::swapEndian(U& value) noexcept {
    // Ensure this is only called for types larger than 1 byte
    if constexpr (sizeof(U) > 1) {
        auto* bytes = reinterpret_cast<std::byte*>(&value);
        std::reverse(bytes, bytes + sizeof(U));
    }
}

template <FitsNumericType T>
T TypedFITSData<T>::getMinValue() const {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot get minimum value of compressed data. Decompress first.");
    }

    if (data.empty()) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Cannot get minimum value of empty data");
    }
    // Use parallel execution for potentially large datasets
    return *std::min_element(std::execution::par_unseq, data.begin(),
                             data.end());
}

template <FitsNumericType T>
T TypedFITSData<T>::getMaxValue() const {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot get maximum value of compressed data. Decompress first.");
    }

    if (data.empty()) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Cannot get maximum value of empty data");
    }
    // Use parallel execution for potentially large datasets
    return *std::max_element(std::execution::par_unseq, data.begin(),
                             data.end());
}

template <FitsNumericType T>
double TypedFITSData<T>::getMean() const {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot calculate mean of compressed data. Decompress first.");
    }

    if (data.empty()) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Cannot calculate mean of empty data");
    }

    // Using Kahan summation algorithm for better precision with large datasets
    double sum = 0.0;
    double c = 0.0;  // A running compensation for lost low-order bits.

    for (const T& val : data) {
        // Note: Potential overflow if T is large integer and sum exceeds double
        // limits, but generally safer than direct summation for floating point
        // types.
        double y = static_cast<double>(val) -
                   c;        // c is subtracted to correct low-order bits
        double t = sum + y;  // Sum might be distorted
        c = (t - sum) - y;   // Algebraically, c should be zero. Effectively
                             // recovers lost bits.
        sum = t;             // Corrected sum
    }

    return sum / data.size();
}

template <FitsNumericType T>
double TypedFITSData<T>::getStdDev() const {
    if (compressed) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Cannot calculate standard deviation of "
                                "compressed data. Decompress first.");
    }

    size_t count = data.size();
    if (count < 2) {
        // Standard deviation is undefined for less than 2 elements
        // Returning 0 or throwing are options. Throwing is less ambiguous.
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot calculate standard deviation with less than 2 data points");
    }

    double mean = getMean();  // Calculate mean first

    // Use two-pass algorithm with double precision for variance calculation
    // to improve numerical stability compared to single-pass algorithms.
    double sum_sq_diff = 0.0;
    double c = 0.0;  // Kahan summation compensation for squared differences

    for (const T& val : data) {
        double diff = static_cast<double>(val) - mean;
        double diff_sq = diff * diff;
        double y = diff_sq - c;
        double t = sum_sq_diff + y;
        c = (t - sum_sq_diff) - y;
        sum_sq_diff = t;
    }

    // Use sample standard deviation ( Bessel's correction n-1 )
    double variance = sum_sq_diff / (count - 1);

    return std::sqrt(variance);
}

template <FitsNumericType T>
bool TypedFITSData<T>::hasNaN() const {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot check for NaN in compressed data. Decompress first.");
    }

    if constexpr (std::is_floating_point_v<T>) {
        // Use parallel execution for potentially large datasets
        return std::any_of(std::execution::par_unseq, data.begin(), data.end(),
                           [](T val) { return std::isnan(val); });
    }
    return false;  // Non-floating point types cannot be NaN
}

template <FitsNumericType T>
bool TypedFITSData<T>::hasInfinity() const {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot check for infinity in compressed data. Decompress first.");
    }

    if constexpr (std::is_floating_point_v<T>) {
        // Use parallel execution for potentially large datasets
        return std::any_of(std::execution::par_unseq, data.begin(), data.end(),
                           [](T val) { return std::isinf(val); });
    }
    return false;  // Non-floating point types cannot be Infinity
}

template <FitsNumericType T>
void TypedFITSData<T>::validateData() {
    if (compressed) {
        // Option 1: Decompress, validate, recompress (potentially slow)
        // Option 2: Throw, requiring user to decompress first (current
        // approach)
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot validate compressed data. Decompress first.");

        // // Option 1 Implementation (if chosen):
        // try {
        //     reportProgress(0.0f, "Decompressing for validation");
        //     decompress(); // Temporarily decompress
        //     validateData(); // Validate the uncompressed data (recursive call
        //     without this block) reportProgress(0.9f, "Recompressing after
        //     validation"); compress();  // Restore compressed state
        //     reportProgress(1.0f, "Validation of compressed data complete");
        //     return;
        // } catch (const std::exception& e) {
        //     // Ensure state consistency if validation/recompression fails
        //     compressed = false; // Assume decompression succeeded but
        //     validation/recompression failed compressedData.clear();
        //     compressedData.shrink_to_fit();
        //     throw FITSDataException(
        //         FITSDataErrorCode::DataValidationError,
        //         "Failed to validate/recompress data: " +
        //         std::string(e.what()));
        // }
    }

    // Check floating point data for invalid values (NaN, Infinity)
    if constexpr (std::is_floating_point_v<T>) {
        reportProgress(0.0f,
                       "Validating floating-point data for invalid values");

        bool hasNaNValues = false;
        bool hasInfValues = false;

        // Check in parallel if supported and beneficial
        // Note: Simple any_of might be faster than setting up parallel sections
        // for this. Reverting to simpler check unless performance profiling
        // shows benefit.
        hasNaNValues = hasNaN();       // Uses parallel any_of internally now
        hasInfValues = hasInfinity();  // Uses parallel any_of internally now

        if (hasNaNValues) {
            throw FITSDataException(
                FITSDataErrorCode::DataValidationError,
                "Data validation failed: Contains NaN values");
        }

        if (hasInfValues) {
            throw FITSDataException(
                FITSDataErrorCode::DataValidationError,
                "Data validation failed: Contains infinity values");
        }

        reportProgress(1.0f,
                       "Floating-point data validation completed successfully");
    } else {
        reportProgress(1.0f,
                       "Data validation skipped (not floating-point type)");
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::optimizeMemory() {
    if (!isOptimized) {
        if (compressed) {
            compressedData.shrink_to_fit();
        } else {
            data.shrink_to_fit();
        }
        isOptimized = true;  // Mark as optimized
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::reserveCapacity(size_t capacity) {
    if (compressed) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Cannot reserve capacity for compressed data.");
    }
    try {
        reportProgress(0.0f, "Reserving capacity for " +
                                 getHumanReadableSize(capacity * sizeof(T)));
        data.reserve(capacity);
        isOptimized = false;  // Reserving capacity might de-optimize
        reportProgress(1.0f, "Capacity reserved successfully.");
    } catch (const std::bad_alloc&) {
        throw FITSDataException(FITSDataErrorCode::MemoryAllocationError,
                                "Failed to reserve memory capacity of " +
                                    getHumanReadableSize(capacity * sizeof(T)));
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::compress() {
    if (compressed || data.empty()) {
        reportProgress(
            1.0f, "Compression skipped (already compressed or data empty)");
        return;  // Already compressed or nothing to compress
    }

    try {
        reportProgress(0.0f, "Starting data compression");

        const size_t dataSize = data.size() * sizeof(T);
        // Calculate required buffer size for compressed data
        uLongf compressedBufSize = compressBound(dataSize);
        compressedData.resize(compressedBufSize);

        reportProgress(0.3f, "Compressing " + getHumanReadableSize(dataSize) +
                                 " using zlib");
        // Perform compression
        uLongf actualCompressedSize =
            compressedBufSize;  // Pass the buffer size
        int result = compress2(
            reinterpret_cast<Bytef*>(compressedData.data()),
            &actualCompressedSize, reinterpret_cast<const Bytef*>(data.data()),
            dataSize, Z_BEST_COMPRESSION);  // Use a suitable compression level

        if (result != Z_OK) {
            compressedData
                .clear();  // Clear potentially partially filled buffer
            throw FITSDataException(
                FITSDataErrorCode::CompressionError,
                "Data compression failed with zlib error code: " +
                    std::to_string(result));
        }

        // Resize buffer to actual compressed size
        compressedData.resize(actualCompressedSize);
        compressedData.shrink_to_fit();  // Optimize memory usage

        reportProgress(0.8f, "Freeing original uncompressed data");
        data.clear();
        data.shrink_to_fit();
        compressed = true;
        isOptimized = true;  // Compressed data is considered optimized

        float compressionRatio =
            (actualCompressedSize > 0)
                ? static_cast<float>(dataSize) / actualCompressedSize
                : 0.0f;
        reportProgress(1.0f, "Completed compression. Ratio: " +
                                 std::to_string(compressionRatio) + ":1");
    } catch (const std::exception& e) {
        throw FITSDataException(
            FITSDataErrorCode::CompressionError,
            "Data compression failed: " + std::string(e.what()));
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::decompress() {
    if (!compressed) {
        reportProgress(1.0f, "Decompression skipped (data not compressed)");
        return;  // Already decompressed or nothing to decompress
    }

    try {
        reportProgress(0.0f, "Starting data decompression");

        // Estimate the original size (FITS data might expand significantly)
        size_t estimatedOriginalSize = compressedData.size() * 8;

        reportProgress(0.2f, "Allocating memory for decompressed data");
        data.resize(estimatedOriginalSize / sizeof(T));

        uLongf decompressedSize = estimatedOriginalSize;

        reportProgress(0.4f, "Decompressing data");
        int result =
            uncompress(reinterpret_cast<Bytef*>(data.data()), &decompressedSize,
                       compressedData.data(), compressedData.size());

        // Retry with larger buffer if needed
        if (result == Z_BUF_ERROR) {
            reportProgress(0.5f, "Retrying with larger buffer");
            estimatedOriginalSize *= 2;
            data.resize(estimatedOriginalSize / sizeof(T));
            decompressedSize = estimatedOriginalSize;

            result = uncompress(reinterpret_cast<Bytef*>(data.data()),
                                &decompressedSize, compressedData.data(),
                                compressedData.size());
        }

        if (result != Z_OK) {
            throw FITSDataException(
                FITSDataErrorCode::CompressionError,
                "Data decompression failed with zlib error code: " +
                    std::to_string(result));
        }

        // Resize to actual decompressed size
        data.resize(decompressedSize / sizeof(T));

        reportProgress(0.8f, "Freeing compressed data");
        compressedData.clear();
        compressedData.shrink_to_fit();
        compressed = false;

        reportProgress(1.0f, "Decompression completed successfully");
    } catch (const std::exception& e) {
        throw FITSDataException(
            FITSDataErrorCode::CompressionError,
            "Data decompression failed: " + std::string(e.what()));
    }
}

template <FitsNumericType T>
size_t TypedFITSData<T>::tryRecover(bool fixNaN, bool fixInfinity,
                                    T replacementValue) {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot recover compressed data. Decompress first.");
    }

    size_t fixedCount = 0;

    if constexpr (std::is_floating_point_v<T>) {
        reportProgress(0.0f, "Starting data recovery");

        if (fixNaN || fixInfinity) {
            reportProgress(0.2f, "Scanning for invalid values");

#pragma omp parallel for reduction(+ : fixedCount)
            for (size_t i = 0; i < data.size(); ++i) {
                bool needsFix = (fixNaN && std::isnan(data[i])) ||
                                (fixInfinity && std::isinf(data[i]));

                if (needsFix) {
                    data[i] = replacementValue;
                    fixedCount++;
                }
            }
        }

        if (fixedCount > 0) {
            reportProgress(1.0f, "Recovered " + std::to_string(fixedCount) +
                                     " invalid values");
        } else {
            reportProgress(1.0f, "No invalid values found");
        }
    }

    return fixedCount;
}

template <FitsNumericType T>
void TypedFITSData<T>::transform(const std::function<T(T)>& func) {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot transform compressed data. Decompress first.");
    }

    reportProgress(0.0f, "Starting data transformation");
    std::transform(data.begin(), data.end(), data.begin(), func);
    reportProgress(1.0f, "Data transformation completed");
}

template <FitsNumericType T>
void TypedFITSData<T>::transformParallel(const std::function<T(T)>& func) {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot transform compressed data. Decompress first.");
    }

    reportProgress(0.0f, "Starting parallel data transformation");
    std::transform(std::execution::par_unseq, data.begin(), data.end(),
                   data.begin(), func);
    reportProgress(1.0f, "Parallel data transformation completed");
}

template <FitsNumericType T>
void TypedFITSData<T>::normalize(T minVal, T maxVal) {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot normalize compressed data. Decompress first.");
    }

    if (data.empty()) {
        throw FITSDataException(FITSDataErrorCode::InvalidOperation,
                                "Cannot normalize empty data");
    }

    reportProgress(0.0f, "Starting data normalization");

    reportProgress(0.1f, "Finding min/max values");
    T currentMin = getMinValue();
    T currentMax = getMaxValue();
    T range = currentMax - currentMin;

    if (range == 0) {
        reportProgress(0.5f, "Data has uniform values, setting to minimum");
        std::fill(data.begin(), data.end(), minVal);
        reportProgress(1.0f, "Normalization completed (uniform data)");
        return;
    }

    T targetRange = maxVal - minVal;

    reportProgress(0.2f, "Applying normalization transform");
    transformParallel([=](T val) {
        return static_cast<T>(minVal +
                              (val - currentMin) * targetRange / range);
    });

    reportProgress(1.0f, "Normalization completed");
}

template <FitsNumericType T>
void TypedFITSData<T>::scale(double factor) {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot scale compressed data. Decompress first.");
    }

    reportProgress(0.0f, "Starting data scaling");
    transformParallel([factor](T val) { return static_cast<T>(val * factor); });
    reportProgress(1.0f,
                   "Scaling completed with factor " + std::to_string(factor));
}

template <FitsNumericType T>
template <FitsNumericType U>
std::vector<U> TypedFITSData<T>::convertTo() const {
    if (compressed) {
        throw FITSDataException(
            FITSDataErrorCode::InvalidOperation,
            "Cannot convert compressed data. Decompress first.");
    }

    reportProgress(0.0f, "Starting data type conversion");

    std::vector<U> result;
    try {
        result.reserve(data.size());
    } catch (const std::bad_alloc&) {
        throw FITSDataException(
            FITSDataErrorCode::MemoryAllocationError,
            "Failed to allocate memory for data conversion");
    }

    reportProgress(0.2f, "Converting data");
    std::transform(data.begin(), data.end(), std::back_inserter(result),
                   [](T val) { return static_cast<U>(val); });

    reportProgress(1.0f, "Data conversion completed");
    return result;
}

// Explicit template instantiations for all supported types and conversion
// combinations
template class TypedFITSData<uint8_t>;
template class TypedFITSData<int16_t>;
template class TypedFITSData<int32_t>;
template class TypedFITSData<int64_t>;
template class TypedFITSData<float>;
template class TypedFITSData<double>;

// Type conversion combinations
template std::vector<uint8_t> TypedFITSData<uint8_t>::convertTo<uint8_t>()
    const;
template std::vector<int16_t> TypedFITSData<uint8_t>::convertTo<int16_t>()
    const;
template std::vector<int32_t> TypedFITSData<uint8_t>::convertTo<int32_t>()
    const;
template std::vector<int64_t> TypedFITSData<uint8_t>::convertTo<int64_t>()
    const;
template std::vector<float> TypedFITSData<uint8_t>::convertTo<float>() const;
template std::vector<double> TypedFITSData<uint8_t>::convertTo<double>() const;

template std::vector<uint8_t> TypedFITSData<int16_t>::convertTo<uint8_t>()
    const;
template std::vector<int16_t> TypedFITSData<int16_t>::convertTo<int16_t>()
    const;
template std::vector<int32_t> TypedFITSData<int16_t>::convertTo<int32_t>()
    const;
template std::vector<int64_t> TypedFITSData<int16_t>::convertTo<int64_t>()
    const;
template std::vector<float> TypedFITSData<int16_t>::convertTo<float>() const;
template std::vector<double> TypedFITSData<int16_t>::convertTo<double>() const;

template std::vector<uint8_t> TypedFITSData<int32_t>::convertTo<uint8_t>()
    const;
template std::vector<int16_t> TypedFITSData<int32_t>::convertTo<int16_t>()
    const;
template std::vector<int32_t> TypedFITSData<int32_t>::convertTo<int32_t>()
    const;
template std::vector<int64_t> TypedFITSData<int32_t>::convertTo<int64_t>()
    const;
template std::vector<float> TypedFITSData<int32_t>::convertTo<float>() const;
template std::vector<double> TypedFITSData<int32_t>::convertTo<double>() const;

template std::vector<uint8_t> TypedFITSData<int64_t>::convertTo<uint8_t>()
    const;
template std::vector<int16_t> TypedFITSData<int64_t>::convertTo<int16_t>()
    const;
template std::vector<int32_t> TypedFITSData<int64_t>::convertTo<int32_t>()
    const;
template std::vector<int64_t> TypedFITSData<int64_t>::convertTo<int64_t>()
    const;
template std::vector<float> TypedFITSData<int64_t>::convertTo<float>() const;
template std::vector<double> TypedFITSData<int64_t>::convertTo<double>() const;

template std::vector<uint8_t> TypedFITSData<float>::convertTo<uint8_t>() const;
template std::vector<int16_t> TypedFITSData<float>::convertTo<int16_t>() const;
template std::vector<int32_t> TypedFITSData<float>::convertTo<int32_t>() const;
template std::vector<int64_t> TypedFITSData<float>::convertTo<int64_t>() const;
template std::vector<float> TypedFITSData<float>::convertTo<float>() const;
template std::vector<double> TypedFITSData<float>::convertTo<double>() const;

template std::vector<uint8_t> TypedFITSData<double>::convertTo<uint8_t>() const;
template std::vector<int16_t> TypedFITSData<double>::convertTo<int16_t>() const;
template std::vector<int32_t> TypedFITSData<double>::convertTo<int32_t>() const;
template std::vector<int64_t> TypedFITSData<double>::convertTo<int64_t>() const;
template std::vector<float> TypedFITSData<double>::convertTo<float>() const;
template std::vector<double> TypedFITSData<double>::convertTo<double>() const;
