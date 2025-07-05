#ifndef ATOM_EXTRA_INICPP_EVENT_LISTENER_HPP
#define ATOM_EXTRA_INICPP_EVENT_LISTENER_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "common.hpp"
#include "section.hpp"

#if INICPP_CONFIG_EVENT_LISTENERS

namespace inicpp {

/**
 * @brief File level event types
 */
enum class FileEventType {
    SECTION_ADDED,     ///< A new section was added
    SECTION_MODIFIED,  ///< An existing section was modified
    SECTION_REMOVED,   ///< A section was removed
    FILE_LOADED,       ///< A file was loaded
    FILE_SAVED,        ///< A file was saved
    FILE_CLEARED       ///< A file was cleared
};

/**
 * @brief File level event data
 */
struct FileEventData {
    std::string fileName;     ///< File name
    std::string sectionName;  ///< Section name (if applicable)
    FileEventType eventType;  ///< Event type
};

/**
 * @brief Path changed event data
 */
struct PathChangedEventData {
    std::string path;      ///< Full path that changed
    std::string oldValue;  ///< Old value (if applicable)
    std::string newValue;  ///< New value (if applicable)
    bool isNew;            ///< Whether it is a new path
    bool isRemoved;        ///< Whether it is a removed path
};

/**
 * @brief File event listener
 */
using FileEventListener = std::function<void(const FileEventData&)>;

/**
 * @brief Path changed event listener
 */
using PathChangedListener = std::function<void(const PathChangedEventData&)>;

/**
 * @class EventManager
 * @brief Class to manage INI configuration events
 */
class EventManager {
private:
    std::vector<FileEventListener> fileListeners_;
    std::unordered_map<std::string, std::vector<PathChangedListener>>
        pathListeners_;
    bool enabled_ = true;

public:
    /**
     * @brief Default constructor
     */
    EventManager() = default;

    /**
     * @brief Add a file event listener
     * @param listener The listener function to add
     * @return Listener ID, can be used to remove
     */
    size_t addFileListener(FileEventListener listener) {
        fileListeners_.push_back(std::move(listener));
        return fileListeners_.size() - 1;
    }

    /**
     * @brief Add a path changed listener
     * @param path The path to listen to (e.g. "section.subsection.field")
     * @param listener The listener function to add
     * @return Listener ID, can be used to remove
     */
    size_t addPathListener(const std::string& path,
                           PathChangedListener listener) {
        auto& listeners = pathListeners_[path];
        listeners.push_back(std::move(listener));
        return listeners.size() - 1;
    }

    /**
     * @brief Remove a file listener
     * @param id Listener ID
     * @return Whether the removal was successful
     */
    bool removeFileListener(size_t id) {
        if (id < fileListeners_.size()) {
            fileListeners_[id] =
                nullptr;  // Do not change vector structure, just set to null
            return true;
        }
        return false;
    }

    /**
     * @brief Remove a path listener
     * @param path Path
     * @param id Listener ID
     * @return Whether the removal was successful
     */
    bool removePathListener(const std::string& path, size_t id) {
        auto it = pathListeners_.find(path);
        if (it != pathListeners_.end() && id < it->second.size()) {
            it->second[id] =
                nullptr;  // Do not change vector structure, just set to null
            return true;
        }
        return false;
    }

    /**
     * @brief Clear all listeners
     */
    void clearAllListeners() {
        fileListeners_.clear();
        pathListeners_.clear();
    }

    /**
     * @brief Enable or disable event notification
     * @param enabled Whether to enable
     */
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    /**
     * @brief Check if event notification is enabled
     * @return True if enabled
     */
    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }

    /**
     * @brief Notify a file event
     * @param eventData Event data
     */
    void notifyFileEvent(const FileEventData& eventData) const {
        if (!enabled_)
            return;

        for (const auto& listener : fileListeners_) {
            if (listener) {
                listener(eventData);
            }
        }
    }

    /**
     * @brief Notify a path changed event
     * @param eventData Event data
     */
    void notifyPathChanged(const PathChangedEventData& eventData) const {
        if (!enabled_)
            return;

        // First check for exact match listeners
        auto it = pathListeners_.find(eventData.path);
        if (it != pathListeners_.end()) {
            for (const auto& listener : it->second) {
                if (listener) {
                    listener(eventData);
                }
            }
        }

        // Check for wildcard listeners (using "*")
        it = pathListeners_.find("*");
        if (it != pathListeners_.end()) {
            for (const auto& listener : it->second) {
                if (listener) {
                    listener(eventData);
                }
            }
        }

        // Handle partial path listeners (e.g., "section.*" will match
        // "section.field1", "section.field2", etc.)
        for (const auto& [pattern, listeners] : pathListeners_) {
            // Skip already handled exact matches
            if (pattern == eventData.path || pattern == "*") {
                continue;
            }

            // Check for wildcard matches
            if (pattern.find('*') != std::string::npos) {
                bool matches = matchesWildcardPattern(eventData.path, pattern);
                if (matches) {
                    for (const auto& listener : listeners) {
                        if (listener) {
                            listener(eventData);
                        }
                    }
                }
            }
        }
    }

private:
    /**
     * @brief Check if a path matches a wildcard pattern
     * @param path The path to check
     * @param pattern The pattern containing wildcards
     * @return True if it matches
     */
    static bool matchesWildcardPattern(const std::string& path,
                                       const std::string& pattern) {
        // Split the path and pattern into parts
        auto pathParts = splitPath(path);
        auto patternParts = splitPath(pattern);

        // If the pattern is longer than the path, it cannot match
        if (patternParts.size() > pathParts.size()) {
            return false;
        }

        // Check each part
        for (size_t i = 0; i < patternParts.size(); ++i) {
            const auto& patternPart = patternParts[i];

            // Handle wildcards
            if (patternPart == "*") {
                continue;  // Matches anything
            }

            // Check if there are parts with wildcards (e.g. "field*")
            if (patternPart.find('*') != std::string::npos) {
                // Simple implementation: only check for prefix match
                // More complex wildcard matching can be implemented if needed
                size_t starPos = patternPart.find('*');
                if (starPos > 0) {
                    if (pathParts[i].compare(0, starPos, patternPart, 0,
                                             starPos) != 0) {
                        return false;
                    }
                }
            } else if (patternPart != pathParts[i]) {
                return false;  // Does not match
            }
        }

        return true;
    }
};

}  // namespace inicpp

#endif  // INICPP_CONFIG_EVENT_LISTENERS

#endif  // ATOM_EXTRA_INICPP_EVENT_LISTENER_HPP
