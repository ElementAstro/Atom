#include "fits_data.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
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

// Explicit template instantiations
template class TypedFITSData<uint8_t>;
template class TypedFITSData<int16_t>;
template class TypedFITSData<int32_t>;
template class TypedFITSData<int64_t>;
template class TypedFITSData<float>;
template class TypedFITSData<double>;
