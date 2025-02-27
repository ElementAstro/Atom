#pragma once

#include <concepts>
#include <coroutine>
#include <fstream>
#include <future>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
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

private:
    int width = 0;
    int height = 0;
    int channels = 1;

    template <FitsNumeric T>
    void initializeData();

    // Helper method to validate pixel coordinates
    [[nodiscard]] bool validateCoordinates(int x, int y,
                                           int channel) const noexcept;

    // Helper method for bilinear interpolation during resizing
    template <FitsNumeric T>
    [[nodiscard]] T bilinearInterpolate(const TypedFITSData<T>& srcData,
                                        double x, double y, int channel) const;
};
