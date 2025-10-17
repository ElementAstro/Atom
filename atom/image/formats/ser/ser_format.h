// ser_format.h
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace serastro {

// SER format constants
inline constexpr std::string_view SER_FILE_ID = "LUCAM-RECORDER";

// Color ID definitions
enum class SERColorID : uint32_t {
    Mono = 0,
    BayerRGGB = 1,
    BayerGRBG = 2,
    BayerGBRG = 3,
    BayerBGGR = 4,
    RGB = 8,
    BGR = 9
};

// SER timestamp structure
struct SERTimestamp {
    uint64_t nanoseconds;

    SERTimestamp() : nanoseconds(0) {}
    explicit SERTimestamp(uint64_t ns) : nanoseconds(ns) {}

    std::chrono::system_clock::time_point toTimePoint() const {
        using namespace std::chrono;
        // SER timestamps are nanoseconds since 2001-01-01
        static const auto serEpoch = sys_days{January / 1 / 2001};
        return serEpoch + std::chrono::nanoseconds{nanoseconds};
    }

    static SERTimestamp fromTimePoint(
        const std::chrono::system_clock::time_point& tp) {
        using namespace std::chrono;
        // SER timestamps are nanoseconds since 2001-01-01
        static const auto serEpoch = sys_days{January / 1 / 2001};
        return SERTimestamp{static_cast<uint64_t>(
            duration_cast<std::chrono::nanoseconds>(tp - serEpoch).count())};
    }

    static SERTimestamp now() {
        return fromTimePoint(std::chrono::system_clock::now());
    }
};

// SER file header structure
struct SERHeader {
    std::array<char, 14> fileID;  // "LUCAM-RECORDER"
    uint32_t luID;                // Usually 0
    uint32_t colorID;             // See SERColorID enum
    uint32_t littleEndian;        // 0 = big endian, 1 = little endian
    uint32_t imageWidth;
    uint32_t imageHeight;
    uint32_t pixelDepth;  // 8, 16, or 32 bits
    uint64_t frameCount;
    std::array<char, 40> observer;
    std::array<char, 40> instrument;
    std::array<char, 40> telescope;
    uint64_t dateTime;  // Datetime value

    SERHeader() {
        std::fill(fileID.begin(), fileID.end(), '\0');
        std::copy(SER_FILE_ID.begin(), SER_FILE_ID.end(), fileID.begin());
        luID = 0;
        colorID = static_cast<uint32_t>(SERColorID::Mono);
        littleEndian = 1;  // Almost always little endian
        imageWidth = 0;
        imageHeight = 0;
        pixelDepth = 8;
        frameCount = 0;
        std::fill(observer.begin(), observer.end(), '\0');
        std::fill(instrument.begin(), instrument.end(), '\0');
        std::fill(telescope.begin(), telescope.end(), '\0');
        dateTime = SERTimestamp::now().nanoseconds;
    }

    // Initialization with parameters
    SERHeader(uint32_t width, uint32_t height, uint32_t depth, SERColorID color)
        : SERHeader() {
        imageWidth = width;
        imageHeight = height;
        pixelDepth = depth;
        colorID = static_cast<uint32_t>(color);
    }

    // Helper methods
    bool isValid() const {
        // Check file ID
        std::string fileIdStr(fileID.begin(), fileID.end());
        if (fileIdStr.substr(0, SER_FILE_ID.size()) != SER_FILE_ID) {
            return false;
        }

        // Check other fields for validity
        if (imageWidth == 0 || imageHeight == 0) {
            return false;
        }

        if (pixelDepth != 8 && pixelDepth != 16 && pixelDepth != 32) {
            return false;
        }

        return true;
    }

    std::string getObserverString() const {
        return std::string(observer.begin(),
                           std::find(observer.begin(), observer.end(), '\0'));
    }

    std::string getInstrumentString() const {
        return std::string(
            instrument.begin(),
            std::find(instrument.begin(), instrument.end(), '\0'));
    }

    std::string getTelescopeString() const {
        return std::string(telescope.begin(),
                           std::find(telescope.begin(), telescope.end(), '\0'));
    }

    void setObserverString(const std::string& obs) {
        std::fill(observer.begin(), observer.end(), '\0');
        std::copy(obs.begin(),
                  obs.begin() + std::min(obs.size(), observer.size() - 1),
                  observer.begin());
    }

    void setInstrumentString(const std::string& inst) {
        std::fill(instrument.begin(), instrument.end(), '\0');
        std::copy(inst.begin(),
                  inst.begin() + std::min(inst.size(), instrument.size() - 1),
                  instrument.begin());
    }

    void setTelescopeString(const std::string& tel) {
        std::fill(telescope.begin(), telescope.end(), '\0');
        std::copy(tel.begin(),
                  tel.begin() + std::min(tel.size(), telescope.size() - 1),
                  telescope.begin());
    }

    SERColorID getColorIDEnum() const {
        return static_cast<SERColorID>(colorID);
    }

    bool isColor() const {
        return colorID == static_cast<uint32_t>(SERColorID::RGB) ||
               colorID == static_cast<uint32_t>(SERColorID::BGR) ||
               (colorID >= static_cast<uint32_t>(SERColorID::BayerRGGB) &&
                colorID <= static_cast<uint32_t>(SERColorID::BayerBGGR));
    }

    bool isBayerPattern() const {
        return colorID >= static_cast<uint32_t>(SERColorID::BayerRGGB) &&
               colorID <= static_cast<uint32_t>(SERColorID::BayerBGGR);
    }

    // Byte size of each pixel
    uint32_t getBytesPerPixel() const { return pixelDepth / 8; }

    // Calculate frame size in bytes
    size_t getFrameSize() const {
        size_t bytesPerPixel = pixelDepth / 8;
        size_t pixelsPerFrame = imageWidth * imageHeight;
        size_t channelsPerPixel = 1;

        if (colorID == static_cast<uint32_t>(SERColorID::RGB) ||
            colorID == static_cast<uint32_t>(SERColorID::BGR)) {
            channelsPerPixel = 3;
        }

        return pixelsPerFrame * bytesPerPixel * channelsPerPixel;
    }

    // Set the current date and time
    void setCurrentDateTime() { dateTime = SERTimestamp::now().nanoseconds; }

    // Get timestamp as time_point
    std::chrono::system_clock::time_point getDateTime() const {
        return SERTimestamp(dateTime).toTimePoint();
    }
};

}  // namespace serastro
