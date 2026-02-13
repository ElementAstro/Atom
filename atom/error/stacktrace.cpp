#include "stacktrace.hpp"
#include "atom/meta/abi.hpp"

// Optional dependencies for compression/decompression
#ifdef ATOM_ENABLE_STACKTRACE_COMPRESSION
#include "atom/algorithm/hash/hash.hpp"
#include "atom/io/compression/compress.hpp"
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <iomanip>
#include <regex>
#include <sstream>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
// clang-format on
#if !defined(__MINGW32__) && !defined(__MINGW64__)
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#endif
#elif defined(__APPLE__) || defined(__linux__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <link.h>
#endif
#endif

#ifdef ATOM_USE_BOOST
#include <boost/stacktrace.hpp>
#endif

namespace atom::error {

// Static member definitions
StackTraceConfig StackTrace::defaultConfig_;
StackTraceMetrics StackTrace::globalMetrics_;
std::mutex StackTrace::globalMutex_;

namespace {

#if defined(__linux__) || defined(__APPLE__)
auto processString(const std::string& input) -> std::string {
    size_t startIndex = input.find("_Z");
    if (startIndex == std::string::npos) {
        return input;
    }

    size_t endIndex = input.find('+', startIndex);
    if (endIndex == std::string::npos) {
        return input;
    }

    std::string abiName = input.substr(startIndex, endIndex - startIndex);
    abiName = atom::meta::DemangleHelper::demangle(abiName);

    std::string result = input;
    result.replace(startIndex, endIndex - startIndex, abiName);
    return result;
}
#endif

auto prettifyStacktrace(const std::string& input) -> std::string {
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
        output = std::regex_replace(output, std::regex(from), to);
    }

    output =
        std::regex_replace(output, std::regex(R"(<\s*([^<> ]+)\s*>)"), "<$1>");
    output = std::regex_replace(
        output, std::regex(R"(<([^<>]*)<([^<>]*)>\s*([^<>]*)>)"), "<$1<$2>$3>");
    output = std::regex_replace(output, std::regex(R"(\s{2,})"), " ");

    return output;
}

auto formatAddress(uintptr_t address) -> std::string {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setfill('0')
        << std::setw(sizeof(void*) * 2) << address;
    return oss.str();
}

auto getBaseName(const std::string& path) -> std::string {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

}  // namespace

// FrameInfo implementations
auto FrameInfo::toString() const -> std::string {
    std::ostringstream oss;
    oss << functionName << " at "
        << formatAddress(reinterpret_cast<uintptr_t>(address));

    if (!moduleName.empty()) {
        oss << " in " << getBaseName(moduleName);
        if (offset > 0) {
            oss << " (+" << std::hex << offset << ")";
        }
    }

    if (!fileName.empty() && lineNumber > 0) {
        oss << " (" << getBaseName(fileName) << ":" << lineNumber << ")";
    }

    return oss.str();
}

auto FrameInfo::toJson() const -> std::string {
    std::ostringstream oss;
    oss << "{"
        << "\"address\":\""
        << formatAddress(reinterpret_cast<uintptr_t>(address)) << "\","
        << "\"function\":\"" << functionName << "\","
        << "\"module\":\"" << moduleName << "\","
        << "\"file\":\"" << fileName << "\","
        << "\"line\":" << lineNumber << ","
        << "\"offset\":" << offset << "}";
    return oss.str();
}

auto FrameInfo::toXml() const -> std::string {
    std::ostringstream oss;
    oss << "<frame>"
        << "<address>" << formatAddress(reinterpret_cast<uintptr_t>(address))
        << "</address>"
        << "<function>" << functionName << "</function>"
        << "<module>" << moduleName << "</module>"
        << "<file>" << fileName << "</file>"
        << "<line>" << lineNumber << "</line>"
        << "<offset>" << offset << "</offset>"
        << "</frame>";
    return oss.str();
}

// SymbolCache implementation
StackTrace::SymbolCache::SymbolCache(size_t maxSize,
                                     std::chrono::milliseconds timeout)
    : maxSize_(maxSize), timeout_(timeout) {}

auto StackTrace::SymbolCache::get(void* key) -> std::optional<std::string> {
    std::shared_lock lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        auto now = std::chrono::steady_clock::now();
        if (now - it->second.lastAccess < timeout_) {
            it->second.lastAccess = now;
            it->second.accessCount++;
            hits_++;
            return it->second.value;
        } else {
            lock.unlock();
            std::unique_lock ulock(mutex_);
            cache_.erase(it);
        }
    }
    misses_++;
    return std::nullopt;
}

void StackTrace::SymbolCache::put(void* key, const std::string& value) {
    std::unique_lock lock(mutex_);

    if (cache_.size() >= maxSize_) {
        evictLRU();
    }

    cache_.emplace(key, CacheEntry(value));
}

void StackTrace::SymbolCache::clear() {
    std::unique_lock lock(mutex_);
    cache_.clear();
    hits_ = 0;
    misses_ = 0;
}

auto StackTrace::SymbolCache::getStats() const -> std::pair<double, size_t> {
    std::shared_lock lock(mutex_);
    auto hits = hits_.load();
    auto misses = misses_.load();
    auto total = hits + misses;
    double hitRatio = total > 0 ? static_cast<double>(hits) / total : 0.0;
    return {hitRatio, cache_.size()};
}

void StackTrace::SymbolCache::evictOldEntries() {
    auto now = std::chrono::steady_clock::now();
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (now - it->second.lastAccess >= timeout_) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void StackTrace::SymbolCache::evictLRU() {
    if (cache_.empty())
        return;

    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.lastAccess < oldest->second.lastAccess) {
            oldest = it;
        }
    }
    cache_.erase(oldest);
}

// StackTrace constructors and methods
StackTrace::StackTrace() : config_(defaultConfig_) {
    if (config_.enableCaching) {
#ifdef _WIN32
        moduleCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
#elif defined(__APPLE__) || defined(__linux__)
        symbolCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
#endif
    }
    capture();
}

StackTrace::StackTrace(const StackTraceConfig& config) : config_(config) {
    if (config_.enableCaching) {
#ifdef _WIN32
        moduleCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
#elif defined(__APPLE__) || defined(__linux__)
        symbolCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
#endif
    }
    capture();
}

auto StackTrace::toString() const -> std::string {
    return toString(config_.outputFormat);
}

auto StackTrace::toString(StackTraceConfig::OutputFormat format) const
    -> std::string {
    auto frames = getFrames();
    return formatFrames(frames, format);
}

auto StackTrace::getFrames() const -> std::vector<FrameInfo> {
    std::vector<FrameInfo> result;

#ifdef ATOM_USE_BOOST
    // For boost stacktrace, we'll need to convert to our format
    // This is a simplified implementation
    auto trace = boost::stacktrace::stacktrace();
    for (size_t i = 0; i < trace.size(); ++i) {
        FrameInfo frame;
        frame.address = const_cast<void*>(trace[i].address());
        frame.functionName = trace[i].name();
        frame.timestamp = std::chrono::system_clock::now();
        result.push_back(std::move(frame));
    }
#elif defined(_WIN32)
    result.reserve(frames_.size());
    for (size_t i = 0; i < frames_.size(); ++i) {
        result.push_back(processFrame(frames_[i], static_cast<int>(i)));
    }
#elif defined(__APPLE__) || defined(__linux__)
    result.reserve(num_frames_);
    for (int i = 0; i < num_frames_; ++i) {
        result.push_back(processFrame(frames_[i], i));
    }
#endif

    return result;
}

#ifdef _WIN32
auto StackTrace::processFrame(void* frame, int frameIndex) const -> FrameInfo {
    FrameInfo frameInfo;
    frameInfo.address = frame;
    frameInfo.timestamp = std::chrono::system_clock::now();

    uintptr_t address = reinterpret_cast<uintptr_t>(frame);

    // Check cache first
    if (config_.enableCaching && moduleCache_) {
        auto cached = moduleCache_->get(frame);
        if (cached) {
            // Parse cached result back to FrameInfo
            // For simplicity, we'll just use the cached string as function name
            frameInfo.functionName = *cached;
            return frameInfo;
        }
    }

    HMODULE module;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(frame), &module)) {
        wchar_t modulePath[MAX_PATH];
        if (GetModuleFileNameW(module, modulePath, MAX_PATH) > 0) {
            char modPathA[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, modPathA, MAX_PATH,
                                nullptr, nullptr);
            frameInfo.moduleName = modPathA;
        }
    }

    constexpr size_t MAX_SYMBOL_LEN = 1024;
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(
        calloc(sizeof(SYMBOL_INFO) + MAX_SYMBOL_LEN * sizeof(char), 1));
    if (!symbol) {
        frameInfo.functionName = "<memory allocation failed>";
        return frameInfo;
    }

    symbol->MaxNameLen = MAX_SYMBOL_LEN - 1;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    DWORD64 displacement = 0;
    frameInfo.functionName = "<unknown function>";
    if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol)) {
        frameInfo.functionName = atom::meta::DemangleHelper::demangle(
            std::string("_") + symbol->Name);
    }

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD lineDisplacement = 0;

    if (SymGetLineFromAddr64(GetCurrentProcess(), address, &lineDisplacement,
                             &line)) {
        frameInfo.fileName = line.FileName;
        frameInfo.lineNumber = line.LineNumber;
    }

    free(symbol);

    // Cache the result
    if (config_.enableCaching && moduleCache_) {
        moduleCache_->put(frame, frameInfo.toString());
    }

    return frameInfo;
}

#elif defined(__APPLE__) || defined(__linux__)
auto StackTrace::processFrame(void* frame, int frameIndex) const -> FrameInfo {
    FrameInfo frameInfo;
    frameInfo.address = frame;
    frameInfo.timestamp = std::chrono::system_clock::now();

    uintptr_t address = reinterpret_cast<uintptr_t>(frame);

    // Check cache first
    if (config_.enableCaching && symbolCache_) {
        auto cached = symbolCache_->get(frame);
        if (cached) {
            // Parse cached result back to FrameInfo
            frameInfo.functionName = *cached;
            return frameInfo;
        }
    }

    Dl_info dlInfo;
    frameInfo.functionName = "<unknown function>";

    if (dladdr(frame, &dlInfo)) {
        if (dlInfo.dli_fname) {
            frameInfo.moduleName = dlInfo.dli_fname;
        }

        if (dlInfo.dli_fbase) {
            frameInfo.offset =
                address - reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
        }

        if (dlInfo.dli_sname) {
            frameInfo.functionName =
                atom::meta::DemangleHelper::demangle(dlInfo.dli_sname);
        }
    }

    if (frameInfo.functionName == "<unknown function>" &&
        frameIndex < num_frames_ && symbols_) {
        std::string symbol(symbols_.get()[frameIndex]);

        std::regex functionRegex(
            R"((?:.*$$0x[0-9a-f]+$$)\s+(.+)\s+\+\s+0x[0-9a-f]+)");
        std::smatch matches;
        if (std::regex_search(symbol, matches, functionRegex) &&
            matches.size() > 1) {
            frameInfo.functionName =
                atom::meta::DemangleHelper::demangle(matches[1].str());
        } else {
            frameInfo.functionName = processString(symbol);
        }
    }

    // Cache the result
    if (config_.enableCaching && symbolCache_) {
        symbolCache_->put(frame, frameInfo.toString());
    }

    return frameInfo;
}

#else
auto StackTrace::processFrame(void* frame, int frameIndex) const -> FrameInfo {
    FrameInfo frameInfo;
    frameInfo.address = frame;
    frameInfo.functionName = "<frame information unavailable>";
    frameInfo.timestamp = std::chrono::system_clock::now();
    return frameInfo;
}
#endif

void StackTrace::capture() {
    auto startTime = std::chrono::high_resolution_clock::now();

#ifdef ATOM_USE_BOOST
    // Boost stacktrace automatically captures the stack trace
#elif defined(_WIN32)
    frames_.resize(config_.maxFrames);

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
                  SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_EXACT_SYMBOLS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    void* framePtrs[256];  // Use larger buffer
    WORD capturedFrames = CaptureStackBackTrace(
        config_.skipFrames,
        std::min(static_cast<DWORD>(config_.maxFrames), 256U), framePtrs,
        nullptr);

    frames_.resize(capturedFrames);
    std::copy_n(framePtrs, capturedFrames, frames_.begin());

    if (config_.enableCaching && moduleCache_) {
        // Don't clear cache on every capture for better performance
    }

#elif defined(__APPLE__) || defined(__linux__)
    void* framePtrs[256];  // Use larger buffer

    int totalFrames = backtrace(
        framePtrs,
        std::min(static_cast<int>(config_.maxFrames + config_.skipFrames),
                 256));
    if (totalFrames > static_cast<int>(config_.skipFrames)) {
        num_frames_ = totalFrames - config_.skipFrames;
        symbols_.reset(
            backtrace_symbols(framePtrs + config_.skipFrames, num_frames_));
        frames_.assign(framePtrs + config_.skipFrames, framePtrs + totalFrames);
    } else {
        symbols_.reset(nullptr);
        frames_.clear();
        num_frames_ = 0;
    }

    if (config_.enableCaching && symbolCache_) {
        // Don't clear cache on every capture for better performance
    }

#else
    num_frames_ = 0;
#endif

    // Update performance metrics
    if (config_.enablePerfMonitoring) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            endTime - startTime);

        metrics_.captureCount++;
        metrics_.totalCaptureTime += duration.count();

        // Update global metrics
        std::lock_guard<std::mutex> lock(globalMutex_);
        globalMetrics_.captureCount++;
        globalMetrics_.totalCaptureTime += duration.count();
    }
}

// Additional StackTrace methods implementation
StackTrace::StackTrace(const StackTrace& other)
    : config_(other.config_), metrics_(other.metrics_), frames_(other.frames_) {
#ifdef _WIN32
    if (config_.enableCaching) {
        moduleCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
    }
#elif defined(__APPLE__) || defined(__linux__)
    num_frames_ = other.num_frames_;
    if (other.symbols_) {
        // Deep copy symbols
        symbols_.reset(backtrace_symbols(frames_.data(), num_frames_));
    }
    if (config_.enableCaching) {
        symbolCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
    }
#endif
}

StackTrace::StackTrace(StackTrace&& other) noexcept
    : config_(std::move(other.config_)),
      metrics_(std::move(other.metrics_)),
      frames_(std::move(other.frames_)) {
#ifdef _WIN32
    moduleCache_ = std::move(other.moduleCache_);
#elif defined(__APPLE__) || defined(__linux__)
    num_frames_ = other.num_frames_;
    symbols_ = std::move(other.symbols_);
    symbolCache_ = std::move(other.symbolCache_);
    other.num_frames_ = 0;
#endif
}

StackTrace& StackTrace::operator=(const StackTrace& other) {
    if (this != &other) {
        config_ = other.config_;
        metrics_ = other.metrics_;
        frames_ = other.frames_;

#ifdef _WIN32
        if (config_.enableCaching) {
            moduleCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                         config_.cacheTimeout);
        } else {
            moduleCache_.reset();
        }
#elif defined(__APPLE__) || defined(__linux__)
        num_frames_ = other.num_frames_;
        if (other.symbols_) {
            symbols_.reset(backtrace_symbols(frames_.data(), num_frames_));
        } else {
            symbols_.reset();
        }
        if (config_.enableCaching) {
            symbolCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                         config_.cacheTimeout);
        } else {
            symbolCache_.reset();
        }
#endif
    }
    return *this;
}

StackTrace& StackTrace::operator=(StackTrace&& other) noexcept {
    if (this != &other) {
        config_ = std::move(other.config_);
        metrics_ = std::move(other.metrics_);
        frames_ = std::move(other.frames_);

#ifdef _WIN32
        moduleCache_ = std::move(other.moduleCache_);
#elif defined(__APPLE__) || defined(__linux__)
        num_frames_ = other.num_frames_;
        symbols_ = std::move(other.symbols_);
        symbolCache_ = std::move(other.symbolCache_);
        other.num_frames_ = 0;
#endif
    }
    return *this;
}

auto StackTrace::formatFrames(const std::vector<FrameInfo>& frames,
                              StackTraceConfig::OutputFormat format) const
    -> std::string {
    std::ostringstream oss;

    switch (format) {
        case StackTraceConfig::OutputFormat::SIMPLE:
            oss << "Stack trace:\n";
            for (size_t i = 0; i < frames.size(); ++i) {
                oss << "\t[" << i << "] " << frames[i].toString() << "\n";
            }
            break;

        case StackTraceConfig::OutputFormat::DETAILED:
            oss << "Stack trace (detailed):\n";
            for (size_t i = 0; i < frames.size(); ++i) {
                oss << "\t[" << i << "] " << frames[i].toString();
                if (frames[i].address) {
                    oss << " (timestamp: "
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               frames[i].timestamp.time_since_epoch())
                               .count()
                        << "ms)";
                }
                oss << "\n";
            }
            break;

        case StackTraceConfig::OutputFormat::JSON:
            oss << "{\n  \"stackTrace\": [\n";
            for (size_t i = 0; i < frames.size(); ++i) {
                oss << "    " << frames[i].toJson();
                if (i < frames.size() - 1)
                    oss << ",";
                oss << "\n";
            }
            oss << "  ]\n}";
            break;

        case StackTraceConfig::OutputFormat::XML:
            oss << "<stackTrace>\n";
            for (const auto& frame : frames) {
                oss << "  " << frame.toXml() << "\n";
            }
            oss << "</stackTrace>";
            break;
    }

    std::string result = oss.str();
    return config_.enablePrettify ? prettifyOutput(result) : result;
}

auto StackTrace::prettifyOutput(const std::string& input) const -> std::string {
    return prettifyStacktrace(input);  // Use existing function
}

auto StackTrace::getMetrics() const -> const StackTraceMetrics& {
    return metrics_;
}

void StackTrace::setConfig(const StackTraceConfig& config) {
    config_ = config;

    // Recreate caches with new configuration
#ifdef _WIN32
    if (config_.enableCaching) {
        moduleCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
    } else {
        moduleCache_.reset();
    }
#elif defined(__APPLE__) || defined(__linux__)
    if (config_.enableCaching) {
        symbolCache_ = std::make_unique<SymbolCache>(config_.cacheMaxSize,
                                                     config_.cacheTimeout);
    } else {
        symbolCache_.reset();
    }
#endif
}

auto StackTrace::getConfig() const -> const StackTraceConfig& {
    return config_;
}

void StackTrace::clearCache() {
#ifdef _WIN32
    if (moduleCache_) {
        moduleCache_->clear();
    }
#elif defined(__APPLE__) || defined(__linux__)
    if (symbolCache_) {
        symbolCache_->clear();
    }
#endif
}

auto StackTrace::getCacheStats() const -> std::pair<double, size_t> {
#ifdef _WIN32
    return moduleCache_ ? moduleCache_->getStats()
                        : std::make_pair(0.0, size_t(0));
#elif defined(__APPLE__) || defined(__linux__)
    return symbolCache_ ? symbolCache_->getStats()
                        : std::make_pair(0.0, size_t(0));
#else
    return {0.0, size_t(0)};
#endif
}

void StackTrace::setDefaultConfig(const StackTraceConfig& config) {
    std::lock_guard<std::mutex> lock(globalMutex_);
    defaultConfig_ = config;
}

auto StackTrace::getGlobalMetrics() -> StackTraceMetrics& {
    return globalMetrics_;
}

void StackTrace::addFilter(const FrameFilter& filter) {
    std::unique_lock lock(filterMutex_);
    filters_.push_back(filter);
}

void StackTrace::clearFilters() {
    std::unique_lock lock(filterMutex_);
    filters_.clear();
}

auto StackTrace::getFilteredFrames() const -> std::vector<FrameInfo> {
    auto allFrames = getFrames();
    if (filters_.empty()) {
        return allFrames;
    }

    std::vector<FrameInfo> filteredFrames;
    std::shared_lock lock(filterMutex_);

    for (const auto& frame : allFrames) {
        bool passesAllFilters = true;
        for (const auto& filter : filters_) {
            if (!filter(frame)) {
                passesAllFilters = false;
                break;
            }
        }
        if (passesAllFilters) {
            filteredFrames.push_back(frame);
        }
    }

    return filteredFrames;
}

// Advanced features implementation
auto StackTrace::captureAsync() -> std::future<StackTrace> {
    return std::async(std::launch::async, []() { return StackTrace(); });
}

auto StackTrace::captureAsync(const StackTraceConfig& config)
    -> std::future<StackTrace> {
    return std::async(std::launch::async,
                      [config]() { return StackTrace(config); });
}

auto StackTrace::compress(const std::string& input) -> std::string {
#ifdef ATOM_ENABLE_STACKTRACE_COMPRESSION
    try {
        // Use the existing compression component
        atom::io::CompressionOptions options;
        options.level = 6;             // Balanced compression level
        options.use_parallel = false;  // Keep it simple for stacktraces

        // Convert string to vector<unsigned char>
        atom::containers::Vector<unsigned char> inputData;
        inputData.reserve(input.size());
        for (char c : input) {
            inputData.push_back(static_cast<unsigned char>(c));
        }

        auto [result, compressedData] =
            atom::io::compressData(inputData, options);

        if (result.success && result.compression_ratio < 1.0) {
            // Convert compressed data to string for base64 encoding
            std::string binaryData;
            binaryData.reserve(compressedData.size());
            for (unsigned char c : compressedData) {
                binaryData.push_back(static_cast<char>(c));
            }

            // Use existing base64 encoding
            auto encodedResult =
                atom::algorithm::base64Encode(binaryData, true);
            if (encodedResult.has_value()) {
                return encodedResult.value();
            }
        }
    } catch (const std::exception&) {
        // Fall through to return original on any error
    }
#endif

    // Return original if compression fails or doesn't provide benefit
    return input;
}

auto StackTrace::decompress(const std::string& compressed) -> std::string {
#ifdef ATOM_ENABLE_STACKTRACE_COMPRESSION
    try {
        // Use existing base64 decoding
        auto decodedResult = atom::algorithm::base64Decode(compressed);
        if (!decodedResult.has_value()) {
            return compressed;  // Return original if base64 decoding fails
        }

        std::string decoded = decodedResult.value();

        // Convert to vector<unsigned char>
        atom::containers::Vector<unsigned char> compressedData;
        compressedData.reserve(decoded.size());
        for (char c : decoded) {
            compressedData.push_back(static_cast<unsigned char>(c));
        }

        // Use the existing decompression component
        atom::io::DecompressionOptions options;
        options.use_parallel = false;  // Keep it simple for stacktraces

        auto [result, decompressedData] =
            atom::io::decompressData(compressedData, 0, options);

        if (result.success) {
            // Convert back to string
            std::string decompressed;
            decompressed.reserve(decompressedData.size());
            for (unsigned char c : decompressedData) {
                decompressed.push_back(static_cast<char>(c));
            }
            return decompressed;
        }
    } catch (const std::exception&) {
        // Fall through to return original on any error
    }
#endif

    // Return original if decompression fails
    return compressed;
}

auto StackTrace::batchProcess(const std::vector<StackTrace>& traces,
                              StackTraceConfig::OutputFormat format)
    -> std::string {
    if (traces.empty()) {
        return "";
    }

    std::ostringstream oss;

    switch (format) {
        case StackTraceConfig::OutputFormat::JSON:
            oss << "{\n  \"stackTraces\": [\n";
            for (size_t i = 0; i < traces.size(); ++i) {
                auto frames = traces[i].getFrames();
                oss << "    {\n      \"index\": " << i << ",\n";
                oss << "      \"frames\": [\n";
                for (size_t j = 0; j < frames.size(); ++j) {
                    oss << "        " << frames[j].toJson();
                    if (j < frames.size() - 1)
                        oss << ",";
                    oss << "\n";
                }
                oss << "      ]\n    }";
                if (i < traces.size() - 1)
                    oss << ",";
                oss << "\n";
            }
            oss << "  ]\n}";
            break;

        case StackTraceConfig::OutputFormat::XML:
            oss << "<stackTraces>\n";
            for (size_t i = 0; i < traces.size(); ++i) {
                oss << "  <stackTrace index=\"" << i << "\">\n";
                auto frames = traces[i].getFrames();
                for (const auto& frame : frames) {
                    oss << "    " << frame.toXml() << "\n";
                }
                oss << "  </stackTrace>\n";
            }
            oss << "</stackTraces>";
            break;

        default:
            for (size_t i = 0; i < traces.size(); ++i) {
                oss << "=== Stack Trace " << i << " ===\n";
                oss << traces[i].toString() << "\n\n";
            }
            break;
    }

    return oss.str();
}

}  // namespace atom::error
