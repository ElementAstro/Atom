/**
 * @file fits_utils.hpp
 * @brief Provides a simplified API for FITS image processing.
 *
 * This file offers a set of easy-to-use functions and classes that hide the
 * complexity of underlying FITS processing, allowing users to focus on image
 * manipulation without needing deep knowledge of the FITS file structure.
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_FITS_UTILS_HPP
#define ATOM_IMAGE_FITS_UTILS_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fits_file.hpp"
#include "hdu.hpp"

#ifdef ATOM_ENABLE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace atom {
namespace image {

/**
 * @class FitsImage
 * @brief Simplified FITS image processing class.
 *
 * This class wraps the underlying FITS processing, providing simple interfaces
 * for image manipulation operations.
 */
class FitsImage {
public:
    /**
     * @brief Default constructor.
     */
    FitsImage();

    /**
     * @brief Load image from file.
     * @param filename File path.
     */
    explicit FitsImage(const std::string& filename);

    /**
     * @brief Create an image of specified size.
     * @param width Width.
     * @param height Height.
     * @param channels Number of channels (default is 1).
     * @param dataType Data type (default is 16-bit integer).
     */
    FitsImage(int width, int height, int channels = 1,
              DataType dataType = DataType::SHORT);

#ifdef ATOM_ENABLE_OPENCV
    /**
     * @brief Create FITS image from OpenCV Mat object.
     * @param mat OpenCV Mat object.
     * @param dataType Conversion type (defaults to automatic selection based on
     * Mat type).
     */
    explicit FitsImage(const cv::Mat& mat, DataType dataType = DataType::SHORT);

    /**
     * @brief Convert to OpenCV Mat object.
     * @return OpenCV Mat object.
     */
    [[nodiscard]] cv::Mat toMat() const;

    /**
     * @brief Apply OpenCV filter.
     * @param filter OpenCV filter function.
     * @param channel Channel (-1 for all channels).
     */
    void applyOpenCVFilter(const std::function<cv::Mat(const cv::Mat&)>& filter,
                           int channel = -1);

    /**
     * @brief Execute image processing algorithm.
     * @param functionName Algorithm name.
     * @param params Algorithm parameters.
     */
    void processWithOpenCV(const std::string& functionName,
                           const std::map<std::string, double>& params = {});
#endif

    /**
     * @brief Get image dimensions.
     * @return Width, height, number of channels.
     */
    std::tuple<int, int, int> getSize() const;

    /**
     * @brief Save image to file.
     * @param filename File path.
     */
    void save(const std::string& filename) const;

    /**
     * @brief Load image.
     * @param filename File path.
     */
    void load(const std::string& filename);

    /**
     * @brief Resize image.
     * @param newWidth New width.
     * @param newHeight New height.
     */
    void resize(int newWidth, int newHeight);

    /**
     * @brief Create thumbnail.
     * @param maxSize Maximum size.
     * @return Thumbnail object.
     */
    std::unique_ptr<FitsImage> createThumbnail(int maxSize) const;

    /**
     * @brief Extract Region of Interest (ROI).
     * @param x Starting X coordinate.
     * @param y Starting Y coordinate.
     * @param width Region width.
     * @param height Region height.
     * @return Extracted region image.
     */
    std::unique_ptr<FitsImage> extractROI(int x, int y, int width,
                                          int height) const;

    /**
     * @brief Apply filter.
     * @param filterType Filter type.
     * @param kernelSize Kernel size (used for some filters only).
     * @param channel Channel (-1 for all channels).
     */
    void applyFilter(FilterType filterType, int kernelSize = 3,
                     int channel = -1);

    /**
     * @brief Apply custom filter.
     * @param kernel Filter kernel.
     * @param channel Channel (-1 for all channels).
     */
    void applyCustomFilter(const std::vector<std::vector<double>>& kernel,
                           int channel = -1);

    /**
     * @brief Blend two images.
     * @param other The other image.
     * @param alpha Blending coefficient (0.0-1.0).
     * @param channel Channel (-1 for all channels).
     */
    void blend(const FitsImage& other, double alpha, int channel = -1);

    /**
     * @brief Histogram equalization.
     * @param channel Channel (-1 for all channels).
     */
    void histogramEqualization(int channel = -1);

    /**
     * @brief Auto adjust levels.
     * @param blackPoint Black point percentage (0.0-1.0).
     * @param whitePoint White point percentage (0.0-1.0).
     * @param channel Channel (-1 for all channels).
     */
    void autoLevels(double blackPoint = 0.02, double whitePoint = 0.98,
                    int channel = -1);

    /**
     * @brief Edge detection.
     * @param filterType Edge detection type.
     * @param channel Channel (-1 for all channels).
     */
    void detectEdges(FilterType filterType = FilterType::SOBEL,
                     int channel = -1);

    /**
     * @brief Apply morphological operation.
     * @param operation Operation type.
     * @param kernelSize Kernel size.
     * @param channel Channel (-1 for all channels).
     */
    void applyMorphology(MorphologicalOperation operation, int kernelSize = 3,
                         int channel = -1);

    /**
     * @brief Denoise.
     * @param filterType Denoising type.
     * @param strength Strength.
     * @param channel Channel (-1 for all channels).
     */
    void removeNoise(FilterType filterType = FilterType::MEDIAN,
                     int strength = 3, int channel = -1);

    /**
     * @brief Add noise.
     * @param noiseType Noise type.
     * @param strength Strength.
     * @param channel Channel (-1 for all channels).
     */
    void addNoise(NoiseType noiseType, double strength, int channel = -1);

    /**
     * @brief Color space conversion.
     * @param fromSpace Source color space.
     * @param toSpace Target color space.
     */
    void convertColorSpace(ColorSpace fromSpace, ColorSpace toSpace);

    /**
     * @brief Correct lens distortion.
     * @param k1 Radial distortion coefficient 1.
     * @param k2 Radial distortion coefficient 2.
     * @param k3 Radial distortion coefficient 3.
     */
    void correctLensDistortion(double k1, double k2 = 0.0, double k3 = 0.0);

    /**
     * @brief Correct vignetting.
     * @param strength Strength.
     * @param radius Radius.
     */
    void correctVignetting(double strength, double radius = 1.0);

    /**
     * @brief Calculate image statistics.
     * @param channel Channel.
     * @return Statistics (min, max, mean, standard deviation).
     */
    std::tuple<double, double, double, double> getStatistics(
        int channel = 0) const;

    /**
     * @brief Compute histogram.
     * @param numBins Number of histogram bins.
     * @param channel Channel.
     * @return Histogram data.
     */
    std::vector<double> computeHistogram(int numBins = 256,
                                         int channel = 0) const;

    /**
     * @brief Image compression.
     * @param algorithm Compression algorithm.
     * @param level Compression level.
     */
    void compress(CompressionAlgorithm algorithm = CompressionAlgorithm::RLE,
                  int level = 5);

    /**
     * @brief Image decompression.
     */
    void decompress();

    /**
     * @brief Apply mathematical operation to each pixel.
     * @param operation Operation function.
     * @param channel Channel (-1 for all channels).
     */
    void applyMathOperation(const std::function<double(double)>& operation,
                            int channel = -1);

    /**
     * @brief Create composite image.
     * @param images Set of source images.
     * @param weights Weights (same length as images).
     * @return Composited image.
     */
    static std::unique_ptr<FitsImage> composite(
        const std::vector<std::reference_wrapper<const FitsImage>>& images,
        const std::vector<double>& weights);

    /**
     * @brief Stack multiple images.
     * @param images Set of source images.
     * @param method Stacking method.
     * @return Stacked image.
     */
    static std::unique_ptr<FitsImage> stack(
        const std::vector<std::reference_wrapper<const FitsImage>>& images,
        StackingMethod method = StackingMethod::MEDIAN);

    /**
     * @brief Get pixel value.
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param channel Channel.
     * @return Pixel value.
     */
    double getPixel(int x, int y, int channel = 0) const;

    /**
     * @brief Set pixel value.
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param value Pixel value.
     * @param channel Channel.
     */
    void setPixel(int x, int y, double value, int channel = 0);

private:
    std::unique_ptr<FITSFile> fitsFile;
    DataType dataType;

    // Helper methods
    ImageHDU& getImageHDU();
    const ImageHDU& getImageHDU() const;

    // Template method call helper
    template <FitsNumeric T>
    void templateOperation(std::function<void(ImageHDU&)> operation);

    template <FitsNumeric T>
    std::unique_ptr<FitsImage> templateReturnOperation(
        std::function<std::unique_ptr<ImageHDU>(const ImageHDU&)> operation)
        const;

#ifdef ATOM_ENABLE_OPENCV
    /**
     * @brief Convert OpenCV type to FITS DataType.
     * @param cvType OpenCV type.
     * @return Corresponding FITS data type.
     */
    static DataType opencvTypeToFitsType(int cvType);

    /**
     * @brief Convert FITS DataType to OpenCV type.
     * @param type FITS data type.
     * @return Corresponding OpenCV type.
     */
    static int fitsTypeToOpenCVType(DataType type);
#endif
};

/**
 * @brief Read data from FITS file.
 * @param filename File path.
 * @return Image object.
 */
std::unique_ptr<FitsImage> loadFitsImage(const std::string& filename);

/**
 * @brief Quickly load FITS image thumbnail.
 * @param filename File path.
 * @param maxSize Maximum size.
 * @return Thumbnail object.
 */
std::unique_ptr<FitsImage> loadFitsThumbnail(const std::string& filename,
                                             int maxSize = 256);

/**
 * @brief Create new FITS image.
 * @param width Width.
 * @param height Height.
 * @param channels Number of channels.
 * @param dataType Data type.
 * @return Image object.
 */
std::unique_ptr<FitsImage> createFitsImage(int width, int height,
                                           int channels = 1,
                                           DataType dataType = DataType::SHORT);

#ifdef ATOM_ENABLE_OPENCV
/**
 * @brief Create FITS image from OpenCV Mat.
 * @param mat OpenCV Mat object.
 * @param dataType Data type (defaults to automatic selection based on Mat
 * type).
 * @return FITS image object.
 */
std::unique_ptr<FitsImage> createFitsFromMat(
    const cv::Mat& mat, DataType dataType = DataType::SHORT);

/**
 * @brief Batch process FITS files in a directory.
 * @param inputDir Input directory.
 * @param outputDir Output directory.
 * @param processor Processing function.
 * @param recursive Whether to process subdirectories recursively.
 * @return Number of successfully processed files.
 */
int processFitsDirectory(const std::string& inputDir,
                         const std::string& outputDir,
                         const std::function<void(FitsImage&)>& processor,
                         bool recursive = false);
#endif

/**
 * @brief Check if a file is a valid FITS file.
 * @param filename File path.
 * @return True if it is a valid FITS file, false otherwise.
 */
bool isValidFits(const std::string& filename);

/**
 * @brief Get basic information about a FITS file.
 * @param filename File path.
 * @return Optional dimension information (width, height, channels). Returns
 * empty if the file is invalid.
 */
std::optional<std::tuple<int, int, int>> getFitsImageInfo(
    const std::string& filename);

}  // namespace image
}  // namespace atom

#endif  // ATOM_IMAGE_FITS_UTILS_HPP
