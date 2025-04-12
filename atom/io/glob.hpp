#pragma once

#include <cassert>
#include <filesystem>
#include <functional>
#include <iterator>
#include <regex>

#include "atom/containers/high_performance.hpp"
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::io {

// Use type aliases from high_performance.hpp
using atom::containers::Map;  // Use Map alias
using atom::containers::String;
using atom::containers::Vector;

namespace fs = std::filesystem;

ATOM_INLINE auto stringReplace(String &str, const String &from,
                               const String &toStr) -> bool {
    String::size_type startPos = str.find(from);  // Use String::size_type
    if (startPos == String::npos) {
        return false;
    }
    str.replace(startPos, from.length(), toStr);
    return true;
}

ATOM_INLINE auto translate(const String &pattern) -> String {
    String::size_type index = 0;                     // Use String::size_type
    String::size_type patternSize = pattern.size();  // Use String::size_type
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
                // Use String iterators or constructor
                String stuff(pattern.begin() + index,
                             pattern.begin() + innerIndex);

                // Replace contains with find() != npos for broader
                // compatibility
                if (stuff.find("--") != String::npos) {
                    stringReplace(stuff, String{"\\"}, String{R"(\\)"});
                } else {
                    Vector<String> chunks;             // Use Vector<String>
                    String::size_type chunkIndex = 0;  // Use String::size_type
                    if (pattern[index] == '!') {
                        chunkIndex = index + 2;
                    } else {
                        chunkIndex = index + 1;
                    }

                    while (true) {
                        // Use find with iterators or indices
                        chunkIndex = pattern.find("-", chunkIndex);
                        if (chunkIndex == String::npos ||
                            chunkIndex >= innerIndex) {  // Check bounds
                            break;
                        }
                        // Use String constructor with iterators
                        chunks.emplace_back(pattern.begin() + index,
                                            pattern.begin() + chunkIndex);
                        index = chunkIndex + 1;
                        // Adjust chunkIndex logic if needed based on find
                        // behavior This part seems complex and might need
                        // careful review depending on String impl. Assuming
                        // find works similarly to std::string::find chunkIndex
                        // = chunkIndex + 3; // This seems potentially wrong,
                        // maybe just chunkIndex + 1?
                        chunkIndex =
                            index;  // Start next search from the new index
                    }

                    // Use String constructor with iterators
                    chunks.emplace_back(pattern.begin() + index,
                                        pattern.begin() + innerIndex);
                    bool first = true;  // Corrected initialization
                    String tempStuff;   // Build the string incrementally
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
                    stuff = std::move(tempStuff);  // Assign the built string
                }

                // Regex operations might require std::string or const char*
                // Convert `stuff` if necessary. Assuming String is compatible
                // or convertible.
                std::string std_stuff =
                    stuff.c_str();  // Convert to std::string via c_str()
                std::string result_std;
                std::regex_replace(
                    std::back_inserter(result_std),       // result
                    std_stuff.begin(), std_stuff.end(),   // string
                    std::regex(std::string{R"([&~|])"}),  // pattern
                    std::string{R"(\\\1)"});              // repl
                stuff = result_std.c_str();  // Convert back to String
                index = innerIndex + 1;
                if (!stuff.empty() &&
                    stuff[0] == '!') {  // Check for empty string
                    stuff = "^" + String(stuff.begin() + 1, stuff.end());
                } else if (!stuff.empty() &&
                           (stuff[0] == '^' ||
                            stuff[0] == '[')) {  // Check for empty string
                    // Use String concatenation
                    stuff = String("\\\\") + stuff;
                }
                resultString.append("[").append(stuff).append("]");
            }
        } else {
            // Use String for specialCharacters
            static const String specialCharacters =
                "()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
            // Use Map<int, String> for specialCharactersMap
            static Map<int, String> specialCharactersMap;
            if (specialCharactersMap.empty()) {
                for (auto &specialChar : specialCharacters) {
                    specialCharactersMap.insert(
                        std::make_pair(static_cast<int>(specialChar),
                                       String{"\\"} + String(1, specialChar)));
                }
            }

            // Replace contains with find() != npos
            if (specialCharacters.find(currentChar) != String::npos) {
                resultString.append(
                    specialCharactersMap[static_cast<int>(currentChar)]);
            } else {
                resultString.append(1, currentChar);
            }
        }
    }
    // Use String concatenation
    return String{"(("} + resultString + String{R"()|[\r\n])$)"};
}

ATOM_INLINE auto compilePattern(const String &pattern) -> std::regex {
    // std::regex constructor usually takes std::string or const char*
    return std::regex(pattern.c_str(), std::regex::ECMAScript);
}

ATOM_INLINE auto fnmatch(const fs::path &name, const String &pattern) -> bool {
    // std::regex_match usually takes std::string or const char* for the subject
    return std::regex_match(name.string(), compilePattern(pattern));
}

ATOM_INLINE auto filter(const Vector<fs::path> &names,  // Use Vector
                        const String &pattern)
    -> Vector<fs::path> {     // Use Vector
    Vector<fs::path> result;  // Use Vector
    for (const auto &name : names) {
        // fnmatch expects String pattern, name.string() returns std::string
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
    String home;  // Use String
#ifdef _WIN32
    size_t len = 0;
    char *homeCStr = nullptr;
    _dupenv_s(&homeCStr, &len, homeVariable);
    if (homeCStr) {
        home = homeCStr;  // Assign C-string to String
        free(homeCStr);
    }
#else
    const char *homeCStr = getenv(homeVariable);
    if (homeCStr) {
        home = homeCStr;  // Assign C-string to String
    }
#endif
    if (home.empty()) {
        THROW_INVALID_ARGUMENT(
            "error: Unable to expand `~` - HOME environment variable not set.");
    }

    // fs::path.string() returns std::string, convert to String if needed for
    // manipulation
    String pathStr = path.string().c_str();  // Convert std::string to String
    if (!pathStr.empty() && pathStr[0] == '~') {  // Check for empty string
        // Use String concatenation and substr
        pathStr = home + pathStr.substr(1);  // substr(pos) is sufficient
        // fs::path constructor accepts std::string or types convertible to it
        return fs::path(pathStr.c_str());  // Convert String back if needed
    }
    return path;
}

ATOM_INLINE auto hasMagic(const String &pathname) -> bool {  // Use String
    static const auto MAGIC_CHECK = std::regex("([*?[])");
    // std::regex_search usually takes std::string or const char*
    return std::regex_search(pathname.c_str(), MAGIC_CHECK);
}

ATOM_INLINE auto isHidden(const String &pathname) -> bool {  // Use String
    // std::regex_match usually takes std::string or const char*
    return std::regex_match(pathname.c_str(),
                            std::regex(R"(^(.*\/)*\.[^\.\/]+\/*$)"));
}

ATOM_INLINE auto isRecursive(const String &pattern) -> bool {  // Use String
    return pattern == "**";
}

ATOM_INLINE auto iterDirectory(const fs::path &dirname,
                               bool dironly)
    -> Vector<fs::path> {     // Use Vector
    Vector<fs::path> result;  // Use Vector

    auto currentDirectory = dirname;
    if (currentDirectory.empty()) {
        // Use default constructor for current path
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
                        // fs::relative might need adjustment depending on path
                        // types
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

ATOM_INLINE auto rlistdir(const fs::path &dirname,
                          bool dironly) -> Vector<fs::path> {  // Use Vector
    Vector<fs::path> result;                                   // Use Vector
    auto names = iterDirectory(dirname, dironly);
    for (auto &name : names) {
        // isHidden expects String, name.string() returns std::string
        if (!isHidden(
                name.string().c_str())) {  // Convert to String via c_str()
            result.push_back(name);
            // Recurse and extend the result vector
            auto subNames = rlistdir(name, dironly);
            result.insert(result.end(), subNames.begin(), subNames.end());
        }
    }
    return result;
}

ATOM_INLINE auto glob2(const fs::path &dirname,
                       [[maybe_unused]] const String &pattern,  // Use String
                       bool dironly) -> Vector<fs::path> {      // Use Vector
    Vector<fs::path> result;                                    // Use Vector
    assert(isRecursive(pattern));
    for (auto &dir : rlistdir(dirname, dironly)) {
        result.push_back(dir);
    }
    return result;
}

ATOM_INLINE auto glob1(const fs::path &dirname,
                       const String &pattern,               // Use String
                       bool dironly) -> Vector<fs::path> {  // Use Vector
    auto names = iterDirectory(dirname, dironly);
    Vector<fs::path> filteredNames;  // Use Vector
    for (auto &name : names) {
        // isHidden expects String, name.string() returns std::string
        if (!isHidden(
                name.string().c_str())) {  // Convert to String via c_str()
            filteredNames.push_back(name.filename());
        }
    }
    return filter(filteredNames, pattern);
}

ATOM_INLINE auto glob0(const fs::path &dirname, const fs::path &basename,
                       bool /*dironly*/) -> Vector<fs::path> {  // Use Vector
    Vector<fs::path> result;                                    // Use Vector
    // Check if basename is empty using fs::path::empty()
    if (basename.empty()) {
        // Check if dirname is a directory
        if (fs::is_directory(dirname)) {
            // result = {basename}; // This would add an empty path if basename
            // is empty
            result.push_back(
                dirname);  // Maybe push dirname itself? Or handle based on
                           // desired glob0 behavior. Let's assume pushing
                           // basename is intended.
            if (fs::exists(dirname)) {  // Ensure dirname exists before adding
                                        // empty basename?
                result.push_back(basename);  // Add the empty path part
            }
        }
    } else {
        // Check if the combined path exists
        if (fs::exists(dirname / basename)) {
            result = {basename};
        }
    }
    return result;
}

ATOM_INLINE auto glob(const String &pathname,
                      bool recursive = false,                      // Use String
                      bool dironly = false) -> Vector<fs::path> {  // Use Vector
    Vector<fs::path> result;                                       // Use Vector

    // fs::path constructor can usually take std::string or const char*
    auto path = fs::path(pathname.c_str());

    // Check first character of String
    if (!pathname.empty() && pathname[0] == '~') {
        path = expandTilde(path);
    }

    auto dirname = path.parent_path();
    // Use fs::path::filename() which returns fs::path
    const auto BASENAME_PATH = path.filename();
    // Convert filename path to String for magic checks etc.
    const String BASENAME = BASENAME_PATH.string().c_str();

    if (!hasMagic(pathname)) {
        assert(!dironly);
        // Use fs::path::empty()
        if (!BASENAME_PATH.empty()) {
            if (fs::exists(path)) {
                result.push_back(path);
            }
        } else {
            // If BASENAME is empty, path refers to dirname
            if (fs::is_directory(dirname)) {
                result.push_back(path);
            }
        }
        return result;
    }

    // Use fs::path::empty()
    if (dirname.empty()) {
        // Use String BASENAME for checks
        if (recursive && isRecursive(BASENAME)) {
            return glob2(dirname, BASENAME, dironly);
        }
        // Pass String BASENAME to glob1
        return glob1(dirname, BASENAME, dironly);
    }

    Vector<fs::path> dirs;  // Use Vector
    // Use fs::path::string() and convert to String for hasMagic
    if (dirname != path && hasMagic(dirname.string().c_str())) {
        // Pass String for recursive glob call
        dirs = glob(dirname.string().c_str(), recursive, true);
    } else {
        dirs = {dirname};
    }

    // Adjust function pointer type to use String
    std::function<Vector<fs::path>(const fs::path &, const String &, bool)>
        globInDir;
    if (hasMagic(BASENAME)) {
        if (recursive && isRecursive(BASENAME)) {
            globInDir = glob2;
        } else {
            globInDir = glob1;
        }
    } else {
        // glob0 expects fs::path basename
        // Need a wrapper or adjust glob0 if it should take String
        auto glob0Wrapper = [](const fs::path &dir, const String &baseStr,
                               bool dironly_flag) -> Vector<fs::path> {
            return glob0(dir, fs::path(baseStr.c_str()), dironly_flag);
        };
        globInDir = glob0Wrapper;
    }

    for (auto &dir : dirs) {
        // Pass String BASENAME to globInDir
        for (auto &name : globInDir(dir, BASENAME, dironly)) {
            fs::path subresult = name;
            // Check parent_path().empty() on fs::path `name`
            if (name.parent_path().empty()) {
                subresult = dir / name;
            }
            result.push_back(subresult);
        }
    }

    return result;
}

// Overloads taking single String
static ATOM_INLINE auto glob(const String &pathname)  // Use String
    -> Vector<fs::path> {                             // Use Vector
    return glob(pathname, false);
}

static ATOM_INLINE auto rglob(const String &pathname)  // Use String
    -> Vector<fs::path> {                              // Use Vector
    return glob(pathname, true);
}

// Overloads taking Vector<String>
static ATOM_INLINE auto glob(
    const Vector<String> &pathnames)  // Use Vector<String>
    -> Vector<fs::path> {             // Use Vector
    Vector<fs::path> result;          // Use Vector
    for (const auto &pathname : pathnames) {
        for (auto &match : glob(pathname, false)) {
            result.push_back(std::move(match));
        }
    }
    return result;
}

static ATOM_INLINE auto rglob(
    const Vector<String> &pathnames)  // Use Vector<String>
    -> Vector<fs::path> {             // Use Vector
    Vector<fs::path> result;          // Use Vector
    for (const auto &pathname : pathnames) {
        for (auto &match : glob(pathname, true)) {
            result.push_back(std::move(match));
        }
    }
    return result;
}

// Overloads taking initializer_list<String>
static ATOM_INLINE auto glob(const std::initializer_list<String>  // Use String
                                 &pathnames)
    -> Vector<fs::path> {  // Use Vector
    // Construct Vector<String> from initializer_list
    return glob(Vector<String>(pathnames));
}

static ATOM_INLINE auto rglob(const std::initializer_list<String>  // Use String
                                  &pathnames)
    -> Vector<fs::path> {  // Use Vector
    // Construct Vector<String> from initializer_list
    return rglob(Vector<String>(pathnames));
}

}  // namespace atom::io
