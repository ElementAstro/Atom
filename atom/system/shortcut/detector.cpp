#include "detector.hpp"
#include "detector_impl.h"
#include <spdlog/spdlog.h>

namespace shortcut_detector {

ShortcutDetector::ShortcutDetector() : pImpl(std::make_unique<ShortcutDetectorImpl>()) {
    spdlog::debug("ShortcutDetector initialized with default cache configuration");
}

ShortcutDetector::ShortcutDetector(const CacheConfig& config)
    : pImpl(std::make_unique<ShortcutDetectorImpl>(config)) {
    spdlog::debug("ShortcutDetector initialized with custom cache configuration");
}

ShortcutDetector::~ShortcutDetector() = default;

ShortcutCheckResult ShortcutDetector::isShortcutCaptured(const Shortcut& shortcut) {
    return pImpl->isShortcutCaptured(shortcut);
}

ShortcutCheckResult ShortcutDetector::isShortcutCaptured(const Shortcut& shortcut, bool bypassCache) {
    return pImpl->isShortcutCaptured(shortcut, bypassCache);
}

bool ShortcutDetector::hasKeyboardHookInstalled() {
    return pImpl->hasKeyboardHookInstalled();
}

std::vector<std::string> ShortcutDetector::getProcessesWithKeyboardHooks() {
    return pImpl->getProcessesWithKeyboardHooks();
}

void ShortcutDetector::clearCache() {
    pImpl->clearCache();
}

void ShortcutDetector::clearShortcutCache(const Shortcut& shortcut) {
    pImpl->clearShortcutCache(shortcut);
}

void ShortcutDetector::updateCacheConfig(const CacheConfig& config) {
    pImpl->updateCacheConfig(config);
}

const CacheConfig& ShortcutDetector::getCacheConfig() const {
    return pImpl->getCacheConfig();
}

PerformanceStats ShortcutDetector::getPerformanceStats() const {
    return pImpl->getPerformanceStats();
}

void ShortcutDetector::resetPerformanceStats() {
    pImpl->resetPerformanceStats();
}

size_t ShortcutDetector::getCacheSize() const {
    return pImpl->getCacheSize();
}

bool ShortcutDetector::isCacheEnabled() const {
    return pImpl->isCacheEnabled();
}

void ShortcutDetector::setCacheEnabled(bool enabled) {
    pImpl->setCacheEnabled(enabled);
}

}  // namespace shortcut_detector
