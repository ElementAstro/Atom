#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <chrono>
#include <vector>
#include <memory>

namespace atom::image::core {

/**
 * @brief Type alias for metadata values
 */
using MetadataValue = std::variant<
    std::string,
    int,
    double,
    bool,
    std::chrono::system_clock::time_point,
    std::vector<std::string>,
    std::vector<int>,
    std::vector<double>
>;

/**
 * @brief Image metadata container
 */
class ImageMetadata {
public:
    /**
     * @brief Default constructor
     */
    ImageMetadata() = default;

    /**
     * @brief Copy constructor
     */
    ImageMetadata(const ImageMetadata&) = default;

    /**
     * @brief Move constructor
     */
    ImageMetadata(ImageMetadata&&) = default;

    /**
     * @brief Copy assignment
     */
    ImageMetadata& operator=(const ImageMetadata&) = default;

    /**
     * @brief Move assignment
     */
    ImageMetadata& operator=(ImageMetadata&&) = default;

    /**
     * @brief Destructor
     */
    ~ImageMetadata() = default;

    /**
     * @brief Set a metadata value
     * @tparam T Value type
     * @param key Metadata key
     * @param value Metadata value
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        metadata_[key] = value;
    }

    /**
     * @brief Get a metadata value
     * @tparam T Expected value type
     * @param key Metadata key
     * @param defaultValue Default value if key not found
     * @return Metadata value or default
     */
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const {
        auto it = metadata_.find(key);
        if (it != metadata_.end()) {
            if (auto value = std::get_if<T>(&it->second)) {
                return *value;
            }
        }
        return defaultValue;
    }

    /**
     * @brief Get a metadata value as string
     * @param key Metadata key
     * @param defaultValue Default value if key not found
     * @return Metadata value as string
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * @brief Get a metadata value as integer
     * @param key Metadata key
     * @param defaultValue Default value if key not found
     * @return Metadata value as integer
     */
    int getInt(const std::string& key, int defaultValue = 0) const;

    /**
     * @brief Get a metadata value as double
     * @param key Metadata key
     * @param defaultValue Default value if key not found
     * @return Metadata value as double
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

    /**
     * @brief Get a metadata value as boolean
     * @param key Metadata key
     * @param defaultValue Default value if key not found
     * @return Metadata value as boolean
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;

    /**
     * @brief Check if a metadata key exists
     * @param key Metadata key
     * @return True if key exists
     */
    bool has(const std::string& key) const {
        return metadata_.find(key) != metadata_.end();
    }

    /**
     * @brief Remove a metadata entry
     * @param key Metadata key
     * @return True if entry was removed
     */
    bool remove(const std::string& key) {
        return metadata_.erase(key) > 0;
    }

    /**
     * @brief Clear all metadata
     */
    void clear() {
        metadata_.clear();
    }

    /**
     * @brief Get all metadata keys
     * @return Vector of metadata keys
     */
    std::vector<std::string> getKeys() const;

    /**
     * @brief Get the number of metadata entries
     * @return Number of entries
     */
    size_t size() const {
        return metadata_.size();
    }

    /**
     * @brief Check if metadata is empty
     * @return True if empty
     */
    bool empty() const {
        return metadata_.empty();
    }

    /**
     * @brief Merge metadata from another object
     * @param other Other metadata object
     * @param overwrite Whether to overwrite existing keys
     */
    void merge(const ImageMetadata& other, bool overwrite = false);

    /**
     * @brief Convert metadata to string representation
     * @return String representation
     */
    std::string toString() const;

    /**
     * @brief Parse metadata from string representation
     * @param str String representation
     * @return True if parsing successful
     */
    bool fromString(const std::string& str);

private:
    std::unordered_map<std::string, MetadataValue> metadata_;

    /**
     * @brief Convert metadata value to string
     * @param value Metadata value
     * @return String representation
     */
    std::string valueToString(const MetadataValue& value) const;

    /**
     * @brief Convert string to metadata value
     * @param str String representation
     * @param typeHint Type hint for conversion
     * @return Metadata value
     */
    MetadataValue stringToValue(const std::string& str, const std::string& typeHint) const;
};

/**
 * @brief Standard metadata keys
 */
namespace MetadataKeys {
    // Basic image information
    constexpr const char* WIDTH = "width";
    constexpr const char* HEIGHT = "height";
    constexpr const char* CHANNELS = "channels";
    constexpr const char* BIT_DEPTH = "bit_depth";
    constexpr const char* COLOR_SPACE = "color_space";
    constexpr const char* FORMAT = "format";

    // Camera information
    constexpr const char* CAMERA_MAKE = "camera_make";
    constexpr const char* CAMERA_MODEL = "camera_model";
    constexpr const char* LENS_MODEL = "lens_model";
    constexpr const char* FOCAL_LENGTH = "focal_length";
    constexpr const char* APERTURE = "aperture";
    constexpr const char* ISO = "iso";
    constexpr const char* SHUTTER_SPEED = "shutter_speed";

    // Timestamps
    constexpr const char* DATE_TIME = "date_time";
    constexpr const char* DATE_TIME_ORIGINAL = "date_time_original";
    constexpr const char* DATE_TIME_DIGITIZED = "date_time_digitized";

    // GPS information
    constexpr const char* GPS_LATITUDE = "gps_latitude";
    constexpr const char* GPS_LONGITUDE = "gps_longitude";
    constexpr const char* GPS_ALTITUDE = "gps_altitude";

    // File information
    constexpr const char* FILE_NAME = "file_name";
    constexpr const char* FILE_SIZE = "file_size";
    constexpr const char* FILE_MODIFIED = "file_modified";

    // Processing information
    constexpr const char* SOFTWARE = "software";
    constexpr const char* PROCESSING = "processing";
    constexpr const char* COMPRESSION = "compression";
    constexpr const char* QUALITY = "quality";

    // Copyright information
    constexpr const char* COPYRIGHT = "copyright";
    constexpr const char* ARTIST = "artist";
    constexpr const char* CREATOR = "creator";
}

/**
 * @brief EXIF metadata handler
 */
class ExifMetadata {
public:
    /**
     * @brief Extract EXIF metadata from image data
     * @param data Image data
     * @param size Data size
     * @return Extracted metadata
     */
    static ImageMetadata extract(const uint8_t* data, size_t size);

    /**
     * @brief Check if data contains EXIF information
     * @param data Image data
     * @param size Data size
     * @return True if EXIF data found
     */
    static bool hasExif(const uint8_t* data, size_t size);

private:
    // Implementation details would go here
    // This is a placeholder for EXIF parsing functionality
};

} // namespace atom::image::core