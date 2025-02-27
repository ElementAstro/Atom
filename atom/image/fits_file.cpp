#include "fits_file.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>

FITSFile::FITSFile(const std::string& filename) { readFITS(filename); }

void FITSFile::readFITS(const std::string& filename) {
    std::filesystem::path filePath(filename);
    if (!std::filesystem::exists(filePath)) {
        throw FITSFileException("File does not exist: " + filename);
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw FITSFileException("Cannot open file: " + filename);
    }

    try {
        hdus.clear();
        while (file && file.peek() != EOF) {
            auto hdu = std::make_unique<ImageHDU>();
            hdu->readHDU(file);
            hdus.push_back(std::move(hdu));
        }
    } catch (const std::exception& e) {
        throw FITSFileException("Error reading FITS file: " +
                                std::string(e.what()));
    }
}

std::future<void> FITSFile::readFITSAsync(const std::string& filename) {
    return std::async(std::launch::async,
                      [this, filename]() { this->readFITS(filename); });
}

void FITSFile::writeFITS(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw FITSFileException("Cannot create file: " + filename);
    }

    try {
        for (const auto& hdu : hdus) {
            hdu->writeHDU(file);
        }
    } catch (const std::exception& e) {
        throw FITSFileException("Error writing FITS file: " +
                                std::string(e.what()));
    }
}

std::future<void> FITSFile::writeFITSAsync(const std::string& filename) const {
    return std::async(std::launch::async,
                      [this, filename]() { this->writeFITS(filename); });
}

size_t FITSFile::getHDUCount() const noexcept { return hdus.size(); }

bool FITSFile::isEmpty() const noexcept { return hdus.empty(); }

const HDU& FITSFile::getHDU(size_t index) const {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }
    return *hdus[index];
}

HDU& FITSFile::getHDU(size_t index) {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }
    return *hdus[index];
}

void FITSFile::addHDU(std::unique_ptr<HDU> hdu) {
    if (!hdu) {
        throw std::invalid_argument("Cannot add null HDU");
    }
    hdus.push_back(std::move(hdu));
}

void FITSFile::removeHDU(size_t index) {
    if (index >= hdus.size()) {
        throw std::out_of_range("HDU index out of range");
    }
    hdus.erase(hdus.begin() + static_cast<std::ptrdiff_t>(index));
}

ImageHDU& FITSFile::createImageHDU(int width, int height, int channels) {
    if (width <= 0 || height <= 0 || channels <= 0) {
        throw std::invalid_argument("Invalid image dimensions");
    }

    auto hdu = std::make_unique<ImageHDU>();
    hdu->setImageSize(width, height, channels);

    // Standard FITS header keywords
    hdu->setHeaderKeyword("SIMPLE", "T");
    hdu->setHeaderKeyword("BITPIX", "16");  // Default to 16-bit
    hdu->setHeaderKeyword("NAXIS", channels > 1 ? "3" : "2");
    hdu->setHeaderKeyword("NAXIS1", std::to_string(width));
    hdu->setHeaderKeyword("NAXIS2", std::to_string(height));
    if (channels > 1) {
        hdu->setHeaderKeyword("NAXIS3", std::to_string(channels));
    }

    // Keep reference before moving
    ImageHDU& imageHDURef = *hdu;
    hdus.push_back(std::move(hdu));

    return imageHDURef;
}
