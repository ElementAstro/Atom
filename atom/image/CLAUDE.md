# atom::image Module Documentation

[根目录](../../CLAUDE.md) > **image**

---

## Module Overview

The **image** module provides comprehensive image processing capabilities for the Atom framework, with specialized support for astronomical image formats (FITS, SER), computer vision operations, and advanced processing pipelines.

### Module Metadata

| Attribute | Value |
|-----------|-------|
| **Version** | 1.0.0 |
| **Namespace** | `atom::image` |
| **Dependencies** | atom-algorithm, atom-io, atom-async |
| **Optional Deps** | OpenCV (computer vision), CFITSIO (FITS), Tesseract (OCR) |

---

## Module Responsibilities

### Core Capabilities

1. **Astronomical Image Formats**
   - FITS (Flexible Image Transport System) - Primary astronomical format
   - SER (Simple Extended Resolution) - Planetary/lunar imaging
   - Advanced FITS features: WCS headers, compression, HDU support

2. **Image Processing Operations**
   - Filters: Gaussian, median, bilateral, edge detection
   - Transforms: rotation, scaling, flipping, cropping
   - Enhancement: brightness/contrast, histogram equalization, noise reduction
   - Computer Vision: feature detection, object tracking, optical flow

3. **Metadata Handling**
   - EXIF: Extract and manipulate image metadata
   - IPTC: Press metadata standards
   - XMP: Extensible metadata platform
   - FITS headers: Astronomical metadata

4. **I/O Operations**
   - Format detection and automatic loading
   - Multi-format support with unified interface
   - Async I/O for large files

5. **Optional Advanced Features**
   - OCR (Optical Character Recognition) with Tesseract
   - GPU acceleration with OpenCV
   - Machine Learning integration
   - Real-time video processing

---

## Entry Points and Public APIs

### Main Header

```cpp
#include "atom/image/image.hpp"
```

This provides access to all image processing functionality with conditional compilation for optional features:

```cpp
namespace atom::image {

// Version and feature detection
constexpr Version getVersion();
constexpr Features getFeatures();

// Initialization
bool initialize();
void cleanup();

} // namespace atom::image
```

### Feature Detection

```cpp
struct Features {
    static constexpr bool HAS_OPENCV = true;   // OpenCV available
    static constexpr bool HAS_CFITSIO = true;  // FITS support
    static constexpr bool HAS_OCR = true;      // OCR support
};
```

### Core Classes and APIs

#### Image Loading and Saving

```cpp
namespace atom::image::io {

// Format detector
enum class ImageFormat {
    UNKNOWN,
    FITS,
    SER,
    PNG,
    JPEG,
    TIFF,
    BMP
};

ImageFormat detectFormat(const std::string& filepath);

// Image loader
class ImageLoader {
public:
    static ImageBlob load(const std::string& filepath);
    static ImageBlob load(const std::string& filepath, const LoadOptions& options);
    static std::future<ImageBlob> loadAsync(const std::string& filepath);
};

// Image saver
class ImageSaver {
public:
    static void save(const ImageBlob& image, const std::string& filepath);
    static void save(const ImageBlob& image, const std::string& filepath,
                    const SaveOptions& options);
};

} // namespace atom::image::io
```

#### FITS Format Support

```cpp
namespace atom::image::formats {

// FITS file handling
class FitsFile {
public:
    FitsFile(const std::string& filepath);
    ~FitsFile();

    // HDU (Header Data Unit) access
    std::shared_ptr<HDU> getPrimaryHDU();
    std::vector<std::shared_ptr<HDU>> getAllHDUs();

    // Header operations
    FitsHeader& getHeader();
    const FitsHeader& getHeader() const;

    // Data access
    ImageBlob getData() const;
    void setData(const ImageBlob& data);

    // WCS (World Coordinate System)
    bool hasWCS() const;
    std::string getWCS() const;
};

// FITS HDU types
enum class HDUType {
    IMAGE_HDU,
    ASCII_TABLE_HDU,
    BINARY_TABLE_HDU
};

// HDU base class
class HDU {
public:
    virtual HDUType getType() const = 0;
    FitsHeader getHeader() const;
};

// Image HDU (astronomical images)
class ImageHDU : public HDU {
public:
    ImageBlob getData() const;
    void setData(const ImageBlob& data);

    int getBitDepth() const;
    std::vector<size_t> getDimensions() const;
};

// FITS header operations
class FitsHeader {
public:
    std::string getString(const std::string& key) const;
    int getInt(const std::string& key) const;
    double getDouble(const std::string& key) const;

    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);

    bool hasKey(const std::string& key) const;
    std::vector<std::string> getKeys() const;
};

} // namespace atom::image::formats
```

#### SER Format Support (OpenCV required)

```cpp
namespace atom::image::formats::ser {

// SER file handling
class SerReader {
public:
    SerReader(const std::string& filepath);

    // Frame access
    cv::Mat getFrame(size_t index) const;
    size_t getFrameCount() const;

    // SER metadata
    int getWidth() const;
    int getHeight() const;
    int getBitDepth() const;
    double getFPS() const;

    // Advanced features
    std::vector<cv::Mat> getFrames(size_t start, size_t count) const;
};

class SerWriter {
public:
    SerWriter(const std::string& filepath, int width, int height, int bitDepth);

    void writeFrame(const cv::Mat& frame);
    void close();

    // Metadata
    void setObserver(const std::string& observer);
    void setInstrument(const std::string& instrument);
    void setTelescope(const std::string& telescope);
};

// SER frame processor
class FrameProcessor {
public:
    // Quality assessment
    static double calculateQuality(const cv::Mat& frame);

    // Image registration
    static cv::Mat registerFrame(const cv::Mat& frame, const cv::Mat& reference);

    // Debayering (color reconstruction)
    static cv::Mat debayer(const cv::Mat& frame, const std::string& pattern);

    // Drizzle (integration algorithm)
    static cv::Mat drizzleStack(const std::vector<cv::Mat>& frames, double scale);
};

// Lucky imaging (select and stack best frames)
class LuckyImaging {
public:
    LuckyImaging(size_t keepCount);

    void addFrame(const cv::Mat& frame);
    cv::Mat getResult() const;

private:
    std::vector<std::pair<double, cv::Mat>> scoredFrames_;
    size_t keepCount_;
};

} // namespace atom::image::formats::ser
```

#### Image Processing Operations

```cpp
namespace atom::image::processing {

// Main image processor
class ImageProcessor {
public:
    ImageProcessor();

    // Filters
    cv::Mat applyGaussianBlur(const cv::Mat& image, double sigma);
    cv::Mat applyMedianFilter(const cv::Mat& image, int kernelSize);
    cv::Mat applyBilateralFilter(const cv::Mat& image, double sigmaColor, double sigmaSpace);
    cv::Mat detectEdges(const cv::Mat& image, double threshold1, double threshold2);

    // Transforms
    cv::Mat rotate(const cv::Mat& image, double degrees);
    cv::Mat scale(const cv::Mat& image, double factor, InterpolationMethod method);
    cv::Mat flip(const cv::Mat& image, FlipDirection direction);
    cv::Mat crop(const cv::Mat& image, const cv::Rect& roi);

    // Enhancement
    cv::Mat adjustBrightness(const cv::Mat& image, double alpha);
    cv::Mat adjustContrast(const cv::Mat& image, double beta);
    cv::Mat histogramEqualization(const cv::Mat& image);
    cv::Mat reduceNoise(const cv::Mat& image, const std::string& method);

    // Advanced operations
    cv::Mat deconvolve(const cv::Mat& image, const cv::Mat& psf, int iterations);
};

// Computer vision operations
class ComputerVision {
public:
    // Feature detection
    std::vector<cv::KeyPoint> detectKeypoints(const cv::Mat& image);
    cv::Mat computeDescriptors(const cv::Mat& image,
                               const std::vector<cv::KeyPoint>& keypoints);

    // Object tracking
    cv::Rect trackObject(const cv::Mat& frame, const cv::Rect& previous);

    // Optical flow
    std::vector<cv::Point2f> calculateOpticalFlow(const cv::Mat& prev,
                                                  const cv::Mat& curr);

    // Image registration
    cv::Mat registerImages(const cv::Mat& source, const cv::Mat& target);
};

// GPU acceleration
class GPUAccelerator {
public:
    static bool isAvailable();

    static cv::Mat gaussianBlurGPU(const cv::Mat& image, double sigma);
    static cv::Mat resizeGPU(const cv::Mat& image, const cv::Size& size);
    static cv::Mat convertColorSpaceGPU(const cv::Mat& image, int code);
};

} // namespace atom::image::processing
```

#### Metadata Handling

```cpp
namespace atom::image::metadata {

// EXIF metadata
class ExifReader {
public:
    ExifReader(const std::string& filepath);

    std::string getCameraMake() const;
    std::string getCameraModel() const;
    std::string getDateTime() const;
    double getFocalLength() const;
    double getAperture() const;
    double getISO() const;
    double getExposureTime() const;

    GPSInfo getGPSInfo() const;
};

class ExifWriter {
public:
    ExifWriter(const std::string& filepath);

    void setCameraMake(const std::string& make);
    void setCameraModel(const std::string& model);
    void setDateTime(const std::string& datetime);
    void setGPSInfo(const GPSInfo& gps);
};

// IPTC metadata
class IptcReader {
public:
    IptcReader(const std::string& filepath);

    std::string getCaption() const;
    std::string getAuthor() const;
    std::string getKeywords() const;
    std::string getCopyright() const;
};

class IptcWriter {
public:
    IptcWriter(const std::string& filepath);

    void setCaption(const std::string& caption);
    void setAuthor(const std::string& author);
    void setKeywords(const std::string& keywords);
    void setCopyright(const std::string& copyright);
};

// XMP metadata
class XmpParser {
public:
    static std::map<std::string, std::string> parse(const std::string& xmpData);
    static std::string serialize(const std::map<std::string, std::string>& data);
};

} // namespace atom::image::metadata
```

#### OCR Support (Optional)

```cpp
#ifdef ATOM_IMAGE_HAS_OCR

namespace atom::image::processing::ocr {

// OCR processor
class OCRProcessor {
public:
    OCRProcessor();
    explicit OCRProcessor(const std::string& language);

    std::string recognizeText(const cv::Mat& image);
    std::vector<std::string> recognizeTextLines(const cv::Mat& image);

    void setLanguage(const std::string& language);
    void setPageSegMode(int mode);

    // Configuration
    void setWhitelist(const std::string& chars);
    void setBlacklist(const std::string& chars);
};

// Spell checking for OCR output
class SpellChecker {
public:
    SpellChecker();

    std::string correctSpell(const std::string& text);
    std::vector<std::string> suggestCorrections(const std::string& word);
};

} // namespace atom::image::processing::ocr

#endif // ATOM_IMAGE_HAS_OCR
```

---

## Key Dependencies and Configuration

### Required Dependencies

- **atom-algorithm**: Image processing algorithms
- **atom-io**: File I/O operations
- **atom-async**: Async I/O support

### Optional Dependencies

| Dependency | Purpose | CMake Flag |
|------------|---------|------------|
| **OpenCV** | Computer vision, SER format | `ATOM_IMAGE_HAS_OPENCV` |
| **CFITSIO** | FITS format support | `ATOM_IMAGE_HAS_CFITSIO` |
| **Tesseract** | OCR capabilities | `ATOM_IMAGE_HAS_OCR` |
| **Leptonica** | Image processing for OCR | Required for Tesseract |
| **nlohmann_json** | JSON config parsing | `ATOM_OCR_HAS_JSON` |

### Conditional Compilation

```cpp
#ifdef ATOM_IMAGE_HAS_OPENCV
    // OpenCV-dependent code
#endif

#ifdef ATOM_IMAGE_HAS_CFITSIO
    // FITS-dependent code
#endif

#ifdef ATOM_IMAGE_HAS_OCR
    // OCR-dependent code
#endif
```

---

## Data Models and Structures

### ImageBlob

Universal image container:

```cpp
class ImageBlob {
public:
    ImageBlob();
    ImageBlob(int width, int height, int channels);

    // Data access
    uint8_t* data();
    const uint8_t* data() const;
    size_t size() const;

    // Properties
    int getWidth() const;
    int getHeight() const;
    int getChannels() const;
    int getDepth() const;  // Bits per channel

    // Conversion
    cv::Mat toMat() const;
    static ImageBlob fromMat(const cv::Mat& mat);

    // Operations
    ImageBlob clone() const;
    bool isEmpty() const;
};
```

### GPSInfo

GPS metadata structure:

```cpp
struct GPSInfo {
    double latitude;
    double longitude;
    double altitude;
    std::string latitudeRef;  // "N" or "S"
    std::string longitudeRef; // "E" or "W"
    std::string altitudeRef;  // "above" or "below"
};
```

### LoadOptions / SaveOptions

Configuration for I/O operations:

```cpp
struct LoadOptions {
    bool cacheInMemory = true;
    bool verifyChecksum = true;
    int maxDimension = 0;  // 0 = no limit
    std::string preferredFormat;
};

struct SaveOptions {
    int quality = 95;  // For lossy formats
    bool preserveMetadata = true;
    bool compress = true;
    std::string compressionLevel;  // Format-specific
};
```

---

## Testing and Quality

### Test Coverage

Tests are located in `tests/image/`:

```
tests/image/
├── test_formats.cpp        # Format loading/saving
├── test_fits.cpp           # FITS-specific tests
├── test_ser.cpp            # SER-specific tests
├── test_processing.cpp     # Image processing
├── test_metadata.cpp       # Metadata handling
├── test_ocr.cpp            # OCR tests (if available)
└── CMakeLists.txt
```

### Running Tests

```bash
# Build image tests
cmake -B build -DATOM_BUILD_IMAGE=ON -DATOM_BUILD_TESTS=ON
cmake --build build

# Run image tests
ctest -R "image_*" --output-on-failure

# Run specific test
./build/tests/image/test_fits
```

### Test Data

Test images are stored in `tests/image/data/`:

```
tests/image/data/
├── fits/                   # FITS test files
│   ├── simple.fits
│   ├── compressed.fits
│   └── wcs.fits
├── ser/                    # SER test files
│   ├── planetary.ser
│   └── lunar.ser
└── test_images/            # Other test images
    ├── exif.jpg
    └── iptc.tif
```

---

## Common Development Tasks

### Adding a New Image Format

1. Create format handler in `formats/<format>_loader.hpp` and `.cpp`
2. Add to format detector in `io/format_detector.cpp`
3. Update `CMakeLists.txt` with new sources
4. Add tests in `tests/image/test_formats.cpp`
5. Add example in `example/image/formats/`

### Adding Image Processing Operations

1. Add operation to `processing/image_processor.hpp`
2. Implement in `processing/image_processor.cpp`
3. Add GPU variant if applicable
4. Add tests and benchmarks
5. Document in `CLAUDE.md`

### Integrating with OpenCV

```cpp
#include "atom/image/image.hpp"

// Convert ImageBlob to cv::Mat
atom::image::ImageBlob blob = /* ... */;
cv::Mat mat = blob.toMat();

// Process with OpenCV
cv::Mat result;
cv::GaussianBlur(mat, result, cv::Size(5, 5), 1.5);

// Convert back
atom::image::ImageBlob resultBlob = atom::image::ImageBlob::fromMat(result);
```

### Using FITS Files

```cpp
#include "atom/image/formats/fits.hpp"

using namespace atom::image::formats;

// Open FITS file
FitsFile fits("observation.fits");

// Access primary HDU
auto primaryHDU = fits.getPrimaryHDU();
ImageBlob data = primaryHDU->getData();

// Read header
FitsHeader header = fits.getHeader();
double exposure = header.getDouble("EXPTIME");
std::string observer = header.getString("OBSERVER");

// Write header
header.setDouble("EXPTIME", 120.0);
header.setString("OBSERVER", "Jane Doe");

// Access WCS
if (fits.hasWCS()) {
    std::string wcs = fits.getWCS();
    // Parse and use WCS data
}
```

---

## Usage Examples

### Loading and Processing Images

```cpp
#include "atom/image/image.hpp"

using namespace atom::image;

// Load image
io::ImageLoader loader;
ImageBlob img = loader.load("observation.fits");

// Process image
processing::ImageProcessor processor;
ImageBlob blurred = processor.applyGaussianBlur(img, 2.0);
ImageBlob edges = processor.detectEdges(img, 100, 200);

// Save result
io::ImageSaver::save(blurred, "output.fits");
```

### SER File Processing (Lucky Imaging)

```cpp
#include "atom/image/formats/ser/ser_reader.h"

using namespace atom::image::formats::ser;

// Open SER file
SerReader reader("planetary_video.ser");

// Lucky imaging
LuckyImaging lucky(100);  // Keep best 100 frames
for (size_t i = 0; i < reader.getFrameCount(); ++i) {
    cv::Mat frame = reader.getFrame(i);
    double quality = FrameProcessor::calculateQuality(frame);
    lucky.addFrame(frame);
}

// Get stacked result
cv::Mat result = lucky.getResult();
```

### FITS WCS Handling

```cpp
#include "atom/image/formats/fits_wcs.hpp"

using namespace atom::image::formats;

FitsFile fits("image_with_wcs.fits");

// Check for WCS
if (fits.hasWCS()) {
    auto wcs = std::make_shared<FitsWCS>(fits.getHeader());

    // Convert pixel to sky coordinates
    auto raDec = wcs->pixelToSky(512, 512);
    std::cout << "RA: " << raDec.first << ", Dec: " << raDec.second << std::endl;

    // Convert sky to pixel coordinates
    auto pixel = wcs->skyToPixel(raDec.first, raDec.second);
}
```

### Metadata Extraction

```cpp
#include "atom/image/metadata/exif.hpp"

using namespace atom::image::metadata;

// Read EXIF
ExifReader exif("photo.jpg");
std::string camera = exif.getCameraModel();
double focalLength = exif.getFocalLength();
GPSInfo gps = exif.getGPSInfo();

// Write EXIF
ExifWriter writer("output.jpg");
writer.setCameraMake("Canon");
writer.setCameraModel("EOS 5D Mark IV");
writer.setGPSInfo(gps);
```

---

## Integration with Other Modules

### Used By

- **Example applications**: Image processing demos
- **Astronomy tools**: FITS/SER processing pipelines
- **Computer vision applications**: Object detection, tracking

### Using

- **algorithm**: Image processing algorithms, compression
- **io**: File I/O operations, async loading
- **async**: Async I/O for large files
- **utils**: String utilities, error handling

---

## Platform-Specific Notes

### Windows

- OpenCV must be built with VS2019/2022
- CFITSIO requires MinGW or vcpkg

### Linux

- System packages: `libopencv-dev`, `libcfitsio-dev`, `tesseract-ocr`

### macOS

- Homebrew: `brew install opencv cfitsio tesseract`

---

## Known Limitations

1. **OpenCV Required**: Some features require OpenCV (computer vision, SER format)
2. **CFITSIO Required**: FITS support requires CFITSIO library
3. **Memory Usage**: Large FITS files can consume significant memory
4. **OCR Accuracy**: OCR accuracy depends on image quality and language data

---

## Future Enhancements

- [ ] Add support for more astronomical formats (XISF, etc.)
- [ ] Implement more advanced image registration algorithms
- [ ] Add GPU acceleration for more operations
- [ ] Support for multi-threaded processing pipelines
- [ ] Integration with astronomy libraries (astropy, WCSLIB)

---

## FAQ

### Q: How do I check if OpenCV is available?

```cpp
#ifdef ATOM_IMAGE_HAS_OPENCV
    // OpenCV-dependent code
#endif
```

### Q: Can I read compressed FITS files?

Yes, the module supports various FITS compression methods (RICE, GZIP, PLIO).

### Q: How do I handle large FITS files?

Use async loading and processing:

```cpp
auto future = io::ImageLoader::loadAsync("large_file.fits");
// Do other work...
ImageBlob img = future.get();
```

### Q: What SER color patterns are supported?

All standard Bayer patterns: RGGB, GRBG, GBRG, BGGR.

---

## Change Log

### 2025-01-15

- Initial module documentation
- Documented FITS, SER, and processing APIs
- Added metadata handling documentation
- Added OCR support documentation

---

**Document Maintainer:** Atom Framework Team
**Last Updated:** 2025-01-15
**Module Version:** 1.0.0
