#ifndef ATOM_IMAGE_METADATA_IPTC_TYPES_HPP
#define ATOM_IMAGE_METADATA_IPTC_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace atom::image::metadata {

/**
 * @brief IPTC record numbers
 */
enum class IptcRecord : uint8_t {
    ENVELOPE = 1,
    APPLICATION = 2,
    NEWSPHOTO = 3,
    PREOBJECTDATA = 7,
    OBJECTDATA = 8,
    POSTOBJECTDATA = 9
};

/**
 * @brief IPTC Application Record (2:xxx) dataset tags
 */
enum class IptcTag : uint8_t {
    // Object Data
    RECORD_VERSION = 0,
    OBJECT_TYPE = 3,
    OBJECT_ATTRIBUTE = 4,
    OBJECT_NAME = 5,  ///< Title
    EDIT_STATUS = 7,
    EDITORIAL_UPDATE = 8,
    URGENCY = 10,
    SUBJECT_REFERENCE = 12,
    CATEGORY = 15,
    SUPPLEMENTAL_CATEGORY = 20,
    FIXTURE_ID = 22,
    KEYWORDS = 25,
    CONTENT_LOCATION_CODE = 26,
    CONTENT_LOCATION_NAME = 27,
    RELEASE_DATE = 30,
    RELEASE_TIME = 35,
    EXPIRATION_DATE = 37,
    EXPIRATION_TIME = 38,
    SPECIAL_INSTRUCTIONS = 40,
    ACTION_ADVISED = 42,
    REFERENCE_SERVICE = 45,
    REFERENCE_DATE = 47,
    REFERENCE_NUMBER = 50,
    DATE_CREATED = 55,
    TIME_CREATED = 60,
    DIGITAL_DATE_CREATED = 62,
    DIGITAL_TIME_CREATED = 63,
    ORIGINATING_PROGRAM = 65,
    PROGRAM_VERSION = 70,
    OBJECT_CYCLE = 75,
    BYLINE = 80,  ///< Author/Creator
    BYLINE_TITLE = 85,
    CITY = 90,
    SUB_LOCATION = 92,
    STATE = 95,
    COUNTRY_CODE = 100,
    COUNTRY = 101,
    ORIGINAL_TRANSMISSION_REF = 103,
    HEADLINE = 105,
    CREDIT = 110,
    SOURCE = 115,
    COPYRIGHT_NOTICE = 116,
    CONTACT = 118,
    CAPTION = 120,  ///< Description
    WRITER_EDITOR = 122,
    RASTERIZED_CAPTION = 125,
    IMAGE_TYPE = 130,
    IMAGE_ORIENTATION = 131,
    LANGUAGE_ID = 135,
    AUDIO_TYPE = 150,
    AUDIO_SAMPLING_RATE = 151,
    AUDIO_SAMPLING_RES = 152,
    AUDIO_DURATION = 153,
    AUDIO_OUTCUE = 154,
    PREVIEW_FORMAT = 200,
    PREVIEW_VERSION = 201,
    PREVIEW_DATA = 202
};

/**
 * @brief IPTC urgency levels
 */
enum class IptcUrgency : uint8_t {
    HIGHEST = 1,
    HIGH = 2,
    ABOVE_NORMAL = 3,
    NORMAL = 4,
    BELOW_NORMAL = 5,
    LOW = 6,
    LOWER = 7,
    LOWEST = 8,
    USER_DEFINED = 9
};

/**
 * @brief IPTC image orientation
 */
enum class IptcImageOrientation : char {
    PORTRAIT = 'P',
    LANDSCAPE = 'L',
    SQUARE = 'S'
};

/**
 * @brief IPTC dataset entry
 */
struct IptcDataset {
    IptcRecord record = IptcRecord::APPLICATION;
    uint8_t tag = 0;
    std::vector<uint8_t> data;

    /**
     * @brief Get data as string (for ASCII datasets)
     */
    [[nodiscard]] std::string asString() const {
        return std::string(data.begin(), data.end());
    }

    /**
     * @brief Get data as integer (for numeric datasets)
     */
    [[nodiscard]] int asInt() const {
        if (data.empty())
            return 0;
        if (data.size() == 1)
            return data[0];
        if (data.size() == 2) {
            return (static_cast<int>(data[0]) << 8) | data[1];
        }
        return 0;
    }
};

/**
 * @brief IPTC contact information
 */
struct IptcContact {
    std::optional<std::string> name;
    std::optional<std::string> phone;
    std::optional<std::string> email;
    std::optional<std::string> website;
    std::optional<std::string> address;
    std::optional<std::string> city;
    std::optional<std::string> state;
    std::optional<std::string> postalCode;
    std::optional<std::string> country;
};

/**
 * @brief IPTC location information
 */
struct IptcLocation {
    std::optional<std::string> subLocation;
    std::optional<std::string> city;
    std::optional<std::string> state;
    std::optional<std::string> country;
    std::optional<std::string> countryCode;
    std::optional<std::string> worldRegion;
};

/**
 * @brief Complete IPTC data structure
 */
struct IptcData {
    // Record version
    std::optional<uint16_t> recordVersion;

    // Object/Content information
    std::optional<std::string> objectName;  ///< Title
    std::optional<std::string> headline;
    std::optional<std::string> caption;  ///< Description/Abstract
    std::vector<std::string> keywords;
    std::optional<std::string> category;
    std::vector<std::string> supplementalCategories;
    std::optional<IptcUrgency> urgency;
    std::optional<std::string> specialInstructions;

    // Creator/Author information
    std::optional<std::string> byline;        ///< Creator/Author
    std::optional<std::string> bylineTitle;   ///< Creator's job title
    std::optional<std::string> writerEditor;  ///< Caption writer
    std::optional<std::string> credit;
    std::optional<std::string> source;
    std::optional<std::string> copyrightNotice;
    IptcContact contact;

    // Location information
    IptcLocation location;
    IptcLocation contentLocation;  ///< Location shown in image

    // Date/Time information
    std::optional<std::chrono::system_clock::time_point> dateCreated;
    std::optional<std::chrono::system_clock::time_point> digitalDateCreated;
    std::optional<std::chrono::system_clock::time_point> releaseDate;
    std::optional<std::chrono::system_clock::time_point> expirationDate;

    // Technical information
    std::optional<std::string> originatingProgram;
    std::optional<std::string> programVersion;
    std::optional<IptcImageOrientation> imageOrientation;
    std::optional<std::string> imageType;
    std::optional<std::string> languageId;

    // Reference information
    std::optional<std::string> originalTransmissionRef;
    std::optional<std::string> fixtureId;
    std::optional<std::string> editStatus;

    // Subject reference codes (IPTC Subject NewsCodes)
    std::vector<std::string> subjectReferences;

    // Preview/Thumbnail
    std::optional<std::vector<uint8_t>> previewData;
    std::optional<uint16_t> previewFormat;

    // Raw datasets for preservation
    std::vector<IptcDataset> rawDatasets;

    /**
     * @brief Check if IPTC data has any meaningful content
     */
    [[nodiscard]] bool hasData() const noexcept {
        return objectName.has_value() || headline.has_value() ||
               caption.has_value() || !keywords.empty() || byline.has_value() ||
               copyrightNotice.has_value();
    }

    /**
     * @brief Add a keyword
     */
    void addKeyword(const std::string& keyword) {
        if (std::find(keywords.begin(), keywords.end(), keyword) ==
            keywords.end()) {
            keywords.push_back(keyword);
        }
    }

    /**
     * @brief Remove a keyword
     */
    bool removeKeyword(const std::string& keyword) {
        auto it = std::find(keywords.begin(), keywords.end(), keyword);
        if (it != keywords.end()) {
            keywords.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Clear all IPTC data
     */
    void clear() noexcept {
        recordVersion.reset();
        objectName.reset();
        headline.reset();
        caption.reset();
        keywords.clear();
        category.reset();
        supplementalCategories.clear();
        urgency.reset();
        specialInstructions.reset();
        byline.reset();
        bylineTitle.reset();
        writerEditor.reset();
        credit.reset();
        source.reset();
        copyrightNotice.reset();
        contact = IptcContact{};
        location = IptcLocation{};
        contentLocation = IptcLocation{};
        dateCreated.reset();
        digitalDateCreated.reset();
        releaseDate.reset();
        expirationDate.reset();
        originatingProgram.reset();
        programVersion.reset();
        imageOrientation.reset();
        imageType.reset();
        languageId.reset();
        originalTransmissionRef.reset();
        fixtureId.reset();
        editStatus.reset();
        subjectReferences.clear();
        previewData.reset();
        previewFormat.reset();
        rawDatasets.clear();
    }
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_IPTC_TYPES_HPP
