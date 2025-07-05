/*
 * history.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "history.hpp"

#include <algorithm>
#include <list>
#include <memory>
#include <mutex>

#include <spdlog/spdlog.h>

namespace atom::system {

class CommandHistory::Impl {
public:
    explicit Impl(size_t maxSize) : _maxSize(maxSize) {}

    void addCommand(const std::string &command, int exitStatus) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_history.size() >= _maxSize) {
            _history.pop_front();
        }

        _history.emplace_back(command, exitStatus);
    }

    auto getLastCommands(size_t count) const
        -> std::vector<std::pair<std::string, int>> {
        std::lock_guard<std::mutex> lock(_mutex);

        count = std::min(count, _history.size());
        std::vector<std::pair<std::string, int>> result;
        result.reserve(count);

        auto it = _history.rbegin();
        for (size_t i = 0; i < count; ++i, ++it) {
            result.push_back(*it);
        }

        return result;
    }

    auto searchCommands(const std::string &substring) const
        -> std::vector<std::pair<std::string, int>> {
        std::lock_guard<std::mutex> lock(_mutex);

        std::vector<std::pair<std::string, int>> result;

        for (const auto &entry : _history) {
            if (entry.first.find(substring) != std::string::npos) {
                result.push_back(entry);
            }
        }

        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _history.clear();
    }

    auto size() const -> size_t {
        std::lock_guard<std::mutex> lock(_mutex);
        return _history.size();
    }

private:
    mutable std::mutex _mutex;
    std::list<std::pair<std::string, int>> _history;
    size_t _maxSize;
};

CommandHistory::CommandHistory(size_t maxSize)
    : pImpl(std::make_unique<Impl>(maxSize)) {}

CommandHistory::~CommandHistory() = default;

void CommandHistory::addCommand(const std::string &command, int exitStatus) {
    pImpl->addCommand(command, exitStatus);
}

auto CommandHistory::getLastCommands(size_t count) const
    -> std::vector<std::pair<std::string, int>> {
    return pImpl->getLastCommands(count);
}

auto CommandHistory::searchCommands(const std::string &substring) const
    -> std::vector<std::pair<std::string, int>> {
    return pImpl->searchCommands(substring);
}

void CommandHistory::clear() { pImpl->clear(); }

auto CommandHistory::size() const -> size_t { return pImpl->size(); }

auto createCommandHistory(size_t maxHistorySize)
    -> std::unique_ptr<CommandHistory> {
    spdlog::debug("Creating command history with max size: {}", maxHistorySize);
    return std::make_unique<CommandHistory>(maxHistorySize);
}

}  // namespace atom::system
