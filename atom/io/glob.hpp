#pragma once

#include <cassert>
#include <filesystem>
#include <functional>
#include <iterator>
#include <regex>

#include "atom/containers/high_performance.hpp"
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

/**
 * @namespace atom::io
 * @brief Input/Output utilities for the Atom framework
 */
namespace atom::io {

using atom::containers::Map;
using atom::containers::String;
using atom::containers::Vector;

namespace fs = std::filesystem;

/**
 * @brief Replace the first occurrence of a substring in a string
 * @param str The string to modify
 * @param from The substring to find
 * @param toStr The replacement substring
 * @return true if replacement was made, false otherwise
 */
ATOM_INLINE auto stringReplace(String &str, const String &from,
                               const String &toStr) -> bool {
    String::size_type startPos = str.find(from);
    if (startPos == String::npos) {
        return false;
    }
    str.replace(startPos, from.length(), toStr);
    return true;
}

/**
 * @brief Translate a shell-style pattern to a regular expression
 * @param pattern The shell pattern to translate (e.g., "*.txt", "file?.py")
 * @return The equivalent regular expression pattern
 * @details Converts glob patterns to regex:
 *          - * becomes .*
 *          - ? becomes .
 *          - [abc] becomes character class
 *          - Special characters are escaped
 */
ATOM_INLINE auto translate(const String &pattern) -> String {
    String::size_type index = 0;
    String::size_type patternSize = pattern.size();
    String resultString;

    while (index < patternSize) {
        auto currentChar = pattern[index];
        index += 1;

        if (currentChar == '*') {
            resultString.append(".*");
        } else if (currentChar == '?') {
            resultString.append(".");
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
                resultString.append("\\[");
            } else {
                String stuff(pattern.begin() + index,
                             pattern.begin() + innerIndex);

                if (stuff.find("--") != String::npos) {
                    stringReplace(stuff, String{"\\"}, String{R"(\\)"});
                } else {
                    Vector<String> chunks;
                    String::size_type chunkIndex = 0;
                    if (pattern[index] == '!') {
                        chunkIndex = index + 2;
                    } else {
                        chunkIndex = index + 1;
                    }

                    while (true) {
                        chunkIndex = pattern.find("-", chunkIndex);
                        if (chunkIndex == String::npos ||
                            chunkIndex >= innerIndex) {
                            break;
                        }
                        chunks.emplace_back(pattern.begin() + index,
                                            pattern.begin() + chunkIndex);
                        index = chunkIndex + 1;
                        chunkIndex = index;
                    }

                    chunks.emplace_back(pattern.begin() + index,
                                        pattern.begin() + innerIndex);
                    bool first = true;
                    String tempStuff;
                    for (auto &chunk : chunks) {
                        stringReplace(chunk, String{"\\"}, String{R"(\\)"});
                        stringReplace(chunk, String{"-"}, String{R"(\-)"});
                        if (first) {
                            tempStuff.append(chunk);
                            first = false;
                        } else {
                            tempStuff.append("-").append(chunk);
                        }
                    }
                    stuff = std::move(tempStuff);
                }

                std::string std_stuff = stuff.c_str();
                std::string result_std;
                std::regex_replace(std::back_inserter(result_std),
                                   std_stuff.begin(), std_stuff.end(),
                                   std::regex(std::string{R"([&~|])"}),
                                   std::string{R"(\\\1)"});
                stuff = result_std.c_str();
                index = innerIndex + 1;

                if (!stuff.empty() && stuff[0] == '!') {
                    stuff = "^" + String(stuff.begin() + 1, stuff.end());
                } else if (!stuff.empty() &&
                           (stuff[0] == '^' || stuff[0] == '[')) {
                    stuff = String("\\\\") + stuff;
                }
                resultString.append("[").append(stuff).append("]");
            }
        } else {
            static const String specialCharacters =
                "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
            static Map<int, String> specialCharactersMap;

            if (specialCharactersMap.empty()) {
                for (auto &specialChar : specialCharacters) {
                    specialCharactersMap.insert(
                        std::make_pair(static_cast<int>(specialChar),
                                       String{"\\"} + String(1, specialChar)));
                }
            }

            if (specialCharacters.find(currentChar) != String::npos) {
                resultString.append(
                    specialCharactersMap[static_cast<int>(currentChar)]);
            } else {
                resultString.append(1, currentChar);
            }
        }
    }
    return String{"(("} + resultString + String{R"()|[\r\n])$)"};
}

/**
 * @brief Compile a pattern string into a regular expression
 * @param pattern The pattern string to compile
 * @return A compiled std::regex object
 */
ATOM_INLINE auto compilePattern(const String &pattern) -> std::regex {
    return std::regex(pattern.c_str(), std::regex::ECMAScript);
}

/**
 * @brief Test whether a filename matches a shell-style pattern
 * @param name The filesystem path to test
 * @param pattern The shell pattern to match against
 * @return true if the name matches the pattern, false otherwise
 */
ATOM_INLINE auto fnmatch(const fs::path &name, const String &pattern) -> bool {
    return std::regex_match(name.string(), compilePattern(pattern));
}

/**
 * @brief Filter a list of paths by a shell-style pattern
 * @param names Vector of filesystem paths to filter
 * @param pattern The shell pattern to match against
 * @return Vector of paths that match the pattern
 */
ATOM_INLINE auto filter(const Vector<fs::path> &names, const String &pattern)
    -> Vector<fs::path> {
    Vector<fs::path> result;
    for (const auto &name : names) {
        if (fnmatch(name, pattern)) {
            result.push_back(name);
        }
    }
    return result;
}

/**
 * @brief Expand tilde (~) in a filesystem path to the user's home directory
 * @param path The path that may contain a tilde
 * @return The expanded path with tilde replaced by home directory
 * @throws std::invalid_argument if HOME environment variable is not set
 */
ATOM_INLINE auto expandTilde(fs::path path) -> fs::path {
    if (path.empty()) {
        return path;
    }

#ifdef _WIN32
    const char *homeVariable = "USERNAME";
#else
    const char *homeVariable = "USER";
#endif
    String home;

#ifdef _WIN32
    size_t len = 0;
    char *homeCStr = nullptr;
    _dupenv_s(&homeCStr, &len, homeVariable);
    if (homeCStr) {
        home = homeCStr;
        free(homeCStr);
    }
#else
    const char *homeCStr = getenv(homeVariable);
    if (homeCStr) {
        home = homeCStr;
    }
#endif

    if (home.empty()) {
        THROW_INVALID_ARGUMENT(
            "error: Unable to expand `~` - HOME environment variable not set.");
    }

    String pathStr = path.string().c_str();
    if (!pathStr.empty() && pathStr[0] == '~') {
        pathStr = home + pathStr.substr(1);
        return fs::path(pathStr.c_str());
    }
    return path;
}

/**
 * @brief Check if a pathname contains glob magic characters
 * @param pathname The path string to check
 * @return true if the pathname contains *, ?, or [ characters
 */
ATOM_INLINE auto hasMagic(const String &pathname) -> bool {
    static const auto MAGIC_CHECK = std::regex("([*?[])");
    return std::regex_search(pathname.c_str(), MAGIC_CHECK);
}

/**
 * @brief Check if a pathname represents a hidden file or directory
 * @param pathname The path string to check
 * @return true if the pathname is hidden (starts with dot)
 */
ATOM_INLINE auto isHidden(const String &pathname) -> bool {
    return std::regex_match(pathname.c_str(),
                            std::regex(R"(^(.*\/)*\.[^\.\/]+\/*$)"));
}

/**
 * @brief Check if a pattern is a recursive glob pattern (**)
 * @param pattern The pattern to check
 * @return true if the pattern is "**"
 */
ATOM_INLINE auto isRecursive(const String &pattern) -> bool {
    return pattern == "**";
}

/**
 * @brief Iterate through entries in a directory
 * @param dirname The directory to iterate
 * @param dironly If true, only return directories
 * @return Vector of filesystem paths found in the directory
 */
ATOM_INLINE auto iterDirectory(const fs::path &dirname, bool dironly)
    -> Vector<fs::path> {
    Vector<fs::path> result;
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
            // Directory iteration failed, return empty result
        }
    }
    return result;
}

/**
 * @brief Recursively list all entries in a directory tree
 * @param dirname The root directory to start from
 * @param dironly If true, only return directories
 * @return Vector of all filesystem paths found recursively
 */
ATOM_INLINE auto rlistdir(const fs::path &dirname, bool dironly)
    -> Vector<fs::path> {
    Vector<fs::path> result;
    auto names = iterDirectory(dirname, dironly);

    for (auto &name : names) {
        if (!isHidden(name.string().c_str())) {
            result.push_back(name);
            auto subNames = rlistdir(name, dironly);
            result.insert(result.end(), subNames.begin(), subNames.end());
        }
    }
    return result;
}

/**
 * @brief Handle recursive glob patterns (**)
 * @param dirname The directory to search in
 * @param pattern The glob pattern (should be "**")
 * @param dironly If true, only return directories
 * @return Vector of all matching paths found recursively
 */
ATOM_INLINE auto glob2(const fs::path &dirname,
                       [[maybe_unused]] const String &pattern, bool dironly)
    -> Vector<fs::path> {
    Vector<fs::path> result;
    assert(isRecursive(pattern));

    for (auto &dir : rlistdir(dirname, dironly)) {
        result.push_back(dir);
    }
    return result;
}

/**
 * @brief Handle single-level glob patterns
 * @param dirname The directory to search in
 * @param pattern The glob pattern
 * @param dironly If true, only return directories
 * @return Vector of matching paths in the directory
 */
ATOM_INLINE auto glob1(const fs::path &dirname, const String &pattern,
                       bool dironly) -> Vector<fs::path> {
    auto names = iterDirectory(dirname, dironly);
    Vector<fs::path> filteredNames;

    for (auto &name : names) {
        if (!isHidden(name.string().c_str())) {
            filteredNames.push_back(name.filename());
        }
    }
    return filter(filteredNames, pattern);
}

/**
 * @brief Handle literal (non-glob) patterns
 * @param dirname The directory containing the file
 * @param basename The filename to check for existence
 * @param dironly Unused parameter for consistency
 * @return Vector containing the path if it exists, empty otherwise
 */
ATOM_INLINE auto glob0(const fs::path &dirname, const fs::path &basename,
                       bool /*dironly*/) -> Vector<fs::path> {
    Vector<fs::path> result;

    if (basename.empty()) {
        if (fs::is_directory(dirname)) {
            if (fs::exists(dirname)) {
                result.push_back(basename);
            }
        }
    } else {
        if (fs::exists(dirname / basename)) {
            result = {basename};
        }
    }
    return result;
}

/**
 * @brief Main glob function - find all paths matching a shell-style pattern
 * @param pathname The pattern to match (may contain *, ?, [], etc.)
 * @param recursive If true, enable recursive matching with **
 * @param dironly If true, only return directories
 * @return Vector of filesystem paths that match the pattern
 * @details This is the core glob implementation that handles:
 *          - Tilde expansion (~)
 *          - Magic character detection
 *          - Recursive and non-recursive pattern matching
 *          - Directory and file filtering
 */
ATOM_INLINE auto glob(const String &pathname, bool recursive = false,
                      bool dironly = false) -> Vector<fs::path> {
    Vector<fs::path> result;
    auto path = fs::path(pathname.c_str());

    if (!pathname.empty() && pathname[0] == '~') {
        path = expandTilde(path);
    }

    auto dirname = path.parent_path();
    const auto BASENAME_PATH = path.filename();
    const String BASENAME = BASENAME_PATH.string().c_str();

    if (!hasMagic(pathname)) {
        assert(!dironly);
        if (!BASENAME_PATH.empty()) {
            if (fs::exists(path)) {
                result.push_back(path);
            }
        } else {
            if (fs::is_directory(dirname)) {
                result.push_back(path);
            }
        }
        return result;
    }

    if (dirname.empty()) {
        if (recursive && isRecursive(BASENAME)) {
            return glob2(dirname, BASENAME, dironly);
        }
        return glob1(dirname, BASENAME, dironly);
    }

    Vector<fs::path> dirs;
    if (dirname != path && hasMagic(dirname.string().c_str())) {
        dirs = glob(dirname.string().c_str(), recursive, true);
    } else {
        dirs = {dirname};
    }

    std::function<Vector<fs::path>(const fs::path &, const String &, bool)>
        globInDir;

    if (hasMagic(BASENAME)) {
        if (recursive && isRecursive(BASENAME)) {
            globInDir = glob2;
        } else {
            globInDir = glob1;
        }
    } else {
        auto glob0Wrapper = [](const fs::path &dir, const String &baseStr,
                               bool dironly_flag) -> Vector<fs::path> {
            return glob0(dir, fs::path(baseStr.c_str()), dironly_flag);
        };
        globInDir = glob0Wrapper;
    }

    for (auto &dir : dirs) {
        for (auto &name : globInDir(dir, BASENAME, dironly)) {
            fs::path subresult = name;
            if (name.parent_path().empty()) {
                subresult = dir / name;
            }
            result.push_back(subresult);
        }
    }
    return result;
}

/**
 * @brief Find all paths matching a shell-style pattern (non-recursive)
 * @param pathname The pattern to match
 * @return Vector of filesystem paths that match the pattern
 */
static ATOM_INLINE auto glob(const String &pathname) -> Vector<fs::path> {
    return glob(pathname, false);
}

/**
 * @brief Find all paths matching a shell-style pattern (recursive)
 * @param pathname The pattern to match
 * @return Vector of filesystem paths that match the pattern recursively
 */
static ATOM_INLINE auto rglob(const String &pathname) -> Vector<fs::path> {
    return glob(pathname, true);
}

/**
 * @brief Find all paths matching multiple shell-style patterns (non-recursive)
 * @param pathnames Vector of patterns to match
 * @return Vector of filesystem paths that match any of the patterns
 */
static ATOM_INLINE auto glob(const Vector<String> &pathnames)
    -> Vector<fs::path> {
    Vector<fs::path> result;
    for (const auto &pathname : pathnames) {
        for (auto &match : glob(pathname, false)) {
            result.push_back(std::move(match));
        }
    }
    return result;
}

/**
 * @brief Find all paths matching multiple shell-style patterns (recursive)
 * @param pathnames Vector of patterns to match
 * @return Vector of filesystem paths that match any of the patterns recursively
 */
static ATOM_INLINE auto rglob(const Vector<String> &pathnames)
    -> Vector<fs::path> {
    Vector<fs::path> result;
    for (const auto &pathname : pathnames) {
        for (auto &match : glob(pathname, true)) {
            result.push_back(std::move(match));
        }
    }
    return result;
}

/**
 * @brief Find all paths matching patterns from initializer list (non-recursive)
 * @param pathnames Initializer list of patterns to match
 * @return Vector of filesystem paths that match any of the patterns
 */
static ATOM_INLINE auto glob(const std::initializer_list<String> &pathnames)
    -> Vector<fs::path> {
    return glob(Vector<String>(pathnames));
}

/**
 * @brief Find all paths matching patterns from initializer list (recursive)
 * @param pathnames Initializer list of patterns to match
 * @return Vector of filesystem paths that match any of the patterns recursively
 */
static ATOM_INLINE auto rglob(const std::initializer_list<String> &pathnames)
    -> Vector<fs::path> {
    return rglob(Vector<String>(pathnames));
}

}  // namespace atom::io
