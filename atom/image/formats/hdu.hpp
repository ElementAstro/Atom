#pragma once

#include <complex>
#include <concepts>
#include <coroutine>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "fits_data.hpp"
#include "fits_header.hpp"

/**
 * @class FileOperationException
 * @brief Exception class for errors related to file operations.
 *
 * This exception is thrown when file operations like opening, reading, or
 * writing fail.
 */
class FileOperationException : public std::runtime_error {
public:
    /**
     * @brief Constructs a FileOperationException with a specific message.
     * @param message The error message describing the file operation failure.
     */
    explicit FileOperationException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @class DataFormatException
 * @brief Exception class for errors related to data format issues.
 *
 * This exception is thrown when data does not conform to the expected format,
 * such as during parsing or validation.
 */
class DataFormatException : public std::runtime_error {
public:
    /**
     * @brief Constructs a DataFormatException with a specific message.
     * @param message The error message describing the data format issue.
     */
    explicit DataFormatException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @concept FitsNumeric
 * @brief Concept defining types allowed as numeric data in FITS files.
 *
 * This concept restricts template parameters to standard integral or
 * floating-point types commonly used in FITS data units.
 * @tparam T The type to check.
 */
template <typename T>
concept FitsNumeric = std::integral<T> || std::floating_point<T>;

/**
 * @class HDUException
 * @brief Exception class for errors specific to HDU (Header Data Unit)
 * operations.
 *
 * This exception is thrown for errors occurring during the processing or
 * manipulation of FITS HDUs, such as invalid HDU structure or header issues.
 */
class HDUException : public std::runtime_error {
public:
    /**
     * @brief Constructs an HDUException with a specific message.
     * @param message The error message describing the HDU-related issue.
     */
    explicit HDUException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @class ImageProcessingException
 * @brief Exception class for errors during image processing operations.
 *
 * This exception is thrown when an error occurs within an image processing
 * algorithm, such as invalid parameters or unsupported operations.
 */
class ImageProcessingException : public std::runtime_error {
public:
    /**
     * @brief Constructs an ImageProcessingException with a specific message.
     * @param message The error message describing the image processing failure.
     */
    explicit ImageProcessingException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @enum FilterType
 * @brief Enumerates common image filter types.
 */
enum class FilterType {
    GAUSSIAN,   ///< Gaussian blur filter.
    MEAN,       ///< Mean (box) blur filter.
    MEDIAN,     ///< Median filter (good for salt-and-pepper noise).
    SOBEL,      ///< Sobel edge detection filter.
    LAPLACIAN,  ///< Laplacian filter (edge detection/sharpening).
    HIGHPASS,   ///< High-pass filter (sharpening).
    LOWPASS,    ///< Low-pass filter (blurring).
    CUSTOM      ///< Placeholder for custom convolution kernels.
};

/**
 * @enum NoiseType
 * @brief Enumerates types of noise that can be added to an image.
 */
enum class NoiseType {
    GAUSSIAN,     ///< Gaussian (normal distribution) noise.
    SALT_PEPPER,  ///< Salt-and-pepper noise (random black and white pixels).
    POISSON,      ///< Poisson noise (shot noise, common in photon counting).
    UNIFORM,      ///< Uniform distribution noise.
    SPECKLE       ///< Speckle noise (multiplicative noise).
};

/**
 * @enum MorphologicalOperation
 * @brief Enumerates morphological image processing operations.
 */
enum class MorphologicalOperation {
    DILATE,  ///< Dilation operation (expands bright regions).
    ERODE,   ///< Erosion operation (shrinks bright regions).
    OPEN,    ///< Opening (erosion followed by dilation, removes small bright
             ///< spots).
    CLOSE,  ///< Closing (dilation followed by erosion, fills small dark holes).
    TOPHAT,   ///< Top-hat transform (difference between image and its opening).
    BLACKHAT  ///< Black-hat transform (difference between closing and image).
};

/**
 * @enum ColorSpace
 * @brief Enumerates common color spaces for image representation.
 */
enum class ColorSpace {
    RGB,   ///< Red, Green, Blue color space.
    HSV,   ///< Hue, Saturation, Value color space.
    YUV,   ///< Luminance (Y) and Chrominance (UV) color space.
    LAB,   ///< CIELAB color space (perceptually uniform).
    GRAY,  ///< Grayscale color space.
    CMYK   ///< Cyan, Magenta, Yellow, Key (black) color space.
};

/**
 * @enum StackingMethod
 * @brief Enumerates methods for combining (stacking) multiple images.
 */
enum class StackingMethod {
    MEAN,           ///< Average pixel values across images.
    MEDIAN,         ///< Median pixel values across images (robust to outliers).
    MAX,            ///< Maximum pixel values across images.
    MIN,            ///< Minimum pixel values across images.
    SUM,            ///< Sum of pixel values across images.
    SIGMA_CLIPPING  ///< Mean stacking after rejecting outlier pixels based on
                    ///< standard deviation.
};

/**
 * @enum CompressionAlgorithm
 * @brief Enumerates supported compression algorithms for FITS data.
 */
enum class CompressionAlgorithm {
    RLE,      ///< Run-Length Encoding.
    HUFFMAN,  ///< Huffman coding (lossless).
    LZW,      ///< Lempel-Ziv-Welch (lossless).
    ZLIB,     ///< Zlib compression (used by gzip, lossless).
    NONE      ///< No compression.
};

/**
 * @class Task
 * @brief A simple coroutine task type for asynchronous operations.
 * @tparam T The type of the result produced by the coroutine.
 */
template <typename T>
class Task {
public:
    /**
     * @struct promise_type
     * @brief The promise type associated with the Task coroutine.
     */
    struct promise_type {
        T result;  ///< Stores the result of the coroutine.

        /**
         * @brief Creates the Task object associated with this promise.
         * @return The Task object.
         */
        Task get_return_object() {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        /**
         * @brief Controls the initial suspension of the coroutine. Never
         * suspends initially.
         * @return std::suspend_never
         */
        std::suspend_never initial_suspend() noexcept { return {}; }
        /**
         * @brief Controls the final suspension of the coroutine. Always
         * suspends finally.
         * @return std::suspend_always
         */
        std::suspend_always final_suspend() noexcept { return {}; }
        /**
         * @brief Called when the coroutine returns a value using co_return.
         * @param value The value returned by the coroutine.
         */
        void return_value(T value) noexcept { result = std::move(value); }
        /**
         * @brief Called when an exception propagates out of the coroutine.
         * Terminates the program.
         */
        void unhandled_exception() { std::terminate(); }
    };

    /**
     * @brief Constructs a Task from a coroutine handle.
     * @param h The coroutine handle.
     */
    explicit Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    /**
     * @brief Destroys the Task and the associated coroutine frame.
     */
    ~Task() {
        if (coro)
            coro.destroy();
    }

    // Task is move-only
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : coro(std::exchange(other.coro, nullptr)) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (coro)
                coro.destroy();
            coro = std::exchange(other.coro, nullptr);
        }
        return *this;
    }

    /**
     * @brief Retrieves the result stored in the promise.
     * @note This should only be called after the coroutine has completed.
     * @return The result of the coroutine.
     */
    T get_result() { return coro.promise().result; }

private:
    std::coroutine_handle<promise_type>
        coro;  ///< The handle to the coroutine frame.
};

/**
 * @class HDU
 * @brief Abstract base class for a FITS Header Data Unit (HDU).
 *
 * Represents a single HDU within a FITS file, containing a header and
 * optionally data. This class provides the basic interface for reading,
 * writing, and accessing header information.
 */
class HDU {
public:
    /**
     * @brief Virtual destructor for HDU.
     */
    virtual ~HDU() = default;

    /**
     * @brief Pure virtual function to read the HDU content from a file stream.
     * @param file The input file stream positioned at the beginning of the HDU.
     * @param progressCallback Optional callback for reporting progress
     * @throws HDUException, FileOperationException, FITSHeaderException On read
     * errors.
     */
    virtual void readHDU(std::ifstream& file,
                         std::function<void(float, const std::string&)>
                             progressCallback = nullptr) = 0;

    /**
     * @brief Pure virtual function to write the HDU content to a file stream.
     * @param file The output file stream.
     * @throws HDUException, FileOperationException, FITSHeaderException On
     * write errors.
     */
    virtual void writeHDU(std::ofstream& file) const = 0;

    /**
     * @brief Reads the HDU content asynchronously.
     * @param file The input file stream.
     * @return A std::future<void> that completes when the read operation
     * finishes.
     */
    [[nodiscard]] std::future<void> readHDUAsync(std::ifstream& file) {
        // Use a lambda to correctly call the member function with its default
        // argument
        return std::async(std::launch::async, [this, &file]() {
            this->readHDU(file,
                          nullptr);  // Pass nullptr for the default callback
        });
    }

    /**
     * @brief Writes the HDU content asynchronously.
     * @param file The output file stream.
     * @return A std::future<void> that completes when the write operation
     * finishes.
     */
    [[nodiscard]] std::future<void> writeHDUAsync(std::ofstream& file) const {
        // Need a mutable lambda capture because writeHDU is const but async
        // might need non-const context Or make a helper non-const function if
        // needed, but async itself is fine with const this
        return std::async(std::launch::async,
                          [this, &file]() { this->writeHDU(file); });
    }

    /**
     * @brief Gets a constant reference to the FITS header of this HDU.
     * @return const FITSHeader& The HDU's header.
     */
    [[nodiscard]] const FITSHeader& getHeader() const noexcept {
        return header;
    }
    /**
     * @brief Gets a mutable reference to the FITS header of this HDU.
     * @return FITSHeader& The HDU's header.
     */
    FITSHeader& getHeader() noexcept { return header; }

    /**
     * @brief Sets or updates a keyword in the FITS header.
     * @param keyword The name of the keyword (max 8 characters).
     * @param value The value to assign to the keyword.
     */
    void setHeaderKeyword(const std::string& keyword,
                          const std::string& value) noexcept;
    /**
     * @brief Gets the value of a specific keyword from the FITS header.
     * @param keyword The name of the keyword to retrieve.
     * @return std::string The value associated with the keyword.
     * @throws FITSHeaderException If the keyword is not found.
     */
    [[nodiscard]] std::string getHeaderKeyword(
        const std::string& keyword) const;

    /**
     * @brief Validate the HDU data
     * @return True if the data is valid, false otherwise
     */
    virtual bool isDataValid() const = 0;

protected:
    FITSHeader header;  ///< The FITS header associated with this HDU.
    std::unique_ptr<FITSData>
        data;  ///< Pointer to the FITS data block (can be null).
};

/**
 * @class ImageHDU
 * @brief Represents an Image HDU in a FITS file.
 *
 * This class specializes the HDU for image data, providing methods for
 * accessing and manipulating pixel data, image dimensions, and performing
 * various image processing tasks.
 */
class ImageHDU : public HDU {
public:
    /**
     * @brief Reads the Image HDU content (header and data) from a file stream.
     * @param file The input file stream positioned at the beginning of the
     * Image HDU.
     * @param progressCallback Optional callback for reporting progress
     * @throws HDUException, FileOperationException, FITSHeaderException,
     * FITSDataException On read errors.
     */
    void readHDU(std::ifstream& file,
                 std::function<void(float, const std::string&)>
                     progressCallback = nullptr) override;
    /**
     * @brief Writes the Image HDU content (header and data) to a file stream.
     * @param file The output file stream.
     * @throws HDUException, FileOperationException, FITSHeaderException,
     * FITSDataException On write errors.
     */
    void writeHDU(std::ofstream& file) const override;

    /**
     * @brief Sets the dimensions of the image data.
     * This also updates the relevant NAXIS keywords in the header.
     * @param w Width of the image.
     * @param h Height of the image.
     * @param c Number of channels (default is 1 for grayscale).
     * @throws std::invalid_argument If dimensions are non-positive.
     */
    void setImageSize(int w, int h, int c = 1);
    /**
     * @brief Gets the dimensions of the image.
     * @return std::tuple<int, int, int> Containing width, height, and number of
     * channels.
     */
    [[nodiscard]] std::tuple<int, int, int> getImageSize() const noexcept;

    /**
     * @brief Sets the value of a specific pixel.
     * @tparam T The numeric type of the pixel data (must satisfy FitsNumeric).
     * @param x The x-coordinate (column) of the pixel.
     * @param y The y-coordinate (row) of the pixel.
     * @param value The new value for the pixel.
     * @param channel The channel index (default is 0).
     * @throws std::out_of_range If coordinates or channel are invalid.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    void setPixel(int x, int y, T value, int channel = 0);  // Declaration only

    /**
     * @brief Gets the value of a specific pixel.
     * @tparam T The numeric type expected for the pixel data (must satisfy
     * FitsNumeric).
     * @param x The x-coordinate (column) of the pixel.
     * @param y The y-coordinate (row) of the pixel.
     * @param channel The channel index (default is 0).
     * @return T The value of the pixel.
     * @throws std::out_of_range If coordinates or channel are invalid.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    [[nodiscard]] T getPixel(int x, int y,
                             int channel = 0) const;  // Declaration only

    /**
     * @struct ImageStats
     * @brief Structure to hold basic statistics of an image channel.
     * @tparam T The numeric type of the image data.
     */
    template <FitsNumeric T>
    struct ImageStats {
        T min;          ///< Minimum pixel value.
        T max;          ///< Maximum pixel value.
        double mean;    ///< Mean pixel value.
        double stddev;  ///< Standard deviation of pixel values.
    };

    /**
     * @brief Computes basic statistics (min, max, mean, stddev) for a specific
     * image channel.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param channel The channel index for which to compute statistics (default
     * is 0).
     * @return ImageStats<T> A structure containing the computed statistics.
     * @throws std::out_of_range If the channel index is invalid.
     * @throws HDUException If data is not allocated, type mismatch occurs, or
     * data is empty.
     */
    template <FitsNumeric T>
    [[nodiscard]] ImageStats<T> computeImageStats(int channel = 0) const;

    /**
     * @brief Applies a convolution filter using the provided kernel to the
     * image data.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param kernel A span of spans representing the 2D convolution kernel.
     * @param channel The channel index to apply the filter to (-1 for all
     * channels, default).
     * @throws ImageProcessingException On filtering errors (e.g., invalid
     * kernel).
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void applyFilter(std::span<const std::span<const double>> kernel,
                     int channel = -1);

    /**
     * @brief Checks if the image has more than one channel.
     * @return true If the image has multiple channels (e.g., RGB), false
     * otherwise.
     */
    [[nodiscard]] bool isColor() const noexcept { return channels > 1; }
    /**
     * @brief Gets the number of channels in the image.
     * @return int The number of channels (e.g., 1 for grayscale, 3 for RGB).
     */
    [[nodiscard]] int getChannelCount() const noexcept { return channels; }

    /**
     * @brief Applies a convolution filter in parallel using the provided
     * kernel. Utilizes multi-threading for potentially faster processing on
     * large images.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param kernel A span of spans representing the 2D convolution kernel.
     * @param channel The channel index to apply the filter to (-1 for all
     * channels, default).
     * @throws ImageProcessingException On filtering errors.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void applyFilterParallel(std::span<const std::span<const double>> kernel,
                             int channel = -1);

    /**
     * @brief Resizes the image to the specified dimensions using bilinear
     * interpolation.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param newWidth The target width.
     * @param newHeight The target height.
     * @throws std::invalid_argument If new dimensions are non-positive.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    void resize(int newWidth, int newHeight);

    /**
     * @brief Creates a smaller thumbnail version of the image.
     * The image is resized so that its largest dimension fits within `maxSize`,
     * preserving aspect ratio.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param maxSize The maximum size (width or height) of the thumbnail.
     * @return std::unique_ptr<ImageHDU> A new ImageHDU containing the
     * thumbnail.
     * @throws std::invalid_argument If maxSize is non-positive.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    [[nodiscard]] std::unique_ptr<ImageHDU> createThumbnail(int maxSize) const;

    /**
     * @brief Extracts a rectangular Region of Interest (ROI) from the image.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param x The starting x-coordinate (column) of the ROI.
     * @param y The starting y-coordinate (row) of the ROI.
     * @param width The width of the ROI.
     * @param height The height of the ROI.
     * @return std::unique_ptr<ImageHDU> A new ImageHDU containing the extracted
     * ROI.
     * @throws std::out_of_range If the ROI coordinates are outside the image
     * bounds.
     * @throws std::invalid_argument If ROI dimensions are non-positive.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    [[nodiscard]] std::unique_ptr<ImageHDU> extractROI(int x, int y,
                                                       int roiWidth,
                                                       int roiHeight) const;

    /**
     * @brief Computes image statistics asynchronously using coroutines.
     * Suitable for large images where computation might block.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param channel The channel index (default is 0).
     * @return Task<ImageStats<T>> A coroutine task yielding the statistics.
     * @throws std::out_of_range If the channel index is invalid.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    [[nodiscard]] Task<ImageStats<T>> computeImageStatsAsync(
        int channel = 0) const;

    // --- Advanced image processing features ---

    /**
     * @brief Blends this image with another image using alpha blending.
     * Formula: result = this * (1 - alpha) + other * alpha
     * Images must have the same dimensions and data type.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param other The other ImageHDU to blend with.
     * @param alpha The blending factor (0.0 to 1.0). 0.0 means only this
     * image, 1.0 means only the other image.
     * @param channel The channel index to blend (-1 for all channels, default).
     * @throws ImageProcessingException If images have incompatible dimensions
     * or types.
     * @throws std::invalid_argument If alpha is outside the [0, 1] range.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    void blendImage(const ImageHDU& other, double alpha, int channel = -1);

    /**
     * @brief Applies a mask to the image. Pixels corresponding to zero in the
     * mask are typically set to zero or another value. The mask image must have
     * compatible dimensions.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param mask The ImageHDU containing the mask data.
     * @param maskChannel The channel of the mask image to use (default is 0).
     * @throws ImageProcessingException If images have incompatible dimensions.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    void applyImageMask(const ImageHDU& mask, int maskChannel = 0);

    /**
     * @brief Applies a mathematical operation to each pixel in the specified
     * channel(s).
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param operation A function object taking a pixel value (T) and returning
     * the transformed value (T).
     * @param channel The channel index to apply the operation to (-1 for all
     * channels, default).
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void applyMathOperation(const std::function<T(T)>& operation,
                            int channel = -1);

    /**
     * @brief Creates a composite image by combining multiple images with
     * specified weights. All input images must have the same dimensions and
     * data type. The current image data is overwritten.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param images A vector of references to the source ImageHDUs.
     * @param weights A vector of weights corresponding to each image. Weights
     * should typically sum to 1.0.
     * @throws ImageProcessingException If images have incompatible
     * dimensions/types or if vector sizes mismatch.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     */
    template <FitsNumeric T>
    void compositeImages(
        const std::vector<std::reference_wrapper<const ImageHDU>>& images,
        const std::vector<double>& weights);

    /**
     * @brief Performs histogram equalization on the specified channel(s) to
     * enhance contrast.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param channel The channel index to equalize (-1 for all channels,
     * default).
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void equalizeHistogram(int channel = -1);

    /**
     * @brief Computes the histogram for a specific image channel.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param numBins The number of bins for the histogram.
     * @param channel The channel index (default is 0).
     * @return std::vector<double> A vector representing the histogram counts or
     * frequencies for each bin.
     * @throws std::invalid_argument If numBins is non-positive.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    [[nodiscard]] std::vector<double> computeHistogram(int numBins,
                                                       int channel = 0) const;

    /**
     * @brief Automatically adjusts image levels by stretching the histogram.
     * Pixels below `blackPoint` percentile are mapped to min value, pixels
     * above `whitePoint` percentile to max value.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param blackPoint The black point percentile (0.0 to 1.0, default 0.0).
     * @param whitePoint The white point percentile (0.0 to 1.0, default 1.0).
     * @param channel The channel index to adjust (-1 for all channels,
     * default).
     * @throws std::invalid_argument If percentiles are invalid or blackPoint >=
     * whitePoint.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void autoLevels(double blackPoint = 0.0, double whitePoint = 1.0,
                    int channel = -1);

    /**
     * @brief Detects edges in the image using a specified method.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param method The edge detection method (e.g., "sobel", "laplacian").
     * @param channel The channel index to process (-1 for all channels,
     * default).
     * @throws std::invalid_argument If the method is unsupported.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void detectEdges(const std::string& method, int channel = -1);

    /**
     * @brief Applies a morphological operation (e.g., dilation, erosion) to the
     * image.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param operation The name of the operation (e.g., "dilate", "erode",
     * "open", "close").
     * @param kernelSize The size of the structuring element (must be odd).
     * @param channel The channel index to process (-1 for all channels,
     * default).
     * @throws std::invalid_argument If the operation is unsupported or
     * kernelSize is invalid.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void applyMorphology(const std::string& operation, int kernelSize,
                         int channel = -1);

    /**
     * @brief Computes the compression ratio if the data is compressed.
     * Ratio = Original Size / Compressed Size.
     * @return double The compression ratio, or 1.0 if not compressed or data is
     * empty.
     */
    [[nodiscard]] double computeCompressionRatio() const noexcept;

    /**
     * @brief Compresses the image data using the specified algorithm.
     * The internal data representation changes to compressed format.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param algorithm The compression algorithm name (e.g., "rle", "zlib").
     * @param level The compression level (algorithm-specific, e.g., 1-9 for
     * zlib).
     * @throws std::invalid_argument If the algorithm is unsupported.
     * @throws HDUException If data is not allocated, type mismatch occurs, or
     * compression fails.
     */
    template <FitsNumeric T>
    void compressData(const std::string& algorithm, int level = 0);

    /**
     * @brief Decompresses the image data if it is currently compressed.
     * The internal data representation changes back to uncompressed format.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @throws HDUException If data is not compressed, type mismatch occurs, or
     * decompression fails.
     */
    template <FitsNumeric T>
    void decompressData();

    /**
     * @brief Removes noise from the image using a specified method.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param method The noise removal method (e.g., "median", "gaussian").
     * @param kernelSize The size of the filter kernel used for noise removal.
     * @param channel The channel index to process (-1 for all channels,
     * default).
     * @throws std::invalid_argument If the method is unsupported or kernelSize
     * is invalid.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void removeNoise(const std::string& method, int kernelSize,
                     int channel = -1);

    /**
     * @brief Adds synthetic noise to the image.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param noiseType The type of noise to add (e.g., "gaussian",
     * "salt_pepper").
     * @param param A parameter controlling the noise intensity (e.g., standard
     * deviation for Gaussian, density for salt-and-pepper).
     * @param channel The channel index to add noise to (-1 for all channels,
     * default).
     * @throws std::invalid_argument If the noise type is unsupported.
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void addNoise(const std::string& noiseType, double param, int channel = -1);

    /**
     * @brief Applies a 2D Fast Fourier Transform (FFT) or its inverse to the
     * image. Transforms the image data to/from the frequency domain. Data is
     * converted to complex numbers internally.
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param inverse If true, performs the inverse FFT; otherwise, performs the
     * forward FFT.
     * @param channel The channel index to transform (-1 for all channels,
     * default).
     * @throws HDUException If data is not allocated or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void applyFourierTransform(bool inverse, int channel = -1);

    /**
     * @brief Applies a filter in the frequency domain.
     * Requires the image to be in the frequency domain (after
     * applyFourierTransform).
     * @tparam T The numeric type of the image data (must satisfy FitsNumeric).
     * @param filterType The type of frequency filter (e.g., "lowpass",
     * "highpass", "bandpass").
     * @param cutoff The cutoff frequency or parameters for the filter.
     * @param channel The channel index to filter (-1 for all channels,
     * default).
     * @throws std::invalid_argument If the filter type is unsupported.
     * @throws HDUException If data is not allocated, not in frequency domain,
     * or type mismatch occurs.
     * @throws std::out_of_range If the channel index is invalid.
     */
    template <FitsNumeric T>
    void applyFrequencyFilter(const std::string& filterType, double cutoff,
                              int channel = -1);

    /**
     * @brief Validate the HDU data
     * @return True if the data is valid, false otherwise
     */
    bool isDataValid() const override;

private:
    int width = 0;
    int height = 0;
    int channels = 1;
    bool compressed = false;
    std::string compressionAlgorithm;

    [[nodiscard]] bool validateCoordinates(int x, int y,
                                           int channel) const noexcept;

    // Helper to initialize data based on BITPIX
    template <FitsNumeric T>
    void initializeData();

    // Helper for bilinear interpolation used in resize/thumbnail
    template <FitsNumeric T>
    [[nodiscard]] T bilinearInterpolate(const TypedFITSData<T>& srcData,
                                        double x, double y, int channel) const;

    // Helper for 2D FFT (implementation might be complex)
    template <FitsNumeric T>
    void fft2D(std::vector<std::complex<double>>& data, bool inverse, int rows,
               int cols);

    // Helper for RLE compression (example)
    template <FitsNumeric T>
    [[nodiscard]] std::vector<unsigned char> compressRLE(
        const std::vector<T>& data) const;

    // Helper for RLE decompression (example)
    template <FitsNumeric T>
    [[nodiscard]] std::vector<T> decompressRLE(
        const std::vector<unsigned char>& compressedData,
        size_t originalSize) const;

    [[nodiscard]] std::vector<std::vector<double>> createFilterKernel(
        const std::string& filterType, int size) const;
};

// --- Template Implementations ---
// Definitions for templated member functions should be in the header file
// if they are not explicitly instantiated for all required types in a .cpp
// file. Since explicit instantiations are used in hdu.cpp, we keep the
// definitions in hdu.cpp and only have declarations here.

// Note: The definitions for other template member functions like
// computeImageStats, applyFilter, resize, etc., should remain in hdu.cpp
// because they are explicitly instantiated there. Only setPixel and getPixel
// declarations needed fixing here.
