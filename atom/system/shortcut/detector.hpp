#pragma once

#include <memory>
#include <string>
#include <vector>
#include "shortcut.h"
#include "status.h"

namespace shortcut_detector {

// Forward declaration of the implementation
class ShortcutDetectorImpl;

/**
 * @brief Main class for detecting if keyboard shortcuts are captured
 */
class ShortcutDetector {
public:
    /**
     * @brief Construct a new Shortcut Detector
     */
    ShortcutDetector();

    /**
     * @brief Destroy the Shortcut Detector
     */
    ~ShortcutDetector();

    // Prevent copy and move
    ShortcutDetector(const ShortcutDetector&) = delete;
    ShortcutDetector& operator=(const ShortcutDetector&) = delete;
    ShortcutDetector(ShortcutDetector&&) = delete;
    ShortcutDetector& operator=(ShortcutDetector&&) = delete;

    /**
     * @brief Check if a shortcut is captured by the system or another
     * application
     *
     * @param shortcut The shortcut to check
     * @return ShortcutCheckResult Result containing status and details
     */
    ShortcutCheckResult isShortcutCaptured(const Shortcut& shortcut);

    /**
     * @brief Check if a keyboard hook is currently installed
     *
     * @return true If a keyboard hook is detected
     * @return false If no keyboard hook is detected
     */
    bool hasKeyboardHookInstalled();

    /**
     * @brief Get a list of processes with keyboard hooks
     *
     * @return std::vector<std::string> Process names with keyboard hooks
     */
    std::vector<std::string> getProcessesWithKeyboardHooks();

private:
    std::unique_ptr<ShortcutDetectorImpl> pImpl;
};

}  // namespace shortcut_detector
