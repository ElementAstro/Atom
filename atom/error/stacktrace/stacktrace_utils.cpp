/*
 * stacktrace_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of stacktrace utility functions

**************************************************/

#include "stacktrace_utils.hpp"

#include <regex>
#include <sstream>
#include <vector>

#ifndef _WIN32
#include <cxxabi.h>
#endif
#include <cstdlib>

namespace atom::error {
namespace stacktrace_utils {

std::string demangle(const std::string& mangled) {
    if (mangled.empty()) {
        return mangled;
    }

    std::string result = mangled;

#if defined(__GNUC__) || defined(__clang__)
#ifndef _WIN32
    // Use abi::__cxa_demangle for GCC/Clang on non-Windows
    int status = 0;
    char* demangled =
        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        result = demangled;
        free(demangled);
        return result;
    }
#endif
#endif

    return mangled;
}

std::string prettify(const std::string& input) {
    std::string output = input;

    static const std::vector<std::pair<std::string, std::string>> REPLACEMENTS =
        {{"std::__1::", "std::"},
         {"std::__cxx11::", "std::"},
         {"__thiscall ", ""},
         {"__cdecl ", ""},
         {", std::allocator<[^<>]+>", ""},
         {"class ", ""},
         {"struct ", ""}};

    for (const auto& [from, to] : REPLACEMENTS) {
        try {
            output = std::regex_replace(output, std::regex(from), to);
        } catch (const std::regex_error&) {
            // Skip problematic regex patterns
            continue;
        }
    }

    try {
        output = std::regex_replace(output, std::regex(R"(<\s*([^<> ]+)\s*>)"),
                                    "<$1>");
        output = std::regex_replace(
            output, std::regex(R"(<([^<>]*)<([^<>]*)>\s*([^<>]*)>)"),
            "<$1<$2>$3>");
    } catch (const std::regex_error&) {
        // Return partially processed output if regex fails
    }

    return output;
}

std::string formatAddress(uintptr_t address) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << address;
    return oss.str();
}

std::string getBaseName(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

bool containsMangledNames(const std::string& str) {
    // Look for common C++ mangling patterns
    return str.find("_Z") != std::string::npos ||
           str.find("__Z") != std::string::npos ||
           str.find("?") == 0;  // MSVC mangling
}

}  // namespace stacktrace_utils
}  // namespace atom::error
