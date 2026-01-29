#ifndef ATOM_IMAGE_METADATA_XMP_TYPES_HPP
#define ATOM_IMAGE_METADATA_XMP_TYPES_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace atom::image::metadata {

/**
 * @brief XMP namespace prefixes
 */
namespace XmpNamespace {
constexpr const char* DC = "http://purl.org/dc/elements/1.1/";
constexpr const char* XMP = "http://ns.adobe.com/xap/1.0/";
constexpr const char* XMP_RIGHTS = "http://ns.adobe.com/xap/1.0/rights/";
constexpr const char* XMP_MM = "http://ns.adobe.com/xap/1.0/mm/";
constexpr const char* XMP_BJ = "http://ns.adobe.com/xap/1.0/bj/";
constexpr const char* XMP_TPG = "http://ns.adobe.com/xap/1.0/t/pg/";
constexpr const char* PDF = "http://ns.adobe.com/pdf/1.3/";
constexpr const char* PHOTOSHOP = "http://ns.adobe.com/photoshop/1.0/";
constexpr const char* TIFF = "http://ns.adobe.com/tiff/1.0/";
constexpr const char* EXIF = "http://ns.adobe.com/exif/1.0/";
constexpr const char* EXIF_AUX = "http://ns.adobe.com/exif/1.0/aux/";
constexpr const char* IPTC_CORE = "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/";
constexpr const char* IPTC_EXT = "http://iptc.org/std/Iptc4xmpExt/2008-02-29/";
constexpr const char* LR = "http://ns.adobe.com/lightroom/1.0/";
constexpr const char* CRS = "http://ns.adobe.com/camera-raw-settings/1.0/";
constexpr const char* RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
}  // namespace XmpNamespace

/**
 * @brief XMP value types
 */
enum class XmpValueType {
    SIMPLE,  ///< Simple string value
    STRUCT,  ///< Structure (nested properties)
    ALT,     ///< Alternative array (language alternatives)
    BAG,     ///< Unordered array
    SEQ      ///< Ordered array (sequence)
};

/**
 * @brief Language alternative entry
 */
struct XmpLangAlt {
    std::string language;  ///< Language code (e.g., "en-US", "x-default")
    std::string value;

    bool operator==(const XmpLangAlt& other) const {
        return language == other.language && value == other.value;
    }
};

/**
 * @brief XMP property value
 */
using XmpValue =
    std::variant<std::string,               ///< Simple value
                 std::vector<std::string>,  ///< Bag or Seq
                 std::vector<XmpLangAlt>,   ///< Alt (language alternatives)
                 std::unordered_map<std::string, std::string>  ///< Struct
                 >;

/**
 * @brief XMP property
 */
struct XmpProperty {
    std::string namespaceUri;
    std::string prefix;
    std::string name;
    XmpValueType type = XmpValueType::SIMPLE;
    XmpValue value;
    std::unordered_map<std::string, std::string> qualifiers;

    /**
     * @brief Get simple string value
     */
    [[nodiscard]] std::optional<std::string> asString() const {
        if (auto* str = std::get_if<std::string>(&value)) {
            return *str;
        }
        return std::nullopt;
    }

    /**
     * @brief Get array values
     */
    [[nodiscard]] std::vector<std::string> asArray() const {
        if (auto* arr = std::get_if<std::vector<std::string>>(&value)) {
            return *arr;
        }
        return {};
    }

    /**
     * @brief Get language alternatives
     */
    [[nodiscard]] std::vector<XmpLangAlt> asLangAlt() const {
        if (auto* alt = std::get_if<std::vector<XmpLangAlt>>(&value)) {
            return *alt;
        }
        return {};
    }

    /**
     * @brief Get default language value
     */
    [[nodiscard]] std::optional<std::string> getDefaultLangValue() const {
        if (auto* alt = std::get_if<std::vector<XmpLangAlt>>(&value)) {
            for (const auto& item : *alt) {
                if (item.language == "x-default") {
                    return item.value;
                }
            }
            if (!alt->empty()) {
                return alt->front().value;
            }
        }
        return std::nullopt;
    }
};

/**
 * @brief Dublin Core metadata (dc:)
 */
struct DublinCore {
    std::vector<std::string> contributor;
    std::optional<std::string> coverage;
    std::vector<std::string> creator;
    std::optional<std::chrono::system_clock::time_point> date;
    std::vector<XmpLangAlt> description;
    std::optional<std::string> format;
    std::optional<std::string> identifier;
    std::optional<std::string> language;
    std::vector<std::string> publisher;
    std::vector<std::string> relation;
    std::vector<XmpLangAlt> rights;
    std::optional<std::string> source;
    std::vector<std::string> subject;  ///< Keywords
    std::vector<XmpLangAlt> title;
    std::optional<std::string> type;

    /**
     * @brief Get default title
     */
    [[nodiscard]] std::optional<std::string> getTitle() const {
        for (const auto& t : title) {
            if (t.language == "x-default")
                return t.value;
        }
        return title.empty() ? std::nullopt
                             : std::make_optional(title.front().value);
    }

    /**
     * @brief Get default description
     */
    [[nodiscard]] std::optional<std::string> getDescription() const {
        for (const auto& d : description) {
            if (d.language == "x-default")
                return d.value;
        }
        return description.empty()
                   ? std::nullopt
                   : std::make_optional(description.front().value);
    }
};

/**
 * @brief XMP Basic metadata (xmp:)
 */
struct XmpBasic {
    std::optional<std::chrono::system_clock::time_point> createDate;
    std::optional<std::string> creatorTool;
    std::vector<std::string> identifier;
    std::optional<std::string> label;
    std::optional<std::chrono::system_clock::time_point> metadataDate;
    std::optional<std::chrono::system_clock::time_point> modifyDate;
    std::optional<int> rating;  ///< 0-5 stars, or -1 for rejected
    std::optional<std::string> baseUrl;
    std::optional<std::string> nickname;
    std::vector<std::string> thumbnails;
};

/**
 * @brief XMP Rights metadata (xmpRights:)
 */
struct XmpRights {
    std::optional<std::string> certificate;
    std::optional<bool> marked;
    std::vector<std::string> owner;
    std::vector<XmpLangAlt> usageTerms;
    std::optional<std::string> webStatement;
};

/**
 * @brief XMP Media Management (xmpMM:)
 */
struct XmpMediaManagement {
    std::optional<std::string> documentId;
    std::optional<std::string> instanceId;
    std::optional<std::string> originalDocumentId;
    std::optional<std::string> renditionClass;
    std::optional<std::string> renditionParams;
    std::optional<std::string> versionId;
    std::vector<std::string> history;
    std::vector<std::string> derivedFrom;
    std::vector<std::string> ingredients;
    std::optional<std::string> manageTo;
    std::optional<std::string> manageUI;
    std::optional<std::string> manager;
    std::optional<std::string> managerVariant;
};

/**
 * @brief Photoshop namespace metadata (photoshop:)
 */
struct PhotoshopMetadata {
    std::optional<std::string> authorsPosition;
    std::optional<std::string> captionWriter;
    std::optional<std::string> category;
    std::optional<std::string> city;
    std::optional<std::string> colorMode;
    std::optional<std::string> country;
    std::optional<std::string> credit;
    std::optional<std::chrono::system_clock::time_point> dateCreated;
    std::optional<std::string> headline;
    std::optional<std::string> instructions;
    std::optional<std::string> source;
    std::optional<std::string> state;
    std::vector<std::string> supplementalCategories;
    std::optional<std::string> transmissionReference;
    std::optional<int> urgency;
    std::optional<std::string> iccProfile;
};

/**
 * @brief Camera Raw Settings (crs:)
 */
struct CameraRawSettings {
    std::optional<double> exposure;
    std::optional<int> contrast;
    std::optional<int> highlights;
    std::optional<int> shadows;
    std::optional<int> whites;
    std::optional<int> blacks;
    std::optional<int> clarity;
    std::optional<int> vibrance;
    std::optional<int> saturation;
    std::optional<int> temperature;
    std::optional<int> tint;
    std::optional<double> sharpness;
    std::optional<int> luminanceNoiseReduction;
    std::optional<int> colorNoiseReduction;
    std::optional<std::string> whiteBalance;
    std::optional<bool> autoExposure;
    std::optional<bool> autoContrast;
    std::optional<bool> autoShadows;
    std::optional<std::string> cameraProfile;
    std::optional<std::string> processVersion;
};

/**
 * @brief Complete XMP data structure
 */
struct XmpData {
    // Standard namespaces
    DublinCore dc;
    XmpBasic xmpBasic;
    XmpRights xmpRights;
    XmpMediaManagement xmpMM;
    PhotoshopMetadata photoshop;
    CameraRawSettings cameraRaw;

    // Raw properties for full preservation
    std::vector<XmpProperty> properties;

    // Custom/unknown namespaces
    std::unordered_map<std::string, std::vector<XmpProperty>> customNamespaces;

    // Raw XMP packet
    std::optional<std::string> rawPacket;
    std::optional<bool> hasXmpPacketWrapper;

    /**
     * @brief Check if XMP data has any meaningful content
     */
    [[nodiscard]] bool hasData() const noexcept {
        return !properties.empty() || dc.getTitle().has_value() ||
               dc.getDescription().has_value() || !dc.subject.empty() ||
               xmpBasic.rating.has_value();
    }

    /**
     * @brief Get a property by namespace and name
     */
    [[nodiscard]] std::optional<XmpProperty> getProperty(
        const std::string& namespaceUri, const std::string& name) const {
        for (const auto& prop : properties) {
            if (prop.namespaceUri == namespaceUri && prop.name == name) {
                return prop;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Set a simple property value
     */
    void setProperty(const std::string& namespaceUri, const std::string& prefix,
                     const std::string& name, const std::string& value) {
        // Check if property exists
        for (auto& prop : properties) {
            if (prop.namespaceUri == namespaceUri && prop.name == name) {
                prop.value = value;
                return;
            }
        }
        // Add new property
        XmpProperty prop;
        prop.namespaceUri = namespaceUri;
        prop.prefix = prefix;
        prop.name = name;
        prop.type = XmpValueType::SIMPLE;
        prop.value = value;
        properties.push_back(prop);
    }

    /**
     * @brief Remove a property
     */
    bool removeProperty(const std::string& namespaceUri,
                        const std::string& name) {
        auto it = std::remove_if(
            properties.begin(), properties.end(), [&](const XmpProperty& p) {
                return p.namespaceUri == namespaceUri && p.name == name;
            });
        if (it != properties.end()) {
            properties.erase(it, properties.end());
            return true;
        }
        return false;
    }

    /**
     * @brief Clear all XMP data
     */
    void clear() noexcept {
        dc = DublinCore{};
        xmpBasic = XmpBasic{};
        xmpRights = XmpRights{};
        xmpMM = XmpMediaManagement{};
        photoshop = PhotoshopMetadata{};
        cameraRaw = CameraRawSettings{};
        properties.clear();
        customNamespaces.clear();
        rawPacket.reset();
        hasXmpPacketWrapper.reset();
    }
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_XMP_TYPES_HPP
