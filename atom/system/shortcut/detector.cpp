#include "detector.hpp"
#include "detector_impl.h"
#include <spdlog/spdlog.h>

namespace shortcut_detector {

ShortcutDetector::ShortcutDetector() : pImpl(std::make_unique<ShortcutDetectorImpl>()) {
    spdlog::debug("ShortcutDetector initialized");
}

ShortcutDetector::~ShortcutDetector() = default;

ShortcutCheckResult ShortcutDetector::isShortcutCaptured(const Shortcut& shortcut) {
    return pImpl->isShortcutCaptured(shortcut);
}

bool ShortcutDetector::hasKeyboardHookInstalled() {
    return pImpl->hasKeyboardHookInstalled();
}

std::vector<std::string> ShortcutDetector::getProcessesWithKeyboardHooks() {
    return pImpl->getProcessesWithKeyboardHooks();
}

}  // namespace shortcut_detector
