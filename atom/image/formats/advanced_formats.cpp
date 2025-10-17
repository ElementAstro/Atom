#include "advanced_formats.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <stdexcept>

// Define error macros to avoid atom error system namespace pollution
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error(msg)
#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument(msg)

#ifdef ATOM_IMAGE_HAS_LIBRAW
#include <libraw/libraw.h>
#endif

#ifdef ATOM_IMAGE_HAS_DCMTK
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#endif

#ifdef ATOM_IMAGE_HAS_OPENEXR
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfArray.h>
#endif

#ifdef ATOM_IMAGE_HAS_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

namespace atom::image {

AdvancedFormat AdvancedFormatProcessor::detectFormat(const std::string& filename) const {
    if (!std::filesystem::exists(filename)) {
        return AdvancedFormat::UNKNOWN;
    }

    // Get file extension
    std::string extension = std::filesystem::path(filename).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    // Remove leading dot
    if (!extension.empty() && extension[0] == '.') {
        extension = extension.substr(1);
    }

    // Map extensions to formats
    static const std::unordered_map<std::string, AdvancedFormat> extensionMap = {
        // RAW formats
        {"cr2", AdvancedFormat::CR2},
        {"nef", AdvancedFormat::NEF},
        {"arw", AdvancedFormat::ARW},
        {"dng", AdvancedFormat::DNG},
        {"raf", AdvancedFormat::RAF},
        {"orf", AdvancedFormat::ORF},
        {"rw2", AdvancedFormat::RW2},
        {"pef", AdvancedFormat::PEF},
        {"srw", AdvancedFormat::SRW},
        {"x3f", AdvancedFormat::X3F},
        
        // Medical formats
        {"dcm", AdvancedFormat::DICOM},
        {"dicom", AdvancedFormat::DICOM},
        {"nii", AdvancedFormat::NIFTI},
        {"nifti", AdvancedFormat::NIFTI},
        {"hdr", AdvancedFormat::ANALYZE},
        {"img", AdvancedFormat::ANALYZE},
        {"mnc", AdvancedFormat::MINC},
        {"nrrd", AdvancedFormat::NRRD},
        {"nhdr", AdvancedFormat::NRRD},
        
        // Scientific formats
        {"h5", AdvancedFormat::HDF5},
        {"hdf5", AdvancedFormat::HDF5},
        {"nc", AdvancedFormat::NETCDF},
        {"cdf", AdvancedFormat::NETCDF},
        // Note: FITS format not included in enum, would need to be added

        // HDR formats
        {"exr", AdvancedFormat::OPENEXR},
        {"hdr", AdvancedFormat::RADIANCE},
        {"pic", AdvancedFormat::RADIANCE},
        {"pfm", AdvancedFormat::PFM},
        
        // Modern web formats
        {"avif", AdvancedFormat::AVIF},
        {"heic", AdvancedFormat::HEIF},  // HEIC is a variant of HEIF
        {"heif", AdvancedFormat::HEIF},
        {"jxl", AdvancedFormat::JPEG_XL},

        // Vector formats
        {"svg", AdvancedFormat::SVG},
        {"pdf", AdvancedFormat::PDF},
        {"eps", AdvancedFormat::EPS},
        // Note: AI format not in enum

        // Animation formats
        {"apng", AdvancedFormat::APNG}
        // Note: MNG format not in enum
    };

    auto it = extensionMap.find(extension);
    if (it != extensionMap.end()) {
        return it->second;
    }

    return AdvancedFormat::UNKNOWN;
}

AdvancedFormat AdvancedFormatProcessor::detectFormat(const uint8_t* data, size_t size) const {
    if (!data || size < 16) {
        return AdvancedFormat::UNKNOWN;
    }

    // Check magic bytes for various formats
    
    // DICOM
    if (size > 132 && std::memcmp(data + 128, "DICM", 4) == 0) {
        return AdvancedFormat::DICOM;
    }
    
    // OpenEXR
    if (size > 4 && data[0] == 0x76 && data[1] == 0x2f && data[2] == 0x31 && data[3] == 0x01) {
        return AdvancedFormat::OPENEXR;
    }

    // Note: FITS format detection would need FITS enum value
    
    // HDF5
    if (size > 8 && std::memcmp(data, "\x89HDF\r\n\x1a\n", 8) == 0) {
        return AdvancedFormat::HDF5;
    }
    
    // PDF
    if (size > 4 && std::memcmp(data, "%PDF", 4) == 0) {
        return AdvancedFormat::PDF;
    }
    
    // SVG
    if (size > 5 && (std::memcmp(data, "<?xml", 5) == 0 || std::memcmp(data, "<svg", 4) == 0)) {
        return AdvancedFormat::SVG;
    }
    
    // AVIF
    if (size > 12 && std::memcmp(data + 4, "ftypavif", 8) == 0) {
        return AdvancedFormat::AVIF;
    }
    
    // HEIC/HEIF
    if (size > 12) {
        if (std::memcmp(data + 4, "ftypheic", 8) == 0) {
            return AdvancedFormat::HEIF;  // HEIC is a variant of HEIF
        }
        if (std::memcmp(data + 4, "ftypheif", 8) == 0) {
            return AdvancedFormat::HEIF;
        }
    }
    
    // JPEG XL
    if (size > 12 && std::memcmp(data, "\x00\x00\x00\x0cJXL ", 12) == 0) {
        return AdvancedFormat::JPEG_XL;
    }

    return AdvancedFormat::UNKNOWN;
}

blob AdvancedFormatProcessor::loadImage(const std::string& filename,
                                       AdvancedFormat format,
                                       const std::unordered_map<std::string, std::string>& params) const {
    if (!std::filesystem::exists(filename)) {
        return blob{};
    }

    // Auto-detect format if not specified
    if (format == AdvancedFormat::UNKNOWN) {
        format = detectFormat(filename);
    }

    try {
        switch (format) {
            case AdvancedFormat::CR2:
            case AdvancedFormat::NEF:
            case AdvancedFormat::ARW:
            case AdvancedFormat::DNG:
            case AdvancedFormat::RAF:
            case AdvancedFormat::ORF:
            case AdvancedFormat::RW2:
            case AdvancedFormat::PEF:
            case AdvancedFormat::SRW:
            case AdvancedFormat::X3F: {
                RAWParams rawParams;
                // Parse RAW-specific parameters from params map
                return loadRAW(filename, rawParams);
            }
            
            case AdvancedFormat::DICOM: {
                auto result = loadDICOM(filename);
                return result.first;
            }
            
            case AdvancedFormat::OPENEXR:
            case AdvancedFormat::RADIANCE:
            case AdvancedFormat::PFM: {
                return loadHDR(filename);
            }

            case AdvancedFormat::SVG:
            case AdvancedFormat::PDF:
            case AdvancedFormat::EPS: {
                return loadVector(filename);
            }

            case AdvancedFormat::APNG: {
                auto frames = loadAnimation(filename);
                if (!frames.empty()) {
                    return frames[0].imageData;
                }
                break;
            }
            
            default:
                // Try to load with OpenCV as fallback
#ifdef ATOM_IMAGE_HAS_OPENCV
                {
                    cv::Mat image = cv::imread(filename, cv::IMREAD_UNCHANGED);
                    if (!image.empty()) {
                        return blob(image);
                    }
                }
#endif
                break;
        }
    } catch (const std::exception&) {
        // Handle loading errors
    }

    return blob{};
}

bool AdvancedFormatProcessor::saveImage(const blob& image,
                                       const std::string& filename,
                                       AdvancedFormat format,
                                       const std::unordered_map<std::string, std::string>& params) const {
    if (image.size() == 0) {
        return false;
    }

    try {
        switch (format) {
            case AdvancedFormat::DICOM: {
                DICOMMetadata metadata;
                // Parse DICOM metadata from params
                return saveDICOM(image, filename, metadata);
            }
            
            case AdvancedFormat::OPENEXR:
            case AdvancedFormat::RADIANCE:
            case AdvancedFormat::PFM: {
                std::string compression = params.count("compression") ? params.at("compression") : "zip";
                return saveHDR(image, filename, format, compression);
            }
            
            default:
                // Try to save with OpenCV as fallback
#ifdef ATOM_IMAGE_HAS_OPENCV
                {
                    cv::Mat mat = image.to_mat();
                    return cv::imwrite(filename, mat);
                }
#else
                return false;
#endif
        }
    } catch (const std::exception&) {
        return false;
    }

    return false;
}

blob AdvancedFormatProcessor::loadRAW(const std::string& filename, const RAWParams& params) const {
#ifdef ATOM_IMAGE_HAS_LIBRAW
    try {
        LibRaw rawProcessor;
        
        // Set processing parameters
        rawProcessor.imgdata.params.use_camera_wb = params.useCameraWhiteBalance ? 1 : 0;
        rawProcessor.imgdata.params.use_auto_wb = params.useAutoWhiteBalance ? 1 : 0;
        rawProcessor.imgdata.params.output_color = params.colorSpace;
        rawProcessor.imgdata.params.output_bps = params.outputBitDepth;
        rawProcessor.imgdata.params.gamma_16bit = params.gamma16bit ? 1 : 0;
        rawProcessor.imgdata.params.no_auto_bright = params.noAutoBright ? 1 : 0;
        rawProcessor.imgdata.params.bright = static_cast<float>(params.brightness);
        
        // Open and process RAW file
        int ret = rawProcessor.open_file(filename.c_str());
        if (ret != LIBRAW_SUCCESS) {
            return blob{};
        }
        
        ret = rawProcessor.unpack();
        if (ret != LIBRAW_SUCCESS) {
            return blob{};
        }
        
        ret = rawProcessor.dcraw_process();
        if (ret != LIBRAW_SUCCESS) {
            return blob{};
        }
        
        libraw_processed_image_t* processedImage = rawProcessor.dcraw_make_mem_image(&ret);
        if (!processedImage) {
            return blob{};
        }
        
        // Convert to blob
        size_t dataSize = processedImage->data_size;
        std::vector<uint8_t> imageData(processedImage->data, processedImage->data + dataSize);
        
        LibRaw::dcraw_clear_mem(processedImage);
        
        return blob(imageData);
    } catch (const std::exception&) {
        return blob{};
    }
#else
    // Fallback without LibRaw
    return blob{};
#endif
}

std::pair<blob, DICOMMetadata> AdvancedFormatProcessor::loadDICOM(const std::string& filename,
                                                                 int seriesIndex,
                                                                 int frameIndex) const {
    DICOMMetadata metadata;

#ifdef ATOM_IMAGE_HAS_DCMTK
    try {
        DcmFileFormat fileFormat;
        OFCondition status = fileFormat.loadFile(filename.c_str());

        if (status.bad()) {
            return {blob{}, metadata};
        }

        DcmDataset* dataset = fileFormat.getDataset();
        if (!dataset) {
            return {blob{}, metadata};
        }

        // Extract metadata
        OFString patientName, studyDate, modality;
        dataset->findAndGetOFString(DCM_PatientName, patientName);
        dataset->findAndGetOFString(DCM_StudyDate, studyDate);
        dataset->findAndGetOFString(DCM_Modality, modality);

        metadata.patientName = patientName.c_str();
        metadata.studyDate = studyDate.c_str();
        metadata.modality = modality.c_str();

        // Get image dimensions
        Uint16 rows, cols, samples;
        dataset->findAndGetUint16(DCM_Rows, rows);
        dataset->findAndGetUint16(DCM_Columns, cols);
        dataset->findAndGetUint16(DCM_SamplesPerPixel, samples);

        metadata.width = cols;
        metadata.height = rows;
        metadata.channels = samples;

        // Create DICOM image
        DicomImage* dicomImage = new DicomImage(dataset, EXS_Unknown);
        if (!dicomImage || dicomImage->getStatus() != EIS_Normal) {
            delete dicomImage;
            return {blob{}, metadata};
        }

        // Get pixel data
        const void* pixelData = dicomImage->getOutputData(8); // 8-bit output
        if (!pixelData) {
            delete dicomImage;
            return {blob{}, metadata};
        }

        size_t dataSize = rows * cols * samples;
        std::vector<uint8_t> imageData(static_cast<const uint8_t*>(pixelData),
                                      static_cast<const uint8_t*>(pixelData) + dataSize);

        delete dicomImage;

        return {blob(imageData), metadata};
    } catch (const std::exception&) {
        return {blob{}, metadata};
    }
#else
    // Fallback without DCMTK
    return {blob{}, metadata};
#endif
}

bool AdvancedFormatProcessor::saveDICOM(const blob& image,
                                       const std::string& filename,
                                       const DICOMMetadata& metadata) const {
#ifdef ATOM_IMAGE_HAS_DCMTK
    try {
        DcmFileFormat fileFormat;
        DcmDataset* dataset = fileFormat.getDataset();

        // Set basic DICOM tags
        dataset->putAndInsertString(DCM_PatientName, metadata.patientName.c_str());
        dataset->putAndInsertString(DCM_StudyDate, metadata.studyDate.c_str());
        dataset->putAndInsertString(DCM_Modality, metadata.modality.c_str());
        dataset->putAndInsertUint16(DCM_Rows, metadata.height);
        dataset->putAndInsertUint16(DCM_Columns, metadata.width);
        dataset->putAndInsertUint16(DCM_SamplesPerPixel, metadata.channels);
        dataset->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
        dataset->putAndInsertUint16(DCM_BitsAllocated, 8);
        dataset->putAndInsertUint16(DCM_BitsStored, 8);
        dataset->putAndInsertUint16(DCM_HighBit, 7);
        dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);

        // Set pixel data
        dataset->putAndInsertUint8Array(DCM_PixelData, image.data(), image.size());

        // Save file
        OFCondition status = fileFormat.saveFile(filename.c_str());
        return status.good();
    } catch (const std::exception&) {
        return false;
    }
#else
    return false;
#endif
}

std::vector<AnimationFrame> AdvancedFormatProcessor::loadAnimation(const std::string& filename) const {
    std::vector<AnimationFrame> frames;

#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        cv::VideoCapture cap(filename);
        if (!cap.isOpened()) {
            return frames;
        }

        cv::Mat frame;
        int frameIndex = 0;

        while (cap.read(frame)) {
            if (frame.empty()) {
                break;
            }

            AnimationFrame animFrame;
            animFrame.imageData = blob(frame);
            animFrame.duration = 100; // Default delay
            animFrame.disposalMethod = 0;

            frames.push_back(animFrame);
            frameIndex++;
        }
    } catch (const std::exception&) {
        // Handle error
    }
#endif

    return frames;
}

bool AdvancedFormatProcessor::saveAnimation(const std::vector<AnimationFrame>& frames,
                                           const std::string& filename,
                                           AdvancedFormat format,
                                           int loopCount) const {
    if (frames.empty()) {
        return false;
    }

#ifdef ATOM_IMAGE_HAS_OPENCV
    try {
        // For now, save as video using OpenCV
        if (!frames.empty()) {
            cv::Mat firstFrame = frames[0].imageData.to_mat();
            cv::Size frameSize(firstFrame.cols, firstFrame.rows);

            int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
            double fps = 1000.0 / std::max(1, frames[0].duration); // Convert delay to FPS

            cv::VideoWriter writer(filename, fourcc, fps, frameSize);
            if (!writer.isOpened()) {
                return false;
            }

            for (const auto& frame : frames) {
                cv::Mat mat = frame.imageData.to_mat();
                writer.write(mat);
            }

            return true;
        }
    } catch (const std::exception&) {
        return false;
    }
#endif

    return false;
}

blob AdvancedFormatProcessor::loadHDR(const std::string& filename,
                                     double exposure,
                                     double gamma) const {
#ifdef ATOM_IMAGE_HAS_OPENEXR
    try {
        if (detectFormat(filename) == AdvancedFormat::EXR) {
            Imf::RgbaInputFile file(filename.c_str());
            Imath::Box2i dw = file.dataWindow();

            int width = dw.max.x - dw.min.x + 1;
            int height = dw.max.y - dw.min.y + 1;

            Imf::Array2D<Imf::Rgba> pixels(height, width);
            file.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
            file.readPixels(dw.min.y, dw.max.y);

            // Convert to 8-bit with exposure and gamma correction
            std::vector<uint8_t> imageData(width * height * 3);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const Imf::Rgba& pixel = pixels[y][x];

                    // Apply exposure
                    float r = pixel.r * std::pow(2.0f, static_cast<float>(exposure));
                    float g = pixel.g * std::pow(2.0f, static_cast<float>(exposure));
                    float b = pixel.b * std::pow(2.0f, static_cast<float>(exposure));

                    // Apply gamma correction
                    r = std::pow(r, 1.0f / static_cast<float>(gamma));
                    g = std::pow(g, 1.0f / static_cast<float>(gamma));
                    b = std::pow(b, 1.0f / static_cast<float>(gamma));

                    // Clamp and convert to 8-bit
                    int idx = (y * width + x) * 3;
                    imageData[idx] = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
                    imageData[idx + 1] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
                    imageData[idx + 2] = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
                }
            }

            return blob(imageData);
        }
    } catch (const std::exception&) {
        // Handle error
    }
#endif

    // Fallback to OpenCV
#ifdef ATOM_IMAGE_HAS_OPENCV
    cv::Mat hdrImage = cv::imread(filename, cv::IMREAD_ANYDEPTH | cv::IMREAD_COLOR);
    if (!hdrImage.empty()) {
        cv::Mat ldrImage;
        hdrImage.convertTo(ldrImage, CV_8UC3, 255.0);
        return blob(ldrImage);
    }
#endif

    return blob{};
}

bool AdvancedFormatProcessor::saveHDR(const blob& /*image*/,
                                     const std::string& /*filename*/,
                                     AdvancedFormat /*format*/,
                                     const std::string& /*compression*/) const {
    THROW_RUNTIME_ERROR("HDR saving not available in this build");
}

blob AdvancedFormatProcessor::loadVector(const std::string& /*filename*/,
                                        int /*width*/, int /*height*/,
                                        double /*dpi*/) const {
    THROW_RUNTIME_ERROR("Vector loading not available in this build");
}

std::pair<blob, std::unordered_map<std::string, std::string>>
AdvancedFormatProcessor::loadMicroscopy(const std::string& /*filename*/,
                                        int /*seriesIndex*/, int /*channelIndex*/,
                                        int /*timeIndex*/, int /*zIndex*/) const {
    THROW_RUNTIME_ERROR("Microscopy loading not available in this build");
}

std::pair<blob, std::unordered_map<std::string, std::string>>
AdvancedFormatProcessor::loadSatellite(const std::string& /*filename*/,
                                       const std::vector<int>& /*bandIndices*/) const {
    THROW_RUNTIME_ERROR("Satellite loading not available in this build");
}

bool AdvancedFormatProcessor::convertFormat(const std::string& /*inputFile*/,
                                           const std::string& /*outputFile*/,
                                           AdvancedFormat /*outputFormat*/,
                                           const std::unordered_map<std::string, std::string>& /*params*/) const {
    THROW_RUNTIME_ERROR("Format conversion not available in this build");
}

std::unordered_map<std::string, std::string>
AdvancedFormatProcessor::getFormatInfo(const std::string& /*filename*/) const {
    return {};
}

std::vector<std::string> AdvancedFormatProcessor::getSupportedFormats() const {
    static const std::vector<std::string> kFormats = {
        "RAW", "DICOM", "HDR", "VECTOR", "ANIMATION", "WEBP", "AVIF"};
    return kFormats;
}

bool AdvancedFormatProcessor::isFormatSupported(AdvancedFormat format) const {
    switch (format) {
        case AdvancedFormat::WEBP:
        case AdvancedFormat::AVIF:
        case AdvancedFormat::DNG:
        case AdvancedFormat::DICOM:
        case AdvancedFormat::OPENEXR:
            return true;
        default:
            return false;
    }
}

std::vector<std::string> AdvancedFormatProcessor::getFormatExtensions(
    AdvancedFormat format) const {
    switch (format) {
        case AdvancedFormat::WEBP:
            return {"webp"};
        case AdvancedFormat::AVIF:
            return {"avif", "heif"};
        case AdvancedFormat::OPENEXR:
            return {"exr"};
        case AdvancedFormat::DNG:
            return {"dng"};
        case AdvancedFormat::DICOM:
            return {"dcm"};
        default:
            return {};
    }
}

int AdvancedFormatProcessor::batchConvert(
    const std::vector<std::string>& /*inputFiles*/,
    const std::string& /*outputDir*/, AdvancedFormat /*outputFormat*/,
    const std::unordered_map<std::string, std::string>& /*params*/,
    std::function<void(int, int)> /*progressCallback*/) const {
    THROW_RUNTIME_ERROR("Batch conversion not available in this build");
}

bool AdvancedFormatProcessor::initializeLibraries() const {
    return false;
}

bool AdvancedFormatProcessor::loadFormatLibrary(AdvancedFormat /*format*/) const {
    return false;
}

std::string AdvancedFormatProcessor::getFormatName(AdvancedFormat format) const {
    switch (format) {
        case AdvancedFormat::WEBP:
            return "WEBP";
        case AdvancedFormat::AVIF:
            return "AVIF";
        case AdvancedFormat::OPENEXR:
            return "OPENEXR";
        case AdvancedFormat::DNG:
            return "DNG";
        case AdvancedFormat::DICOM:
            return "DICOM";
        default:
            return "UNKNOWN";
    }
}

std::unordered_map<std::string, std::string> AdvancedFormatProcessor::parseFormatParams(
    const std::unordered_map<std::string, std::string>& params,
    AdvancedFormat /*format*/) const {
    return params;
}

std::unique_ptr<AdvancedFormatProcessor> createOptimalFormatProcessor(
    bool /*enableAllFormats*/) {
    return std::make_unique<AdvancedFormatProcessor>();
}

} // namespace atom::image
