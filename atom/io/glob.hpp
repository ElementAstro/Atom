#pragma once
#include <cassert>
#include <filesystem>
#include <functional>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include "atom/error/exception.hpp"

#include "macro.hpp"

namespace atom::io {

namespace fs = std::filesystem;

namespace {
ATOM_INLINE auto stringReplace(std::string &str, const std::string &from,
                               const std::string &toStr) -> bool {
    std::size_t startPos = str.find(from);
    if (startPos == std::string::npos) {
        return false;
    }
    str.replace(startPos, from.length(), toStr);
    return true;
}

ATOM_INLINE auto translate(const std::string &pattern) -> std::string {
    std::size_t index = 0;
    std::size_t patternSize = pattern.size();
    std::string resultString;

    while (index < patternSize) {
        auto currentChar = pattern[index];
        index += 1;
        if (currentChar == '*') {
            resultString += ".*";
        } else if (currentChar == '?') {
            resultString += ".";
        } else if (currentChar == '[') {
            auto innerIndex = index;
            if (innerIndex < patternSize && pattern[innerIndex] == '!') {
                innerIndex += 1;
            }
            if (innerIndex < patternSize && pattern[innerIndex] == ']') {
                innerIndex += 1;
            }
            while (innerIndex < patternSize && pattern[innerIndex] != ']') {
                innerIndex += 1;
            }
            if (innerIndex >= patternSize) {
                resultString += "\\[";
            } else {
                auto stuff = std::string(pattern.begin() + index,
                                         pattern.begin() + innerIndex);
#if USE_ABSL
                if (!absl::StrContains(stuff, "--")) {
#else
                if (stuff.find("--") == std::string::npos) {
#endif
                    stringReplace(stuff, std::string{"\\"},
                                  std::string{R"(\\)"});
                } else {
                    std::vector<std::string> chunks;
                    std::size_t chunkIndex = 0;
                    if (pattern[index] == '!') {
                        chunkIndex = index + 2;
                    } else {
                        chunkIndex = index + 1;
                    }

                    while (true) {
                        chunkIndex = pattern.find("-", chunkIndex, innerIndex);
                        if (chunkIndex == std::string::npos) {
                            break;
                        }
                        chunks.emplace_back(pattern.begin() + index,
                                            pattern.begin() + chunkIndex);
                        index = chunkIndex + 1;
                        chunkIndex = chunkIndex + 3;
                    }

                    chunks.emplace_back(pattern.begin() + index,
                                        pattern.begin() + innerIndex);
                    // Escape backslashes and hyphens for set difference (--).
                    // Hyphens that create ranges shouldn't be escaped.
                    bool first = false;
                    for (auto &chunk : chunks) {
                        stringReplace(chunk, std::string{"\\"},
                                      std::string{R"(\\)"});
                        stringReplace(chunk, std::string{"-"},
                                      std::string{R"(\-)"});
                        if (first) {
                            stuff += chunk;
                            first = false;
                        } else {
                            stuff += "-" + chunk;
                        }
                    }
                }

                // Escape set operations (&&, ~~ and ||).
                std::string result;
                std::regex_replace(
                    std::back_inserter(result),           // result
                    stuff.begin(), stuff.end(),           // string
                    std::regex(std::string{R"([&~|])"}),  // pattern
                    std::string{R"(\\\1)"});              // repl
                stuff = result;
                index = innerIndex + 1;
                if (stuff[0] == '!') {
                    stuff = "^" + std::string(stuff.begin() + 1, stuff.end());
                } else if (stuff[0] == '^' || stuff[0] == '[') {
                    stuff = "\\\\" + stuff;
                }
                resultString += "[" + stuff + "]";
            }
        } else {
            // SPECIAL_CHARS
            // closing ')', '}' and ']'
            // '-' (a range in character set)
            // '&', '~', (extended character set operations)
            // '#' (comment) and WHITESPACE (ignored) in verbose mode
            static std::string specialCharacters =
                "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
            static std::map<int, std::string> specialCharactersMap;
            if (specialCharactersMap.empty()) {
                for (auto &specialChar : specialCharacters) {
                    specialCharactersMap.insert(std::make_pair(
                        static_cast<int>(specialChar),
                        std::string{"\\"} + std::string(1, specialChar)));
                }
            }

#if USE_ABSL
            if (absl::StrContains(specialCharacters, currentChar)) {
#else
            if (specialCharacters.find(currentChar) != std::string::npos) {
#endif
                resultString +=
                    specialCharactersMap[static_cast<int>(currentChar)];
            } else {
                resultString += currentChar;
            }
        }
    }
    return std::string{"(("} + resultString + std::string{R"()|[\r\n])$)"};
}

ATOM_INLINE auto compilePattern(const std::string &pattern) -> std::regex {
    return std::regex(translate(pattern), std::regex::ECMAScript);
}

ATOM_INLINE auto fnmatch(const fs::path &name,
                         const std::string &pattern) -> bool {
    return std::regex_match(name.string(), compilePattern(pattern));
}

ATOM_INLINE auto filter(const std::vector<fs::path> &names,
                        const std::string &pattern) -> std::vector<fs::path> {
    // std::cout << "Pattern: " << pattern << "\n";
    std::vector<fs::path> result;
    for (const auto &name : names) {
        // std::cout << "Checking for " << name.string() << "\n";
        if (fnmatch(name, pattern)) {
            result.push_back(name);
        }
    }
    return result;
}

ATOM_INLINE auto expandTilde(fs::path path) -> fs::path {
    if (path.empty()) {
        return path;
    }

#ifdef _WIN32
    const char *homeVariable = "USERNAME";
#else
    const char *homeVariable = "USER";
#endif
    char *home = nullptr;
    size_t len = 0;
    _dupenv_s(&home, &len, homeVariable);
    if (home == nullptr) {
        THROW_INVALID_ARGUMENT(
            "error: Unable to expand `~` - HOME environment variable not set.");
    }

    std::string pathStr = path.string();
    if (pathStr[0] == '~') {
        pathStr = std::string(home) + pathStr.substr(1, pathStr.size() - 1);
        free(home);
        return fs::path(pathStr);
    } else {
        free(home);
        return path;
    }
}

ATOM_INLINE auto hasMagic(const std::string &pathname) -> bool {
    static const auto MAGIC_CHECK = std::regex("([*?[])");
    return std::regex_search(pathname, MAGIC_CHECK);
}

ATOM_INLINE auto isHidden(const std::string &pathname) -> bool {
    return std::regex_match(pathname, std::regex(R"(^(.*\/)*\.[^\.\/]+\/*$)"));
}

ATOM_INLINE auto isRecursive(const std::string &pattern) -> bool {
    return pattern == "**";
}

ATOM_INLINE auto iterDirectory(const fs::path &dirname,
                               bool dironly) -> std::vector<fs::path> {
    std::vector<fs::path> result;

    auto currentDirectory = dirname;
    if (currentDirectory.empty()) {
        currentDirectory = fs::current_path();
    }

    if (fs::exists(currentDirectory)) {
        try {
            for (const auto &entry : fs::directory_iterator(
                     currentDirectory,
                     fs::directory_options::follow_directory_symlink |
                         fs::directory_options::skip_permission_denied)) {
                if (!dironly || entry.is_directory()) {
                    if (dirname.is_absolute()) {
                        result.push_back(entry.path());
                    } else {
                        result.push_back(fs::relative(entry.path()));
                    }
                }
            }
        } catch (std::exception &) {
            // not a directory
            // do nothing
        }
    }

    return result;
}

// Recursively yields relative pathnames inside a literal directory.
ATOM_INLINE auto rlistdir(const fs::path &dirname,
                          bool dironly) -> std::vector<fs::path> {
    std::vector<fs::path> result;
    auto names = iterDirectory(dirname, dironly);
    for (auto &name : names) {
        if (!isHidden(name.string())) {
            result.push_back(name);
            for (auto &subName : rlistdir(name, dironly)) {
                result.push_back(subName);
            }
        }
    }
    return result;
}

// This helper function recursively yields relative pathnames inside a literal
// directory.
ATOM_INLINE auto glob2(const fs::path &dirname,
                       [[maybe_unused]] const std::string &pattern,
                       bool dironly) -> std::vector<fs::path> {
    // std::cout << "In glob2\n";
    std::vector<fs::path> result;
    assert(isRecursive(pattern));
    for (auto &dir : rlistdir(dirname, dironly)) {
        result.push_back(dir);
    }
    return result;
}

// These 2 helper functions non-recursively glob inside a literal directory.
// They return a list of basenames.  _glob1 accepts a pattern while _glob0
// takes a literal basename (so it only has to check for its existence).
ATOM_INLINE auto glob1(const fs::path &dirname, const std::string &pattern,
                       bool dironly) -> std::vector<fs::path> {
    // std::cout << "In glob1\n";
    auto names = iterDirectory(dirname, dironly);
    std::vector<fs::path> filteredNames;
    for (auto &name : names) {
        if (!isHidden(name.string())) {
            filteredNames.push_back(name.filename());
        }
    }
    return filter(filteredNames, pattern);
}

ATOM_INLINE auto glob0(const fs::path &dirname, const fs::path &basename,
                       bool /*dironly*/) -> std::vector<fs::path> {
    // std::cout << "In glob0\n";
    std::vector<fs::path> result;
    if (basename.empty()) {
        // 'q*x/' should match only directories.
        if (fs::is_directory(dirname)) {
            result = {basename};
        }
    } else {
        if (fs::exists(dirname / basename)) {
            result = {basename};
        }
    }
    return result;
}

ATOM_INLINE auto glob(const std::string &pathname, bool recursive = false,
                      bool dironly = false) -> std::vector<fs::path> {
    std::vector<fs::path> result;

    auto path = fs::path(pathname);

    if (pathname[0] == '~') {
        // expand tilde
        path = expandTilde(path);
    }

    auto dirname = path.parent_path();
    const auto BASENAME = path.filename();

    if (!hasMagic(pathname)) {
        assert(!dironly);
        if (!BASENAME.empty()) {
            if (fs::exists(path)) {
                result.push_back(path);
            }
        } else {
            // Patterns ending with a slash should match only directories
            if (fs::is_directory(dirname)) {
                result.push_back(path);
            }
        }
        return result;
    }

    if (dirname.empty()) {
        if (recursive && isRecursive(BASENAME.string())) {
            return glob2(dirname, BASENAME.string(), dironly);
        }
        return glob1(dirname, BASENAME.string(), dironly);
    }

    std::vector<fs::path> dirs;
    if (dirname != fs::path(pathname) && hasMagic(dirname.string())) {
        dirs = glob(dirname.string(), recursive, true);
    } else {
        dirs = {dirname};
    }

    std::function<std::vector<fs::path>(const fs::path &, const std::string &,
                                        bool)>
        globInDir;
    if (hasMagic(BASENAME.string())) {
        if (recursive && isRecursive(BASENAME.string())) {
            globInDir = glob2;
        } else {
            globInDir = glob1;
        }
    } else {
        globInDir = glob0;
    }

    for (auto &dir : dirs) {
        for (auto &name : globInDir(dir, BASENAME.string(), dironly)) {
            fs::path subresult = name;
            if (name.parent_path().empty()) {
                subresult = dir / name;
            }
            result.push_back(subresult);
        }
    }

    return result;
}

}  // namespace

static ATOM_INLINE auto glob(const std::string &pathname)
    -> std::vector<fs::path> {
    return glob(pathname, false);
}

static ATOM_INLINE auto rglob(const std::string &pathname)
    -> std::vector<fs::path> {
    return glob(pathname, true);
}

static ATOM_INLINE auto glob(const std::vector<std::string> &pathnames)
    -> std::vector<fs::path> {
    std::vector<fs::path> result;
    for (const auto &pathname : pathnames) {
        for (auto &match : glob(pathname, false)) {
            result.push_back(std::move(match));
        }
    }
    return result;
}

static ATOM_INLINE auto rglob(const std::vector<std::string> &pathnames)
    -> std::vector<fs::path> {
    std::vector<fs::path> result;
    for (const auto &pathname : pathnames) {
        for (auto &match : glob(pathname, true)) {
            result.push_back(std::move(match));
        }
    }
    return result;
}

static ATOM_INLINE auto glob(const std::initializer_list<std::string>
                                 &pathnames) -> std::vector<fs::path> {
    return glob(std::vector<std::string>(pathnames));
}

static ATOM_INLINE auto rglob(const std::initializer_list<std::string>
                                  &pathnames) -> std::vector<fs::path> {
    return rglob(std::vector<std::string>(pathnames));
}

}  // namespace atom::io