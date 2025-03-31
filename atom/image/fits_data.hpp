#ifndef ATOM_IMAGE_FITS_DATA_HPP
#define ATOM_IMAGE_FITS_DATA_HPP

#include <concepts>
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

/**
 * @class FITSDataException
 * @brief Exception class for FITS data operations.
 */
class FITSDataException : public std::runtime_error {
public:
    explicit FITSDataException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @enum DataType
 * @brief Enum representing different data types that can be stored in FITS
 * files.
 */
enum class DataType { BYTE, SHORT, INT, LONG, FLOAT, DOUBLE };

// Concept for FITS numeric types
template <typename T>
concept FitsNumericType =
    std::same_as<T, uint8_t> || std::same_as<T, int16_t> ||
    std::same_as<T, int32_t> || std::same_as<T, int64_t> ||
    std::same_as<T, float> || std::same_as<T, double>;

/**
 * @class FITSData
 * @brief Abstract base class for handling FITS data.
 */
class FITSData {
public:
    /**
     * @brief Virtual destructor for FITSData.
     */
    virtual ~FITSData() = default;

    /**
     * @brief Pure virtual function to read data from a file.
     * @param file The input file stream to read data from.
     * @param dataSize The size of the data to read.
     * @throws FITSDataException If there is an error reading data
     */
    virtual void readData(std::ifstream& file, int64_t dataSize) = 0;

    /**
     * @brief Pure virtual function to write data to a file.
     * @param file The output file stream to write data to.
     * @throws FITSDataException If there is an error writing data
     */
    virtual void writeData(std::ofstream& file) const = 0;

    /**
     * @brief Pure virtual function to get the data type.
     * @return The data type of the FITS data.
     */
    [[nodiscard]] virtual DataType getDataType() const noexcept = 0;

    /**
     * @brief Pure virtual function to get the number of elements in the data.
     * @return The number of elements in the data.
     */
    [[nodiscard]] virtual size_t getElementCount() const noexcept = 0;

    /**
     * @brief Pure virtual function to get the data size in bytes.
     * @return The size in bytes of the data.
     */
    [[nodiscard]] virtual size_t getDataSizeBytes() const noexcept = 0;

    /**
     * @brief Creates a new FITSData instance of the specified type.
     * @param type The data type for the new instance.
     * @return A unique pointer to the new FITSData instance.
     * @throws std::invalid_argument If the data type is not supported.
     */
    [[nodiscard]] static std::unique_ptr<FITSData> createData(DataType type);
};

/**
 * @class TypedFITSData
 * @brief Template class for handling typed FITS data.
 * @tparam T The data type of the elements.
 */
template <FitsNumericType T>
class TypedFITSData : public FITSData {
public:
    /**
     * @brief Default constructor.
     */
    TypedFITSData() = default;

    /**
     * @brief Constructor with initial data.
     * @param initialData The initial data to store.
     */
    explicit TypedFITSData(std::vector<T> initialData)
        : data(std::move(initialData)) {}

    /**
     * @brief Constructor with size.
     * @param size The number of elements to allocate.
     * @param initialValue The initial value for elements (default 0).
     */
    explicit TypedFITSData(size_t size, T initialValue = T{})
        : data(size, initialValue) {}

    /**
     * @brief Reads data from a file.
     * @param file The input file stream to read data from.
     * @param dataSize The size of the data to read.
     * @throws FITSDataException If there is an error reading data
     */
    void readData(std::ifstream& file, int64_t dataSize) override;

    /**
     * @brief Writes data to a file.
     * @param file The output file stream to write data to.
     * @throws FITSDataException If there is an error writing data
     */
    void writeData(std::ofstream& file) const override;

    /**
     * @brief Gets the data type.
     * @return The data type of the FITS data.
     */
    [[nodiscard]] DataType getDataType() const noexcept override;

    /**
     * @brief Gets the number of elements in the data.
     * @return The number of elements in the data.
     */
    [[nodiscard]] size_t getElementCount() const noexcept override;

    /**
     * @brief Gets the data size in bytes.
     * @return The size in bytes of the data.
     */
    [[nodiscard]] size_t getDataSizeBytes() const noexcept override;

    /**
     * @brief Gets the data as a constant reference.
     * @return A constant reference to the data vector.
     */
    [[nodiscard]] const std::vector<T>& getData() const noexcept {
        return data;
    }

    /**
     * @brief Gets the data as a reference.
     * @return A reference to the data vector.
     */
    std::vector<T>& getData() noexcept { return data; }

    /**
     * @brief Gets a span view of the data (non-const).
     * @return A span of the data.
     */
    [[nodiscard]] std::span<T> getDataSpan() noexcept {
        return std::span<T>(data);
    }

    /**
     * @brief Gets a const span view of the data.
     * @return A const span of the data.
     */
    [[nodiscard]] std::span<const T> getDataSpan() const noexcept {
        return std::span<const T>(data);
    }

    /**
     * @brief Gets the minimum value in the data.
     * @return The minimum value.
     */
    [[nodiscard]] T getMinValue() const;

    /**
     * @brief Gets the maximum value in the data.
     * @return The maximum value.
     */
    [[nodiscard]] T getMaxValue() const;

    /**
     * @brief Gets the mean value of the data.
     * @return The mean value.
     */
    [[nodiscard]] double getMean() const;

    /**
     * @brief Gets the standard deviation of the data.
     * @return The standard deviation.
     */
    [[nodiscard]] double getStdDev() const;

    /**
     * @brief Checks if the data contains NaN values.
     * @return True if NaN values are present, false otherwise.
     */
    [[nodiscard]] bool hasNaN() const;

    /**
     * @brief Checks if the data contains infinity values.
     * @return True if infinity values are present, false otherwise.
     */
    [[nodiscard]] bool hasInfinity() const;

    /**
     * @brief Validates the data for consistency.
     * @throws FITSDataException If the data is invalid.
     */
    void validateData();

    /**
     * @brief Optimizes memory usage for the data.
     */
    void optimizeMemory();

    /**
     * @brief Reserves capacity for the data vector.
     * @param capacity The capacity to reserve.
     */
    void reserveCapacity(size_t capacity);

    /**
     * @brief Compresses the data.
     */
    void compress();

    /**
     * @brief Decompresses the data.
     */
    void decompress();

    /**
     * @brief Checks if the data is compressed.
     * @return True if the data is compressed, false otherwise.
     */
    [[nodiscard]] bool isCompressed() const noexcept { return compressed; }

    /**
     * @brief Applies a transformation function to the data.
     * @param func The transformation function.
     */
    void transform(const std::function<T(T)>& func);

    /**
     * @brief Applies a transformation function to the data in parallel.
     * @param func The transformation function.
     */
    void transformParallel(const std::function<T(T)>& func);

    /**
     * @brief Normalizes the data to a specified range.
     * @param minVal The minimum value of the range.
     * @param maxVal The maximum value of the range.
     */
    void normalize(T minVal, T maxVal);

    /**
     * @brief Scales the data by a specified factor.
     * @param factor The scaling factor.
     */
    void scale(double factor);

    /**
     * @brief Converts the data to another type.
     * @tparam U The target type.
     * @return A vector of the converted data.
     */
    template <FitsNumericType U>
    std::vector<U> convertTo() const;

private:
    std::vector<T> data;       ///< The data vector.
    bool isOptimized = false;  ///< Flag indicating if memory is optimized.
    bool compressed = false;   ///< Flag indicating if the data is compressed.
    std::vector<uint8_t> compressedData;  ///< Compressed data storage.

    /**
     * @brief Swaps the endianness of a value.
     * @tparam U The type of the value.
     * @param value The value to swap endianness.
     */
    template <typename U>
    static void swapEndian(U& value) noexcept;
};

#endif  // ATOM_IMAGE_FITS_DATA_HPP
