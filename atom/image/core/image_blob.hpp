#ifndef ATOM_IMAGE_BLOB_HPP
#define ATOM_IMAGE_BLOB_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

// Forward declare error macros - actual definitions in source files
#ifndef THROW_RUNTIME_ERROR
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#endif
#ifndef THROW_OUT_OF_RANGE
#define THROW_OUT_OF_RANGE(msg) throw std::out_of_range(msg)
#endif

#if __has_include(<opencv2/core.hpp>)
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#if __has_include(<CImg.h>)
#include <CImg.h>
#endif

#if __has_include(<stb_image.h>)
#include <stb_image.h>
#include <stb_image_write.h>
#endif

namespace atom::image {

template <class T>
concept BlobType =
    std::same_as<T, std::byte> || std::same_as<T, const std::byte>;

template <class T>
concept BlobValueType = std::is_trivially_copyable_v<T>;

enum class BlobMode { NORMAL, FAST };

constexpr int DEFAULT_DEPTH = 8;

template <BlobType T, BlobMode Mode = BlobMode::NORMAL>
class Blob {
private:
    std::conditional_t<Mode == BlobMode::FAST, std::span<T>, std::vector<T>>
        storage_;
    int rows_ = 0;
    int cols_ = 0;
    int channels_ = 1;
#if __has_include(<opencv2/core.hpp>)
    int depth_ = CV_8U;
#else
    int depth_ = DEFAULT_DEPTH;
#endif

public:
    Blob() noexcept = default;
    Blob(const Blob&) = default;
    Blob(Blob&&) noexcept = default;
    auto operator=(const Blob&) -> Blob& = default;
    auto operator=(Blob&&) noexcept -> Blob& = default;
    ~Blob() = default;

    template <class U>
        requires(std::is_const_v<T> && std::same_as<const U, T>)
    explicit Blob(const Blob<U, Mode>& that) noexcept
        : storage_(that.storage_),
          rows_(that.rows_),
          cols_(that.cols_),
          channels_(that.channels_),
          depth_(that.depth_) {}

    template <class U>
        requires(std::is_const_v<T> && std::same_as<const U, T>)
    explicit Blob(Blob<U, Mode>&& that) noexcept
        : storage_(std::move(that.storage_)),
          rows_(that.rows_),
          cols_(that.cols_),
          channels_(that.channels_),
          depth_(that.depth_) {}

    Blob(std::nullptr_t, size_t n) {
        if (n != 0) {
            THROW_RUNTIME_ERROR(
                "Cannot create Blob from null pointer with non-zero size");
        }
        if constexpr (Mode == BlobMode::FAST) {
            storage_ = std::span<T>();
        } else {
            storage_.clear();
        }
        rows_ = 0;
        cols_ = 0;
        channels_ = 1;
#if __has_include(<opencv2/core.hpp>)
        depth_ = CV_8U;
#else
        depth_ = DEFAULT_DEPTH;
#endif
    }

    Blob(void* ptr, size_t n)
        requires(Mode == BlobMode::FAST)
    {
        if (ptr == nullptr) {
            if (n != 0) {
                THROW_RUNTIME_ERROR(
                    "Cannot create Blob from null pointer with non-zero size");
            }
            storage_ = std::span<T>();
            return;
        }
        storage_ = std::span<T>(reinterpret_cast<T*>(ptr), n);
    }

    template <BlobValueType U>
    explicit Blob(U& var) noexcept
        requires(Mode == BlobMode::FAST)
        : storage_(reinterpret_cast<T*>(&var), sizeof(U)) {}

    template <BlobValueType U>
    Blob(U* ptr, size_t n) {
        if (ptr == nullptr) {
            if (n != 0) {
                THROW_RUNTIME_ERROR(
                    "Cannot create Blob from null pointer with non-zero size");
            }
            if constexpr (Mode == BlobMode::FAST) {
                storage_ = std::span<T>();
            } else {
                storage_.clear();
            }
            rows_ = 0;
            cols_ = 0;
            channels_ = 1;
            return;
        }

        const auto byte_count = n * sizeof(U);

        if constexpr (Mode == BlobMode::FAST) {
            if constexpr (std::is_const_v<U>) {
                THROW_RUNTIME_ERROR(
                    "Cannot create fast blob from const data source");
            }
            auto* byte_ptr = reinterpret_cast<T*>(ptr);
            storage_ = std::span<T>(byte_ptr, byte_count);
        } else {
            auto* byte_ptr = reinterpret_cast<const T*>(ptr);
            storage_ = std::vector<T>(byte_ptr, byte_ptr + byte_count);
        }

        // Try to infer dimensions from byte count
        // Assume single-channel, single-row layout if not otherwise specified
        rows_ = 1;
        cols_ = static_cast<int>(byte_count);
        channels_ = 1;
#if __has_include(<opencv2/core.hpp>)
        depth_ = CV_8U;
#else
        depth_ = DEFAULT_DEPTH;
#endif
    }

    template <BlobValueType U, size_t N>
    explicit Blob(const std::array<U, N>& arr) {
        if constexpr (Mode == BlobMode::FAST) {
            storage_ = std::span<T>(reinterpret_cast<const T*>(arr.data()),
                                    N * sizeof(U));
        } else {
            storage_.resize(N * sizeof(U));
            std::memcpy(storage_.data(), arr.data(), N * sizeof(U));
        }
    }

    // Constructor from raw data with dimensions
    Blob(void* ptr, size_t size, int rows, int cols, int channels = 1,
         int depth = DEFAULT_DEPTH) {
        if (ptr == nullptr) {
            if (size != 0) {
                THROW_RUNTIME_ERROR(
                    "Cannot create Blob from null pointer with non-zero size");
            }
            if constexpr (Mode == BlobMode::FAST) {
                storage_ = std::span<T>();
            } else {
                storage_.clear();
            }
        } else if constexpr (Mode == BlobMode::FAST) {
            storage_ = std::span<T>(reinterpret_cast<T*>(ptr), size);
        } else {
            storage_.resize(size);
            std::memcpy(storage_.data(), ptr, size);
        }
        rows_ = rows;
        cols_ = cols;
        channels_ = channels;
        depth_ = depth;
    }

    template <BlobValueType U, size_t N>
    explicit Blob(std::array<U, N>& arr) {
        if constexpr (Mode == BlobMode::FAST) {
            storage_ =
                std::span<T>(reinterpret_cast<T*>(arr.data()), sizeof(U) * N);
        } else {
            storage_ = std::vector<T>(
                reinterpret_cast<T*>(arr.data()),
                reinterpret_cast<T*>(arr.data()) + sizeof(U) * N);
        }
    }

#if __has_include(<opencv2/core.hpp>)
    explicit Blob(const cv::Mat& mat)
        : rows_(mat.rows),
          cols_(mat.cols),
          channels_(mat.channels()),
          depth_(mat.depth()) {
        if (mat.isContinuous()) {
            if constexpr (Mode == BlobMode::FAST) {
                storage_ = std::span<T>(reinterpret_cast<T*>(mat.data),
                                        mat.total() * mat.elemSize());
            } else {
                storage_.assign(reinterpret_cast<T*>(mat.data),
                                reinterpret_cast<T*>(mat.data) +
                                    mat.total() * mat.elemSize());
            }
        } else {
            if constexpr (Mode == BlobMode::FAST) {
                THROW_RUNTIME_ERROR(
                    "Cannot create fast blob from non-continuous cv::Mat");
            } else {
                storage_.reserve(mat.total() * mat.elemSize());
                for (int i = 0; i < mat.rows; ++i) {
                    storage_.insert(storage_.end(), mat.ptr<T>(i),
                                    mat.ptr<T>(i) + mat.cols * mat.elemSize());
                }
            }
        }
    }
#endif

#if __has_include(<CImg.h>)
    explicit Blob(const cimg_library::CImg<unsigned char>& img)
        : rows_(img.height()),
          cols_(img.width()),
          channels_(img.spectrum()),
          depth_(DEFAULT_DEPTH) {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot create fast blob from CImg");
        } else {
            storage_.resize(img.size());
            std::memcpy(storage_.data(), img.data(), img.size());
        }
    }

    cimg_library::CImg<unsigned char> to_cimg() const {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot convert fast blob to CImg");
        } else {
            cimg_library::CImg<unsigned char> img(cols_, rows_, 1, channels_);
            std::memcpy(img.data(), storage_.data(), storage_.size());
            return img;
        }
    }

    void apply_cimg_filter(const cimg_library::CImg<float>& kernel) {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot apply CImg filter in fast mode");
        } else {
            auto img = to_cimg();
            img.convolve(kernel);
            *this = Blob(img);
        }
    }
#endif

#if __has_include(<stb_image.h>)
    explicit Blob(const std::string& filename) {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot create fast blob from stb_image");
        } else {
            int width, height, channels;
            unsigned char* data =
                stbi_load(filename.c_str(), &width, &height, &channels, 0);
            if (!data) {
                THROW_RUNTIME_ERROR("Failed to load image using stb_image");
            }
            rows_ = height;
            cols_ = width;
            channels_ = channels;
            depth_ = DEFAULT_DEPTH;
            storage_.resize(width * height * channels);
            std::memcpy(storage_.data(), data, width * height * channels);
            stbi_image_free(data);
        }
    }

    void save_as(const std::string& filename, const std::string& format) const {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot save fast blob using stb_image");
        } else {
            if (format == "png") {
                stbi_write_png(filename.c_str(), cols_, rows_, channels_,
                               storage_.data(), cols_ * channels_);
            } else if (format == "bmp") {
                stbi_write_bmp(filename.c_str(), cols_, rows_, channels_,
                               storage_.data());
            } else if (format == "tga") {
                stbi_write_tga(filename.c_str(), cols_, rows_, channels_,
                               storage_.data());
            } else if (format == "jpg") {
                stbi_write_jpg(filename.c_str(), cols_, rows_, channels_,
                               storage_.data(), 100);
            } else {
                THROW_RUNTIME_ERROR("Unsupported format for stb_image");
            }
        }
    }
#endif

    auto operator[](size_t idx) -> T& { return storage_[idx]; }
    auto operator[](size_t idx) const -> const T& { return storage_[idx]; }

    auto begin() { return storage_.begin(); }
    auto end() { return storage_.end(); }
    auto begin() const { return storage_.begin(); }
    auto end() const { return storage_.end(); }

    [[nodiscard]] auto size() const -> size_t { return storage_.size(); }

    [[nodiscard]] auto data() -> T* { return storage_.data(); }
    [[nodiscard]] auto data() const -> const T* { return storage_.data(); }

    auto slice(size_t offset, size_t length) const -> Blob {
        if (offset + length > size()) {
            THROW_OUT_OF_RANGE("Slice range out of bounds");
        }
        Blob result;
        // Preserve original geometry if possible: infer rows from current row
        // width
        result.channels_ = channels_;
        result.depth_ = depth_;
        if (cols_ > 0 && channels_ > 0) {
            const size_t row_bytes =
                static_cast<size_t>(cols_) * static_cast<size_t>(channels_);
            if (length % row_bytes == 0) {
                result.rows_ = static_cast<int>(length / row_bytes);
                result.cols_ = cols_;
            } else {
                result.rows_ = 1;
                result.cols_ =
                    static_cast<int>(length / std::max<size_t>(channels_, 1));
            }
        } else {
            result.rows_ = 1;
            result.cols_ = static_cast<int>(length);
        }
        if constexpr (Mode == BlobMode::FAST) {
            result.storage_ = std::span<T>(storage_.data() + offset, length);
        } else {
            result.storage_.assign(storage_.begin() + offset,
                                   storage_.begin() + offset + length);
        }
        return result;
    }

    auto operator==(const Blob& other) const -> bool {
        return std::ranges::equal(storage_, other.storage_) &&
               rows_ == other.rows_ && cols_ == other.cols_ &&
               channels_ == other.channels_ && depth_ == other.depth_;
    }

    void fill(T value) { std::ranges::fill(storage_, value); }

    void append(const Blob& other) {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot append in Fast mode");
        } else {
            storage_.insert(storage_.end(), other.storage_.begin(),
                            other.storage_.end());
            rows_ += other.rows_;
        }
    }

    void append(const void* ptr, size_t n) {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot append in Fast mode");
        } else {
            const auto* bytePtr = reinterpret_cast<const T*>(ptr);
            storage_.insert(storage_.end(), bytePtr, bytePtr + n);
            // Update dimensions only when they are meaningful and consistent
            size_t denom =
                static_cast<size_t>(cols_) * static_cast<size_t>(channels_);
            if (denom > 0 && n % denom == 0) {
                rows_ += static_cast<int>(n / denom);
            } else if (cols_ == 0 || channels_ == 0) {
                // Initialize a sane default to avoid division by zero later
                channels_ = channels_ == 0 ? 1 : channels_;
                cols_ = cols_ == 0 ? static_cast<int>(n) : cols_;
                // With this initialization, the current buffer represents one
                // row
                if (n == static_cast<size_t>(cols_) *
                             static_cast<size_t>(channels_)) {
                    rows_ = std::max(1, rows_);
                }
            }
        }
    }

    void allocate(size_t size) {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot allocate in Fast mode");
        } else {
            storage_.resize(size);
        }
    }

    void deallocate() {
        if constexpr (Mode == BlobMode::FAST) {
            THROW_RUNTIME_ERROR("Cannot deallocate in Fast mode");
        } else {
            storage_.clear();
            storage_.shrink_to_fit();
        }
    }

    void xorWith(const Blob& other) {
        if (size() != other.size()) {
            THROW_RUNTIME_ERROR(
                "Blobs must be of the same size for XOR operation");
        }
        std::transform(begin(), end(), other.begin(), begin(), std::bit_xor());
    }

    [[nodiscard]] auto compress() const -> Blob {
        Blob compressed;
        for (size_t i = 0; i < size(); ++i) {
            T current = storage_[i];
            size_t count = 1;
#ifdef __NVCC__
#pragma unroll
#endif
            while (i + 1 < size() && storage_[i + 1] == current) {
                ++count;
                ++i;
            }
            compressed.append(&current, sizeof(T));
            compressed.append(&count, sizeof(size_t));
        }
        return compressed;
    }

    [[nodiscard]] auto decompress() const -> Blob {
        Blob decompressed;
        for (size_t i = 0; i < size(); i += sizeof(T) + sizeof(size_t)) {
            T value;
            size_t count;
            std::memcpy(&value, &storage_[i], sizeof(T));
            std::memcpy(&count, &storage_[i + sizeof(T)], sizeof(size_t));
#ifdef __NVCC__
#pragma unroll
#endif
            for (size_t j = 0; j < count; ++j) {
                decompressed.append(&value, sizeof(T));
            }
        }
        return decompressed;
    }

    [[nodiscard]] auto serialize() const -> std::vector<std::byte> {
        std::vector<std::byte> serialized;
        size_t size = storage_.size();
        serialized.insert(
            serialized.end(), reinterpret_cast<const std::byte*>(&size),
            reinterpret_cast<const std::byte*>(&size) + sizeof(size_t));
        serialized.insert(serialized.end(),
                          reinterpret_cast<const std::byte*>(storage_.data()),
                          reinterpret_cast<const std::byte*>(storage_.data()) +
                              storage_.size());
        return serialized;
    }

    static auto deserialize(const std::vector<std::byte>& data) -> Blob {
        if (data.size() < sizeof(size_t)) {
            THROW_RUNTIME_ERROR("Invalid serialized data");
        }
        size_t size;
        std::memcpy(&size, data.data(), sizeof(size_t));
        if (data.size() != sizeof(size_t) + size) {
            THROW_RUNTIME_ERROR("Invalid serialized data size");
        }
        // Create a copy to avoid const issues
        Blob result;
        // Use memcpy to copy the data instead of reinterpret_cast
        result.storage_.resize(size);
        std::memcpy(result.storage_.data(), data.data() + sizeof(size_t), size);
        return result;
    }

#if __has_include(<opencv2/core.hpp>)
    cv::Mat to_mat() const {
        int type = CV_MAKETYPE(depth_, channels_);
        cv::Mat mat(rows_, cols_, type);
        std::memcpy(mat.data, storage_.data(), size());
        return mat;
    }

    void apply_filter(const cv::Mat& kernel) {
        cv::Mat src = to_mat();
        cv::Mat dst;
        cv::filter2D(src, dst, -1, kernel);
        *this = Blob(dst);
    }

    void resize(int new_rows, int new_cols) {
        cv::Mat src = to_mat();
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(new_cols, new_rows));
        *this = Blob(dst);
    }

    void convert_color(int code) {
        cv::Mat src = to_mat();
        cv::Mat dst;
        cv::cvtColor(src, dst, code);
        *this = Blob(dst);
    }

    void rotate(double angle) {
        cv::Mat src = to_mat();
        cv::Mat dst;
        cv::Point2f center(src.cols / 2.0, src.rows / 2.0);
        cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
        cv::warpAffine(src, dst, rot, src.size());
        *this = Blob(dst);
    }

    void flip(int flipCode) {
        cv::Mat src = to_mat();
        cv::Mat dst;
        cv::flip(src, dst, flipCode);
        *this = Blob(dst);
    }

    void save(const std::string& filename) const {
        cv::Mat mat = to_mat();
        cv::imwrite(filename, mat);
    }

    static Blob load(const std::string& filename) {
        cv::Mat mat = cv::imread(filename, cv::IMREAD_UNCHANGED);
        if (mat.empty()) {
            THROW_RUNTIME_ERROR("Failed to load image from file");
        }
        return Blob(mat);
    }

    std::vector<Blob> split_channels() const {
        cv::Mat src = to_mat();
        std::vector<cv::Mat> channels;
        cv::split(src, channels);
        std::vector<Blob> channel_blobs;
        for (const auto& channel : channels) {
            channel_blobs.emplace_back(channel);
        }
        return channel_blobs;
    }

    static Blob merge_channels(const std::vector<Blob>& channel_blobs) {
        std::vector<cv::Mat> channels;
        for (const auto& blob : channel_blobs) {
            channels.push_back(blob.to_mat());
        }
        cv::Mat merged;
        cv::merge(channels, merged);
        return Blob(merged);
    }
#endif

    [[nodiscard]] auto getRows() const -> int { return rows_; }
    [[nodiscard]] auto getCols() const -> int { return cols_; }
    [[nodiscard]] auto getChannels() const -> int { return channels_; }
    [[nodiscard]] auto getDepth() const -> int { return depth_; }

    // Compatibility accessors for legacy examples
    [[nodiscard]] auto rows() const -> int { return rows_; }
    [[nodiscard]] auto cols() const -> int { return cols_; }
    [[nodiscard]] auto channels() const -> int { return channels_; }

    // Convenience pixel accessor for 8-bit data (for examples)
    auto at(int y, int x, int channel = 0) -> uint8_t& {
        const size_t bytesPerChannel = static_cast<size_t>(depth_ == 8    ? 1
                                                           : depth_ == 16 ? 2
                                                                          : 4);
        const size_t idx =
            (static_cast<size_t>(y) * static_cast<size_t>(cols_) +
             static_cast<size_t>(x)) *
                (static_cast<size_t>(channels_) * bytesPerChannel) +
            static_cast<size_t>(channel) * bytesPerChannel;
        return *reinterpret_cast<uint8_t*>(&storage_[idx]);
    }
    [[nodiscard]] auto at(int y, int x, int channel = 0) const -> uint8_t {
        const size_t bytesPerChannel = static_cast<size_t>(depth_ == 8    ? 1
                                                           : depth_ == 16 ? 2
                                                                          : 4);
        const size_t idx =
            (static_cast<size_t>(y) * static_cast<size_t>(cols_) +
             static_cast<size_t>(x)) *
                (static_cast<size_t>(channels_) * bytesPerChannel) +
            static_cast<size_t>(channel) * bytesPerChannel;
        return *reinterpret_cast<const uint8_t*>(&storage_[idx]);
    }

    // Additional utility methods
    [[nodiscard]] auto getWidth() const -> int { return cols_; }
    [[nodiscard]] auto getHeight() const -> int { return rows_; }
    [[nodiscard]] auto isEmpty() const -> bool { return storage_.empty(); }
    [[nodiscard]] auto getPixelSize() const -> size_t {
        return channels_ * (depth_ == 8 ? 1 : depth_ == 16 ? 2 : 4);
    }
    [[nodiscard]] auto getImageSize() const -> size_t {
        return static_cast<size_t>(rows_) * static_cast<size_t>(cols_) *
               getPixelSize();
    }

    // Memory alignment for performance
    void alignMemory(size_t alignment = 64) {
        if constexpr (Mode == BlobMode::NORMAL) {
            if (storage_.size() % alignment != 0) {
                size_t newSize =
                    ((storage_.size() + alignment - 1) / alignment) * alignment;
                storage_.resize(newSize);
            }
        }
    }

    // Clone method for explicit copying
    [[nodiscard]] auto clone() const -> Blob {
        Blob result;
        result.rows_ = rows_;
        result.cols_ = cols_;
        result.channels_ = channels_;
        result.depth_ = depth_;
        if constexpr (Mode == BlobMode::NORMAL) {
            result.storage_ = storage_;
        } else {
            // For fast mode, create a normal blob copy
            result.storage_.assign(storage_.begin(), storage_.end());
        }
        return result;
    }

    // Crop method
    [[nodiscard]] auto crop(int x, int y, int width, int height) const -> Blob {
        if (x < 0 || y < 0 || x + width > cols_ || y + height > rows_) {
            THROW_OUT_OF_RANGE("Crop region out of bounds");
        }

        Blob result;
        result.rows_ = height;
        result.cols_ = width;
        result.channels_ = channels_;
        result.depth_ = depth_;

        if constexpr (Mode == BlobMode::NORMAL) {
            result.storage_.reserve(static_cast<size_t>(width) *
                                    static_cast<size_t>(height) *
                                    static_cast<size_t>(channels_));

            size_t pixelSize = getPixelSize();
            for (int row = y; row < y + height; ++row) {
                size_t srcOffset =
                    (static_cast<size_t>(row) * static_cast<size_t>(cols_) +
                     static_cast<size_t>(x)) *
                    pixelSize;
                size_t copySize = static_cast<size_t>(width) * pixelSize;

                result.storage_.insert(result.storage_.end(),
                                       storage_.begin() + srcOffset,
                                       storage_.begin() + srcOffset + copySize);
            }
        }

        return result;
    }
};

using cblob = Blob<const std::byte>;
using blob = Blob<std::byte>;

using fast_cblob = Blob<const std::byte, BlobMode::FAST>;
using fast_blob = Blob<std::byte, BlobMode::FAST>;

}  // namespace atom::image

#endif  // ATOM_IMAGE_BLOB_HPP
