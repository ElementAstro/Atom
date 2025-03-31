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

// File operation exception class
class FileOperationException : public std::runtime_error {
public:
    explicit FileOperationException(const std::string& message)
        : std::runtime_error(message) {}
};

// Data format exception class
class DataFormatException : public std::runtime_error {
public:
    explicit DataFormatException(const std::string& message)
        : std::runtime_error(message) {}
};

// Concept for numeric types allowed in FITS data
template <typename T>
concept FitsNumeric = std::integral<T> || std::floating_point<T>;

// Task class for coroutines
template <typename T>
class Task {
public:
    struct promise_type {
        T result;

        Task get_return_object() {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(T value) noexcept { result = std::move(value); }
        void unhandled_exception() { std::terminate(); }
    };

    explicit Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    ~Task() {
        if (coro)
            coro.destroy();
    }

    T get_result() { return coro.promise().result; }

private:
    std::coroutine_handle<promise_type> coro;
};

class HDU {
public:
    virtual ~HDU() = default;
    virtual void readHDU(std::ifstream& file) = 0;
    virtual void writeHDU(std::ofstream& file) const = 0;

    [[nodiscard]] std::future<void> readHDUAsync(std::ifstream& file) {
        return std::async(std::launch::async,
                          [this, &file]() { this->readHDU(file); });
    }

    [[nodiscard]] std::future<void> writeHDUAsync(std::ofstream& file) const {
        return std::async(std::launch::async,
                          [this, &file]() { this->writeHDU(file); });
    }

    [[nodiscard]] const FITSHeader& getHeader() const noexcept {
        return header;
    }
    FITSHeader& getHeader() noexcept { return header; }

    void setHeaderKeyword(const std::string& keyword,
                          const std::string& value) noexcept;
    [[nodiscard]] std::string getHeaderKeyword(
        const std::string& keyword) const;

protected:
    FITSHeader header;
    std::unique_ptr<FITSData> data;
};

class ImageHDU : public HDU {
public:
    void readHDU(std::ifstream& file) override;
    void writeHDU(std::ofstream& file) const override;

    void setImageSize(int w, int h, int c = 1);
    [[nodiscard]] std::tuple<int, int, int> getImageSize() const noexcept;

    template <FitsNumeric T>
    void setPixel(int x, int y, T value, int channel = 0);

    template <FitsNumeric T>
    [[nodiscard]] T getPixel(int x, int y, int channel = 0) const;

    template <FitsNumeric T>
    struct ImageStats {
        T min;
        T max;
        double mean;
        double stddev;
    };

    template <FitsNumeric T>
    [[nodiscard]] ImageStats<T> computeImageStats(int channel = 0) const;

    template <FitsNumeric T>
    void applyFilter(std::span<const std::span<const double>> kernel,
                     int channel = -1);

    // Color image support methods
    [[nodiscard]] bool isColor() const noexcept { return channels > 1; }
    [[nodiscard]] int getChannelCount() const noexcept { return channels; }

    // Parallel image processing capability
    template <FitsNumeric T>
    void applyFilterParallel(std::span<const std::span<const double>> kernel,
                             int channel = -1);

    // Resize image with new width and height
    template <FitsNumeric T>
    void resize(int newWidth, int newHeight);

    // Create a thumbnail version of the image
    template <FitsNumeric T>
    [[nodiscard]] std::unique_ptr<ImageHDU> createThumbnail(int maxSize) const;

    // Extract a region of interest from image
    template <FitsNumeric T>
    [[nodiscard]] std::unique_ptr<ImageHDU> extractROI(int x, int y, int width,
                                                       int height) const;

    // Coroutine based processing for large images
    template <FitsNumeric T>
    [[nodiscard]] Task<ImageStats<T>> computeImageStatsAsync(
        int channel = 0) const;

    // Advanced image processing features

    // Image blending and operations
    template <FitsNumeric T>
    void blendImage(const ImageHDU& other, double alpha, int channel = -1);

    template <FitsNumeric T>
    void applyImageMask(const ImageHDU& mask, int maskChannel = 0);

    template <FitsNumeric T>
    void applyMathOperation(const std::function<T(T)>& operation,
                            int channel = -1);

    template <FitsNumeric T>
    void compositeImages(
        const std::vector<std::reference_wrapper<const ImageHDU>>& images,
        const std::vector<double>& weights);

    // Image enhancement and analysis
    template <FitsNumeric T>
    void histogramEqualization(int channel = -1);

    template <FitsNumeric T>
    std::vector<double> computeHistogram(int numBins, int channel = 0) const;

    template <FitsNumeric T>
    void autoAdjustLevels(double blackPoint = 0.0, double whitePoint = 1.0,
                          int channel = -1);

    // Edge detection and morphological operations
    template <FitsNumeric T>
    void detectEdges(const std::string& method = "sobel", int channel = -1);

    template <FitsNumeric T>
    void applyMorphologicalOperation(const std::string& operation,
                                     int kernelSize, int channel = -1);

    // Image compression and data processing
    [[nodiscard]] double computeCompressionRatio() const noexcept;

    template <FitsNumeric T>
    void compress(const std::string& algorithm = "rle", int level = 5);

    template <FitsNumeric T>
    void decompress();

    // Noise processing
    template <FitsNumeric T>
    void removeNoise(const std::string& method = "median", int kernelSize = 3,
                     int channel = -1);

    template <FitsNumeric T>
    void addNoise(const std::string& noiseType, double param, int channel = -1);

    // Wavelet transform and frequency domain operations
    template <FitsNumeric T>
    void applyFourierTransform(bool inverse = false, int channel = -1);

    template <FitsNumeric T>
    void applyFrequencyFilter(const std::string& filterType, double cutoff,
                              int channel = -1);

    // Advanced optical corrections
    template <FitsNumeric T>
    void correctVignetting(double strength, double radius, int channel = -1);

    template <FitsNumeric T>
    void correctLensDistortion(double k1, double k2, double k3,
                               int channel = -1);

    // Color management
    template <FitsNumeric T>
    void convertColorSpace(const std::string& fromSpace,
                           const std::string& toSpace);

    // Image alignment and stacking
    template <FitsNumeric T>
    void registerToReference(const ImageHDU& reference,
                             const std::string& method = "affine");

    template <FitsNumeric T>
    static std::unique_ptr<ImageHDU> stackImages(
        const std::vector<std::reference_wrapper<const ImageHDU>>& images,
        const std::string& method = "median");

    // Enhanced asynchronous processing interface
    template <FitsNumeric T>
    [[nodiscard]] std::future<void> applyFilterAsync(
        std::span<const std::span<const double>> kernel, int channel = -1);

    template <FitsNumeric T>
    [[nodiscard]] std::future<std::vector<double>> computeHistogramAsync(
        int numBins, int channel = 0) const;

private:
    int width = 0;
    int height = 0;
    int channels = 1;
    bool compressed = false;
    std::string compressionAlgorithm;

    template <FitsNumeric T>
    void initializeData();

    // Helper method to validate pixel coordinates
    [[nodiscard]] bool validateCoordinates(int x, int y,
                                           int channel) const noexcept;

    // Helper method for bilinear interpolation during resizing
    template <FitsNumeric T>
    [[nodiscard]] T bilinearInterpolate(const TypedFITSData<T>& srcData,
                                        double x, double y, int channel) const;

    // Create standard filter kernel
    [[nodiscard]] std::vector<std::vector<double>> createFilterKernel(
        const std::string& filterType, int size) const;

    // Helper methods for frequency domain processing
    template <FitsNumeric T>
    void fft2D(std::vector<std::complex<double>>& data, bool inverse, int rows,
               int cols);

    // Internal compression implementation
    template <FitsNumeric T>
    std::vector<unsigned char> compressRLE(const std::vector<T>& data) const;

    template <FitsNumeric T>
    std::vector<T> decompressRLE(
        const std::vector<unsigned char>& compressedData,
        size_t originalSize) const;
};
