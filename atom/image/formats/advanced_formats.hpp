#ifndef ATOM_IMAGE_ADVANCED_FORMATS_HPP
#define ATOM_IMAGE_ADVANCED_FORMATS_HPP

/**
 * @file advanced_formats.hpp
 * @brief Advanced image format support
 *
 * This module provides support for advanced and specialized image formats
 * including RAW camera formats, medical imaging formats, scientific formats,
 * and modern web formats.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include "atom/image/core/image_blob.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace atom::image {

/**
 * @brief Supported advanced image formats
 */
enum class AdvancedFormat {
    // RAW camera formats
    CR2,            // Canon RAW
    NEF,            // Nikon RAW
    ARW,            // Sony RAW
    DNG,            // Adobe Digital Negative
    RAF,            // Fujifilm RAW
    ORF,            // Olympus RAW
    RW2,            // Panasonic RAW
    PEF,            // Pentax RAW
    SRW,            // Samsung RAW
    X3F,            // Sigma RAW

    // Medical imaging formats
    DICOM,          // Digital Imaging and Communications in Medicine
    NIFTI,          // Neuroimaging Informatics Technology Initiative
    ANALYZE,        // Analyze format
    MINC,           // Medical Image NetCDF
    NRRD,           // Nearly Raw Raster Data

    // Scientific formats
    HDF5,           // Hierarchical Data Format 5
    NETCDF,         // Network Common Data Form
    GRIB,           // Gridded Binary
    MATLAB,         // MATLAB format

    // Modern web formats
    WEBP,           // WebP format
    AVIF,           // AV1 Image File Format
    HEIF,           // High Efficiency Image Format
    JPEG_XL,        // JPEG XL

    // Vector formats
    SVG,            // Scalable Vector Graphics
    PDF,            // Portable Document Format
    EPS,            // Encapsulated PostScript

    // Archive formats
    ICO,            // Windows Icon
    ICNS,           // macOS Icon
    CUR,            // Windows Cursor

    // Specialized formats
    OPENEXR,        // OpenEXR HDR format
    RADIANCE,       // Radiance HDR format
    PFM,            // Portable Float Map
    XCF,            // GIMP native format
    PSD,            // Photoshop Document

    // Animation formats
    GIF,            // Graphics Interchange Format
    APNG,           // Animated PNG
    WEBP_ANIM,      // Animated WebP

    // Microscopy formats
    LSM,            // Zeiss LSM
    CZI,            // Zeiss CZI
    LIF,            // Leica LIF
    ND2,            // Nikon ND2
    OIB,            // Olympus OIB

    // Satellite/GIS formats
    GEOTIFF,        // GeoTIFF
    NITF,           // National Imagery Transmission Format
    MrSID,          // Multi-resolution Seamless Image Database
    ECW,            // Enhanced Compression Wavelet

    UNKNOWN         // Unknown format
};

/**
 * @brief RAW processing parameters
 */
struct RAWParams {
    double exposure = 0.0;          // Exposure compensation
    double highlights = 0.0;        // Highlight recovery
    double shadows = 0.0;           // Shadow recovery
    double whites = 0.0;            // White point adjustment
    double blacks = 0.0;            // Black point adjustment
    double clarity = 0.0;           // Clarity/structure
    double vibrance = 0.0;          // Vibrance adjustment
    double saturation = 0.0;        // Saturation adjustment
    double temperature = 0.0;       // Color temperature
    double tint = 0.0;              // Tint adjustment
    double sharpness = 0.0;         // Sharpening amount
    double noiseReduction = 0.0;    // Noise reduction
    bool autoWhiteBalance = true;   // Auto white balance
    bool autoExposure = false;      // Auto exposure
    std::string colorSpace = "sRGB"; // Output color space
    int bitDepth = 8;               // Output bit depth
};

/**
 * @brief DICOM metadata structure
 */
struct DICOMMetadata {
    std::string patientName;
    std::string patientID;
    std::string studyDate;
    std::string modality;
    std::string manufacturer;
    std::string modelName;
    double pixelSpacing[2] = {1.0, 1.0};
    double sliceThickness = 1.0;
    int bitsAllocated = 16;
    int bitsStored = 16;
    int samplesPerPixel = 1;
    std::string photometricInterpretation;
    std::unordered_map<std::string, std::string> customTags;
};

/**
 * @brief Animation frame information
 */
struct AnimationFrame {
    blob imageData;                 // Frame image data
    int duration = 100;             // Frame duration in milliseconds
    int disposalMethod = 0;         // Frame disposal method
    int offsetX = 0;                // X offset
    int offsetY = 0;                // Y offset
    bool transparent = false;       // Has transparency
    uint32_t transparentColor = 0;  // Transparent color index
};

/**
 * @brief Advanced format processor
 */
class AdvancedFormatProcessor {
public:
    AdvancedFormatProcessor() = default;
    virtual ~AdvancedFormatProcessor() = default;

    /**
     * @brief Detect image format from file
     * @param filename Path to image file
     * @return Detected format
     */
    virtual AdvancedFormat detectFormat(const std::string& filename) const;

    /**
     * @brief Detect image format from data
     * @param data Image data buffer
     * @param size Data size
     * @return Detected format
     */
    virtual AdvancedFormat detectFormat(const uint8_t* data, size_t size) const;

    /**
     * @brief Load image from advanced format
     * @param filename Path to image file
     * @param format Image format (auto-detect if UNKNOWN)
     * @param params Format-specific parameters
     * @return Loaded image blob
     */
    virtual blob loadImage(const std::string& filename,
                          AdvancedFormat format = AdvancedFormat::UNKNOWN,
                          const std::unordered_map<std::string, std::string>& params = {}) const;

    /**
     * @brief Save image to advanced format
     * @param image Image blob to save
     * @param filename Output filename
     * @param format Output format
     * @param params Format-specific parameters
     * @return Success status
     */
    virtual bool saveImage(const blob& image,
                          const std::string& filename,
                          AdvancedFormat format,
                          const std::unordered_map<std::string, std::string>& params = {}) const;

    /**
     * @brief Load RAW camera image
     * @param filename Path to RAW file
     * @param params RAW processing parameters
     * @return Processed image blob
     */
    virtual blob loadRAW(const std::string& filename, const RAWParams& params = {}) const;

    /**
     * @brief Load DICOM medical image
     * @param filename Path to DICOM file
     * @param seriesIndex Series index (for multi-series files)
     * @param frameIndex Frame index (for multi-frame files)
     * @return Loaded image blob and metadata
     */
    virtual std::pair<blob, DICOMMetadata> loadDICOM(const std::string& filename,
                                                    int seriesIndex = 0,
                                                    int frameIndex = 0) const;

    /**
     * @brief Save DICOM medical image
     * @param image Image blob to save
     * @param filename Output filename
     * @param metadata DICOM metadata
     * @return Success status
     */
    virtual bool saveDICOM(const blob& image,
                          const std::string& filename,
                          const DICOMMetadata& metadata) const;

    /**
     * @brief Load animated image (GIF, APNG, WebP)
     * @param filename Path to animated image file
     * @return Vector of animation frames
     */
    virtual std::vector<AnimationFrame> loadAnimation(const std::string& filename) const;

    /**
     * @brief Save animated image
     * @param frames Vector of animation frames
     * @param filename Output filename
     * @param format Animation format
     * @param loopCount Number of loops (0 = infinite)
     * @return Success status
     */
    virtual bool saveAnimation(const std::vector<AnimationFrame>& frames,
                              const std::string& filename,
                              AdvancedFormat format,
                              int loopCount = 0) const;

    /**
     * @brief Load HDR image (OpenEXR, Radiance)
     * @param filename Path to HDR file
     * @param exposure Exposure adjustment
     * @param gamma Gamma correction
     * @return HDR image blob (float data)
     */
    virtual blob loadHDR(const std::string& filename,
                        double exposure = 0.0,
                        double gamma = 1.0) const;

    /**
     * @brief Save HDR image
     * @param image HDR image blob (float data)
     * @param filename Output filename
     * @param format HDR format
     * @param compression Compression method
     * @return Success status
     */
    virtual bool saveHDR(const blob& image,
                        const std::string& filename,
                        AdvancedFormat format,
                        const std::string& compression = "zip") const;

    /**
     * @brief Load vector image (SVG, PDF, EPS)
     * @param filename Path to vector file
     * @param width Rasterization width
     * @param height Rasterization height
     * @param dpi Resolution in DPI
     * @return Rasterized image blob
     */
    virtual blob loadVector(const std::string& filename,
                           int width = 0,
                           int height = 0,
                           double dpi = 96.0) const;

    /**
     * @brief Load microscopy image
     * @param filename Path to microscopy file
     * @param seriesIndex Series index
     * @param channelIndex Channel index
     * @param timeIndex Time point index
     * @param zIndex Z-stack index
     * @return Loaded image blob with metadata
     */
    virtual std::pair<blob, std::unordered_map<std::string, std::string>>
    loadMicroscopy(const std::string& filename,
                   int seriesIndex = 0,
                   int channelIndex = 0,
                   int timeIndex = 0,
                   int zIndex = 0) const;

    /**
     * @brief Load satellite/GIS image
     * @param filename Path to satellite image file
     * @param bandIndices Band indices to load (empty = all bands)
     * @return Multi-band image blob with geospatial metadata
     */
    virtual std::pair<blob, std::unordered_map<std::string, std::string>>
    loadSatellite(const std::string& filename,
                  const std::vector<int>& bandIndices = {}) const;

    /**
     * @brief Convert between formats
     * @param inputFile Input file path
     * @param outputFile Output file path
     * @param outputFormat Target format
     * @param params Conversion parameters
     * @return Success status
     */
    virtual bool convertFormat(const std::string& inputFile,
                              const std::string& outputFile,
                              AdvancedFormat outputFormat,
                              const std::unordered_map<std::string, std::string>& params = {}) const;

    /**
     * @brief Get format information
     * @param filename Path to image file
     * @return Format information and metadata
     */
    virtual std::unordered_map<std::string, std::string> getFormatInfo(const std::string& filename) const;

    /**
     * @brief Get supported formats
     * @return Vector of supported format names
     */
    virtual std::vector<std::string> getSupportedFormats() const;

    /**
     * @brief Check if format is supported
     * @param format Format to check
     * @return True if supported
     */
    virtual bool isFormatSupported(AdvancedFormat format) const;

    /**
     * @brief Get format file extensions
     * @param format Image format
     * @return Vector of file extensions
     */
    virtual std::vector<std::string> getFormatExtensions(AdvancedFormat format) const;

    /**
     * @brief Batch convert images
     * @param inputFiles Vector of input file paths
     * @param outputDir Output directory
     * @param outputFormat Target format
     * @param params Conversion parameters
     * @param progressCallback Progress callback function
     * @return Number of successfully converted files
     */
    virtual int batchConvert(const std::vector<std::string>& inputFiles,
                            const std::string& outputDir,
                            AdvancedFormat outputFormat,
                            const std::unordered_map<std::string, std::string>& params = {},
                            std::function<void(int, int)> progressCallback = nullptr) const;

protected:
    /**
     * @brief Initialize format libraries
     * @return Success status
     */
    virtual bool initializeLibraries() const;

    /**
     * @brief Load format-specific library
     * @param format Format to load library for
     * @return Success status
     */
    virtual bool loadFormatLibrary(AdvancedFormat format) const;

    /**
     * @brief Get format name string
     * @param format Format enum
     * @return Format name
     */
    virtual std::string getFormatName(AdvancedFormat format) const;

    /**
     * @brief Parse format-specific parameters
     * @param params Parameter map
     * @param format Target format
     * @return Parsed parameters
     */
    virtual std::unordered_map<std::string, std::string>
    parseFormatParams(const std::unordered_map<std::string, std::string>& params,
                      AdvancedFormat format) const;
};

/**
 * @brief Factory function to create optimal advanced format processor
 * @param enableAllFormats Whether to enable all available formats
 * @return Unique pointer to format processor
 */
std::unique_ptr<AdvancedFormatProcessor> createOptimalFormatProcessor(bool enableAllFormats = true);

} // namespace atom::image

#endif // ATOM_IMAGE_ADVANCED_FORMATS_HPP
