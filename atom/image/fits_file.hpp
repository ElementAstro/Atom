#ifndef ATOM_IMAGE_FITS_FILE_HPP
#define ATOM_IMAGE_FITS_FILE_HPP

#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "hdu.hpp"

/**
 * @enum FITSErrorCode
 * @brief Error codes for FITS file operations.
 */
enum class FITSErrorCode {
    Success = 0,
    FileNotExist,
    FileNotAccessible,
    InvalidFormat,
    ReadError,
    WriteError,
    MemoryError,
    CompressionError,
    CorruptedData,
    UnsupportedFeature,
    InternalError
};

/**
 * @brief Creates an error code object from a FITSErrorCode.
 */
std::error_code make_error_code(FITSErrorCode);

/**
 * @brief Error category for FITS errors.
 */
class FITSErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::string message(int ev) const override;
    [[nodiscard]] static const FITSErrorCategory& instance() noexcept;
};

/**
 * @class FITSFileException
 * @brief Exception class for FITS file operations.
 */
class FITSFileException : public std::system_error {
public:
    explicit FITSFileException(FITSErrorCode code,
                               const std::string& message = "")
        : std::system_error(make_error_code(code), message) {}

    explicit FITSFileException(const std::string& message)
        : std::system_error(make_error_code(FITSErrorCode::InternalError),
                            message) {}

    [[nodiscard]] FITSErrorCode errorCode() const noexcept {
        return static_cast<FITSErrorCode>(code().value());
    }
};

/**
 * @brief Callback type for progress reporting.
 */
using ProgressCallback =
    std::function<void(float progress, const std::string& status)>;

/**
 * @class FITSFile
 * @brief Class for handling FITS files.
 */
class FITSFile {
public:
    /**
     * @brief Default constructor
     */
    FITSFile() = default;

    /**
     * @brief Constructor that reads a FITS file
     * @param filename The name of the file to read
     * @throws FITSFileException if file cannot be read
     */
    explicit FITSFile(const std::string& filename);

    /**
     * @brief Move constructor
     */
    FITSFile(FITSFile&&) noexcept = default;

    /**
     * @brief Move assignment operator
     */
    FITSFile& operator=(FITSFile&&) noexcept = default;

    /**
     * @brief Deleted copy constructor (FITS files are resource-heavy)
     */
    FITSFile(const FITSFile&) = delete;

    /**
     * @brief Deleted copy assignment operator
     */
    FITSFile& operator=(const FITSFile&) = delete;

    /**
     * @brief Reads a FITS file from the specified filename.
     * @param filename The name of the file to read.
     * @throws FITSFileException if file cannot be opened or read
     */
    void readFITS(const std::string& filename);

    /**
     * @brief Reads a FITS file asynchronously.
     * @param filename The name of the file to read.
     * @return A future that can be waited on for completion.
     */
    [[nodiscard]] std::future<void> readFITSAsync(const std::string& filename);

    /**
     * @brief Writes the FITS file to the specified filename.
     * @param filename The name of the file to write.
     * @throws FITSFileException if file cannot be created or written
     */
    void writeFITS(const std::string& filename) const;

    /**
     * @brief Writes the FITS file asynchronously.
     * @param filename The name of the file to write.
     * @return A future that can be waited on for completion.
     */
    [[nodiscard]] std::future<void> writeFITSAsync(
        const std::string& filename) const;

    /**
     * @brief Gets the number of HDUs (Header Data Units) in the FITS file.
     * @return The number of HDUs.
     */
    [[nodiscard]] size_t getHDUCount() const noexcept;

    /**
     * @brief Checks if the FITS file contains any HDUs.
     * @return true if empty, false otherwise.
     */
    [[nodiscard]] bool isEmpty() const noexcept;

    /**
     * @brief Gets a constant reference to the HDU at the specified index.
     * @param index The index of the HDU to retrieve.
     * @return A constant reference to the HDU.
     * @throws std::out_of_range if index is invalid
     */
    [[nodiscard]] const HDU& getHDU(size_t index) const;

    /**
     * @brief Gets a reference to the HDU at the specified index.
     * @param index The index of the HDU to retrieve.
     * @return A reference to the HDU.
     * @throws std::out_of_range if index is invalid
     */
    HDU& getHDU(size_t index);

    /**
     * @brief Gets a specific type of HDU at the specified index.
     * @tparam T The HDU type to retrieve (must be derived from HDU).
     * @param index The index of the HDU to retrieve.
     * @return A reference to the HDU as the specified type.
     * @throws std::out_of_range if index is invalid
     * @throws std::bad_cast if HDU is not of the requested type
     */
    template <typename T>
        requires std::derived_from<T, HDU>
    [[nodiscard]] T& getHDUAs(size_t index);

    /**
     * @brief Gets a specific type of HDU at the specified index (const
     * version).
     * @tparam T The HDU type to retrieve (must be derived from HDU).
     * @param index The index of the HDU to retrieve.
     * @return A const reference to the HDU as the specified type.
     * @throws std::out_of_range if index is invalid
     * @throws std::bad_cast if HDU is not of the requested type
     */
    template <typename T>
        requires std::derived_from<T, HDU>
    [[nodiscard]] const T& getHDUAs(size_t index) const;

    /**
     * @brief Adds an HDU to the FITS file.
     * @param hdu A unique pointer to the HDU to add.
     * @throws std::invalid_argument if hdu is null
     */
    void addHDU(std::unique_ptr<HDU> hdu);

    /**
     * @brief Removes the HDU at the specified index.
     * @param index The index of the HDU to remove.
     * @throws std::out_of_range if index is invalid
     */
    void removeHDU(size_t index);

    /**
     * @brief Creates a new ImageHDU and adds it to the file.
     * @param width The width of the image.
     * @param height The height of the image.
     * @param channels The number of channels in the image (default 1).
     * @return A reference to the newly created ImageHDU.
     */
    ImageHDU& createImageHDU(int width, int height, int channels = 1);

    /**
     * @brief Sets a callback function for progress reporting.
     * @param callback The callback function to set.
     */
    void setProgressCallback(ProgressCallback callback) noexcept;

    /**
     * @brief Reads a FITS file from the specified filename with options.
     * @param filename The name of the file to read.
     * @param useMmap Whether to use memory-mapped I/O for large files.
     * @param validateData Whether to validate data after reading.
     * @throws FITSFileException if file cannot be opened or read
     */
    void readFITS(const std::string& filename, bool useMmap = false,
                  bool validateData = true);

    /**
     * @brief Reads a FITS file asynchronously with options.
     * @param filename The name of the file to read.
     * @param useMmap Whether to use memory-mapped I/O for large files.
     * @param validateData Whether to validate data after reading.
     * @return A future that can be waited on for completion.
     */
    [[nodiscard]] std::future<void> readFITSAsync(const std::string& filename,
                                                  bool useMmap = false,
                                                  bool validateData = true);

private:
    std::vector<std::unique_ptr<HDU>>
        hdus;                           ///< Vector of unique pointers to HDUs.
    ProgressCallback progressCallback;  ///< Callback for progress reporting.

    /**
     * @brief Reports progress to the registered callback, if any.
     * @param progress Progress value (0.0 to 1.0).
     * @param status Status message.
     */
    void reportProgress(float progress, const std::string& status) const;

    /**
     * @brief Reads a FITS file using memory-mapped I/O.
     * @param filename The name of the file to read.
     * @param validateData Whether to validate data after reading.
     * @throws FITSFileException if file cannot be memory-mapped or read
     */
    void readFITSWithMmap(const std::string& filename, bool validateData);
};

// Template implementations

template <typename T>
    requires std::derived_from<T, HDU>
T& FITSFile::getHDUAs(size_t index) {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }

    auto* typedHDU = dynamic_cast<T*>(hdus[index].get());
    if (!typedHDU) {
        throw std::bad_cast();
    }

    return *typedHDU;
}

template <typename T>
    requires std::derived_from<T, HDU>
const T& FITSFile::getHDUAs(size_t index) const {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }

    const auto* typedHDU = dynamic_cast<const T*>(hdus[index].get());
    if (!typedHDU) {
        throw std::bad_cast();
    }

    return *typedHDU;
}

#endif  // ATOM_IMAGE_FITS_FILE_HPP
