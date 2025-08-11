#ifndef ATOM_EXTRA_INICPP_INISECTION_HPP
#define ATOM_EXTRA_INICPP_INISECTION_HPP

#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "common.hpp"
#include "field.hpp"

namespace inicpp {

inline auto splitPath(const std::string& path) -> std::vector<std::string> {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = path.find('.');

    while (end != std::string::npos) {
        parts.push_back(path.substr(start, end - start));
        start = end + 1;
        end = path.find('.', start);
    }
    parts.push_back(path.substr(start));

    return parts;
}

#if INICPP_CONFIG_EVENT_LISTENERS
/**
 * @brief Event types for section events.
 */
enum class SectionEventType {
    FIELD_ADDED,     ///< A new field was added.
    FIELD_MODIFIED,  ///< An existing field was modified.
    FIELD_REMOVED,   ///< A field was removed.
    SECTION_CLEARED  ///< The section was cleared.
};

/**
 * @brief Event data for section events.
 */
struct SectionEventData {
    std::string sectionName;        ///< Name of the section.
    std::string fieldName;          ///< Name of the field (if applicable).
    std::string oldValue;           ///< Old value (if applicable).
    std::string newValue;           ///< New value (if applicable).
    SectionEventType eventType;     ///< Type of the event.
};

/**
 * @brief Signature for section event listeners.
 */
using SectionEventListener = std::function<void(const SectionEventData&)>;
#endif

/**
 * @brief Base class for INI file sections with customizable string comparison.
 * @tparam Comparator The comparator type for field names.
 */
template <typename Comparator>
class IniSectionBase : public map_type<std::string, IniField, Comparator> {
private:
#if INICPP_CONFIG_EVENT_LISTENERS
    std::vector<SectionEventListener> eventListeners_;
    std::string sectionName_;
#endif

#if INICPP_CONFIG_NESTED_SECTIONS
    std::string parentSectionName_;
    std::unordered_set<std::string> childSections_;
#endif

    /**
     * @brief Notify event listeners about section events.
     * @param eventData The event data to send to listeners.
     */
#if INICPP_CONFIG_EVENT_LISTENERS
    void notifyListeners(const SectionEventData& eventData) const {
        for (const auto& listener : eventListeners_) {
            listener(eventData);
        }
    }
#endif

public:
    /**
     * @brief Default constructor.
     */
    IniSectionBase() = default;

    /**
     * @brief Constructor with section name.
     * @param name The name of the section.
     */
    explicit IniSectionBase(const std::string& name)
#if INICPP_CONFIG_EVENT_LISTENERS
        : sectionName_(name)
#endif
#if INICPP_CONFIG_NESTED_SECTIONS && INICPP_CONFIG_EVENT_LISTENERS
        , parentSectionName_("")
#elif INICPP_CONFIG_NESTED_SECTIONS
        : parentSectionName_("")
#endif
    {}

    /**
     * @brief Destructor.
     */
    ~IniSectionBase() = default;

#if INICPP_CONFIG_EVENT_LISTENERS
    /**
     * @brief Set the section name.
     * @param name The name of the section.
     */
    void setSectionName(const std::string& name) {
        sectionName_ = name;
    }

    /**
     * @brief Get the section name.
     * @return The name of the section.
     */
    [[nodiscard]] const std::string& getSectionName() const noexcept {
        return sectionName_;
    }

    /**
     * @brief Add an event listener.
     * @param listener The event listener to add.
     */
    void addEventListener(const SectionEventListener& listener) {
        eventListeners_.push_back(listener);
    }

    /**
     * @brief Remove all event listeners.
     */
    void clearEventListeners() noexcept {
        eventListeners_.clear();
    }
#endif

#if INICPP_CONFIG_NESTED_SECTIONS
    /**
     * @brief Set the parent section name.
     * @param parentName The parent section name.
     */
    void setParentSectionName(const std::string& parentName) {
        parentSectionName_ = parentName;
    }

    /**
     * @brief Get the parent section name.
     * @return The parent section name, or empty string if this is a top-level section.
     */
    [[nodiscard]] const std::string& getParentSectionName() const noexcept {
        return parentSectionName_;
    }

    /**
     * @brief Add a child section.
     * @param childName The child section name.
     */
    void addChildSection(const std::string& childName) {
        childSections_.insert(childName);
    }

    /**
     * @brief Remove a child section.
     * @param childName The child section name.
     * @return True if the child section was removed, false if it wasn't found.
     */
    bool removeChildSection(const std::string& childName) {
        return childSections_.erase(childName) > 0;
    }

    /**
     * @brief Check if this section has child sections.
     * @return True if this section has child sections, false otherwise.
     */
    [[nodiscard]] bool hasChildSections() const noexcept {
        return !childSections_.empty();
    }

    /**
     * @brief Get the child section names.
     * @return A vector containing the names of all child sections.
     */
    [[nodiscard]] std::vector<std::string> getChildSectionNames() const {
        return std::vector<std::string>(childSections_.begin(), childSections_.end());
    }

    /**
     * @brief Check if this is a top-level section.
     * @return True if this is a top-level section, false otherwise.
     */
    [[nodiscard]] bool isTopLevel() const noexcept {
        return parentSectionName_.empty();
    }
#endif

    /**
     * @brief Get a field value as the specified type.
     * @tparam T The target type.
     * @param key The field key.
     * @return The converted field value.
     * @throws std::out_of_range if the key doesn't exist.
     * @throws std::invalid_argument if the conversion fails.
     */
    template <typename T>
    [[nodiscard]] T get(const std::string& key) const {
        try {
            auto it = this->find(key);
            if (it == this->end()) {
                throw std::out_of_range("Field '" + key +
                                        "' not found in section");
            }
            return it->second.template as<T>();
        } catch (const std::out_of_range& ex) {
            throw;
        } catch (const std::exception& ex) {
            throw std::invalid_argument("Failed to get field '" + key +
                                        "': " + ex.what());
        }
    }

    /**
     * @brief Get a field value as the specified type with a default value.
     * @tparam T The target type.
     * @param key The field key.
     * @param defaultValue The default value to return if the key doesn't exist
     * or conversion fails.
     * @return The converted field value or the default value.
     */
    template <typename T>
    [[nodiscard]] T get(const std::string& key,
                        const T& defaultValue) const noexcept {
        try {
            auto it = this->find(key);
            if (it == this->end()) {
                return defaultValue;
            }
            return it->second.template as<T>();
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Get a field value as the specified type with validation.
     * @tparam T The target type.
     * @param key The field key.
     * @return An optional containing the converted value, or nullopt if the key
     * doesn't exist or conversion fails.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> get_optional(
        const std::string& key) const noexcept {
        try {
            auto it = this->find(key);
            if (it == this->end()) {
                return std::nullopt;
            }
            return it->second.template as_optional<T>();
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Set a field value.
     * @tparam T The type of the value.
     * @param key The field key.
     * @param value The value to set.
     */
    template <typename T>
    void set(const std::string& key, const T& value) {
        try {
            // 检查字段是否已存在
            auto it = this->find(key);
            bool fieldExists = (it != this->end());

            // 如果启用了事件监听，准备事件数据
#if INICPP_CONFIG_EVENT_LISTENERS
            std::string oldValue;
            if (fieldExists) {
                oldValue = it->second.template as<std::string>();
            }
#endif

            // 设置或更新字段值
            (*this)[key] = value;

            // 如果启用了事件监听，触发事件
#if INICPP_CONFIG_EVENT_LISTENERS
            // 准备事件数据
            SectionEventData eventData;
            eventData.sectionName = sectionName_;
            eventData.fieldName = key;
            eventData.newValue = (*this)[key].template as<std::string>();

            if (fieldExists) {
                eventData.oldValue = oldValue;
                eventData.eventType = SectionEventType::FIELD_MODIFIED;
            } else {
                eventData.eventType = SectionEventType::FIELD_ADDED;
            }

            // 通知监听器
            notifyListeners(eventData);
#endif

        } catch (const std::exception& ex) {
            throw std::invalid_argument("Failed to set field '" + key +
                                        "': " + ex.what());
        }
    }

    /**
     * @brief Delete a field.
     * @param key The field key.
     * @return True if the field was deleted, false if it didn't exist.
     */
    bool deleteField(const std::string& key) {
        auto it = this->find(key);
        if (it == this->end()) {
            return false;
        }

#if INICPP_CONFIG_EVENT_LISTENERS
        // 准备事件数据
        SectionEventData eventData;
        eventData.sectionName = sectionName_;
        eventData.fieldName = key;
        eventData.oldValue = it->second.template as<std::string>();
        eventData.eventType = SectionEventType::FIELD_REMOVED;
#endif

        // 删除字段
        this->erase(it);

#if INICPP_CONFIG_EVENT_LISTENERS
        // 通知监听器
        notifyListeners(eventData);
#endif

        return true;
    }

    /**
     * @brief Check if a field with the specified key exists.
     * @param key The field key.
     * @return True if the field exists, false otherwise.
     */
    [[nodiscard]] bool hasField(const std::string& key) const noexcept {
        return this->find(key) != this->end();
    }

    /**
     * @brief Clear all fields from this section.
     */
    void clearFields() {
#if INICPP_CONFIG_EVENT_LISTENERS
        // 准备事件数据
        SectionEventData eventData;
        eventData.sectionName = sectionName_;
        eventData.eventType = SectionEventType::SECTION_CLEARED;
#endif

        // 清空所有字段
        this->clear();

#if INICPP_CONFIG_EVENT_LISTENERS
        // 通知监听器
        notifyListeners(eventData);
#endif
    }

#if INICPP_CONFIG_PATH_QUERY
    /**
     * @brief Get a field value using a path query.
     * @tparam T The target type.
     * @param path The path to the field in format "subsection.field".
     * @return The converted field value.
     * @throws std::out_of_range if the path is invalid.
     */
    template <typename T>
    [[nodiscard]] T getPath(const std::string& path) const {
        auto parts = splitPath(path);
        if (parts.size() != 1) {
            throw std::out_of_range("Path query cannot be processed in a section.");
        }
        return get<T>(parts[0]);
    }

    /**
     * @brief Set a field value using a path query.
     * @tparam T The type of the value.
     * @param path The path to the field in format "field".
     * @param value The value to set.
     * @throws std::out_of_range if the path is invalid.
     */
    template <typename T>
    void setPath(const std::string& path, const T& value) {
        auto parts = splitPath(path);
        if (parts.size() != 1) {
            throw std::out_of_range("Path query cannot be processed in a section.");
        }
        set<T>(parts[0], value);
    }
#endif
};

/**
 * @brief Case-sensitive INI section.
 */
using IniSection = IniSectionBase<std::less<std::string>>;

/**
 * @brief Case-insensitive INI section.
 */
using IniSectionCaseInsensitive = IniSectionBase<StringInsensitiveLess>;

#if INICPP_HAS_BOOST && INICPP_CONFIG_USE_BOOST_CONTAINERS
/**
 * @brief Hash-based INI section for faster lookups with large data sets.
 */
using IniSectionHash = hash_map_type<std::string, IniField>;

/**
 * @brief Case-insensitive hash-based INI section.
 */
using IniSectionHashCaseInsensitive = hash_map_type<std::string, IniField, StringInsensitiveHash>;
#endif

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_INISECTION_HPP
