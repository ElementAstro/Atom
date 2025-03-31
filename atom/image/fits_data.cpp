#include "fits_data.hpp"

#include <zlib.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <execution>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
constexpr size_t FITS_BLOCK_SIZE = 2880;

// Helper function to calculate padding
[[nodiscard]] size_t calculatePadding(size_t dataSize) noexcept {
    return (FITS_BLOCK_SIZE - (dataSize % FITS_BLOCK_SIZE)) % FITS_BLOCK_SIZE;
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
            throw std::invalid_argument("Unsupported data type");
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::readData(std::ifstream& file, int64_t dataSize) {
    if (dataSize <= 0) {
        throw FITSDataException("Invalid data size for reading");
    }

    if (!file.good()) {
        throw FITSDataException("Invalid file stream for reading data");
    }

    const size_t elemCount = dataSize / sizeof(T);
    data.resize(elemCount);

    try {
        file.read(reinterpret_cast<char*>(data.data()), dataSize);

        if (!file.good()) {
            throw FITSDataException("Failed to read FITS data from file");
        }

        // FITS files store data in big-endian format
        if constexpr (sizeof(T) > 1) {
            if (std::endian::native == std::endian::little) {
                std::for_each(data.begin(), data.end(),
                              [](T& value) { swapEndian(value); });
            }
        }

        // Skip padding
        size_t padding = calculatePadding(dataSize);
        if (padding > 0) {
            file.ignore(padding);
        }
    } catch (const std::exception& e) {
        throw FITSDataException("Error reading data: " + std::string(e.what()));
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::writeData(std::ofstream& file) const {
    if (!file.good()) {
        throw FITSDataException("Invalid file stream for writing data");
    }

    if (data.empty()) {
        // Write empty data block with END card
        const std::string end = "END";
        std::vector<char> emptyBlock(FITS_BLOCK_SIZE, ' ');
        std::copy(end.begin(), end.end(), emptyBlock.begin());
        file.write(emptyBlock.data(), emptyBlock.size());
        return;
    }

    try {
        // FITS files store data in big-endian format
        std::vector<T> tempData = data;

        if constexpr (sizeof(T) > 1) {
            if (std::endian::native == std::endian::little) {
                std::for_each(tempData.begin(), tempData.end(),
                              [](T& value) { swapEndian(value); });
            }
        }

        const size_t dataSize = tempData.size() * sizeof(T);
        file.write(reinterpret_cast<const char*>(tempData.data()),
                   static_cast<std::streamsize>(dataSize));

        // Pad the data to a multiple of FITS_BLOCK_SIZE bytes
        size_t padding = calculatePadding(dataSize);
        if (padding > 0) {
            std::vector<char> paddingData(padding, '\0');
            file.write(paddingData.data(),
                       static_cast<std::streamsize>(padding));
        }

        if (!file.good()) {
            throw FITSDataException("Failed to write FITS data to file");
        }
    } catch (const std::exception& e) {
        throw FITSDataException("Error writing data: " + std::string(e.what()));
    }
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
        static_assert(std::is_same_v<T, void>, "Unsupported data type");
        return {};  // Unreachable, needed for compilation
    }
}

template <FitsNumericType T>
size_t TypedFITSData<T>::getElementCount() const noexcept {
    return data.size();
}

template <FitsNumericType T>
size_t TypedFITSData<T>::getDataSizeBytes() const noexcept {
    return data.size() * sizeof(T);
}

template <FitsNumericType T>
template <typename U>
void TypedFITSData<T>::swapEndian(U& value) noexcept {
    auto* bytes = reinterpret_cast<std::byte*>(&value);
    std::reverse(bytes, bytes + sizeof(U));
}

template <FitsNumericType T>
T TypedFITSData<T>::getMinValue() const {
    if (data.empty()) {
        throw FITSDataException("Cannot get minimum value of empty data");
    }
    return *std::min_element(data.begin(), data.end());
}

template <FitsNumericType T>
T TypedFITSData<T>::getMaxValue() const {
    if (data.empty()) {
        throw FITSDataException("Cannot get maximum value of empty data");
    }
    return *std::max_element(data.begin(), data.end());
}

template <FitsNumericType T>
double TypedFITSData<T>::getMean() const {
    if (data.empty()) {
        throw FITSDataException("Cannot calculate mean of empty data");
    }
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / data.size();
}

template <FitsNumericType T>
double TypedFITSData<T>::getStdDev() const {
    if (data.size() < 2) {
        throw FITSDataException(
            "Cannot calculate standard deviation with less than 2 elements");
    }
    double mean = getMean();
    double sumSquares = std::accumulate(data.begin(), data.end(), 0.0,
                                        [mean](double acc, T val) {
                                            double diff = val - mean;
                                            return acc + diff * diff;
                                        });
    return std::sqrt(sumSquares / (data.size() - 1));
}

template <FitsNumericType T>
bool TypedFITSData<T>::hasNaN() const {
    if constexpr (std::is_floating_point_v<T>) {
        return std::any_of(data.begin(), data.end(),
                           [](T val) { return std::isnan(val); });
    }
    return false;
}

template <FitsNumericType T>
bool TypedFITSData<T>::hasInfinity() const {
    if constexpr (std::is_floating_point_v<T>) {
        return std::any_of(data.begin(), data.end(),
                           [](T val) { return std::isinf(val); });
    }
    return false;
}

template <FitsNumericType T>
void TypedFITSData<T>::validateData() {
    if constexpr (std::is_floating_point_v<T>) {
        if (hasNaN()) {
            throw FITSDataException("Data contains NaN values");
        }
        if (hasInfinity()) {
            throw FITSDataException("Data contains infinity values");
        }
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::optimizeMemory() {
    if (!isOptimized) {
        data.shrink_to_fit();
        isOptimized = true;
    }
}

template <FitsNumericType T>
void TypedFITSData<T>::reserveCapacity(size_t capacity) {
    data.reserve(capacity);
}

template <FitsNumericType T>
void TypedFITSData<T>::compress() {
    if (compressed || data.empty())
        return;

    const size_t dataSize = data.size() * sizeof(T);
    compressedData.resize(compressBound(dataSize));
    uLongf compressedSize = compressedData.size();

    if (compress2(compressedData.data(), &compressedSize,
                  reinterpret_cast<const Bytef*>(data.data()), dataSize,
                  Z_BEST_COMPRESSION) != Z_OK) {
        throw FITSDataException("Data compression failed");
    }

    compressedData.resize(compressedSize);
    data.clear();
    data.shrink_to_fit();
    compressed = true;
}

template <FitsNumericType T>
void TypedFITSData<T>::decompress() {
    if (!compressed)
        return;

    const size_t originalSize = compressedData.size() * sizeof(T);
    std::vector<T> decompressedData(originalSize / sizeof(T));
    uLongf decompressedSize = originalSize;

    if (uncompress(reinterpret_cast<Bytef*>(decompressedData.data()),
                   &decompressedSize, compressedData.data(),
                   compressedData.size()) != Z_OK) {
        throw FITSDataException("Data decompression failed");
    }

    data = std::move(decompressedData);
    compressedData.clear();
    compressedData.shrink_to_fit();
    compressed = false;
}

template <FitsNumericType T>
void TypedFITSData<T>::transform(const std::function<T(T)>& func) {
    if (compressed)
        decompress();
    std::transform(data.begin(), data.end(), data.begin(), func);
}

template <FitsNumericType T>
void TypedFITSData<T>::transformParallel(const std::function<T(T)>& func) {
    if (compressed)
        decompress();
    std::transform(std::execution::par_unseq, data.begin(), data.end(),
                   data.begin(), func);
}

template <FitsNumericType T>
void TypedFITSData<T>::normalize(T minVal, T maxVal) {
    if (compressed)
        decompress();
    if (data.empty())
        return;

    T currentMin = getMinValue();
    T currentMax = getMaxValue();
    T range = currentMax - currentMin;

    if (range == 0) {
        std::fill(data.begin(), data.end(), minVal);
        return;
    }

    T targetRange = maxVal - minVal;
    transform([=](T val) {
        return static_cast<T>(minVal +
                              (val - currentMin) * targetRange / range);
    });
}

template <FitsNumericType T>
void TypedFITSData<T>::scale(double factor) {
    if (compressed)
        decompress();
    transform([factor](T val) { return static_cast<T>(val * factor); });
}

template <FitsNumericType T>
template <FitsNumericType U>
std::vector<U> TypedFITSData<T>::convertTo() const {
    if (compressed) {
        throw FITSDataException("Cannot convert compressed data");
    }

    std::vector<U> result;
    result.reserve(data.size());

    std::transform(data.begin(), data.end(), std::back_inserter(result),
                   [](T val) { return static_cast<U>(val); });

    return result;
}

// Explicit template instantiations
template class TypedFITSData<uint8_t>;
template class TypedFITSData<int16_t>;
template class TypedFITSData<int32_t>;
template class TypedFITSData<int64_t>;
template class TypedFITSData<float>;
template class TypedFITSData<double>;
