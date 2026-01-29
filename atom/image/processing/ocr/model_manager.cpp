#include "model_manager.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>

#endif

namespace fs = std::filesystem;

namespace atom::image::ocr {

// Default model URLs
static const char* EAST_MODEL_URL =
    "https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/"
    "frozen_east_text_detection.pb";
static const char* ESPCN_MODEL_URL =
    "https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb";
static const char* ENGLISH_DICT_URL =
    "https://raw.githubusercontent.com/dwyl/english-words/master/words.txt";

ModelManager::ModelManager(const std::string& modelsDir,
                           const std::string& dictDir)
    : m_modelsDir(modelsDir), m_dictDir(dictDir) {}

bool ModelManager::initialize() {
    try {
        // Create directories if they don't exist
        if (!fs::exists(m_modelsDir)) {
            fs::create_directories(m_modelsDir);
        }
        if (!fs::exists(m_dictDir)) {
            fs::create_directories(m_dictDir);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize ModelManager: " << e.what()
                  << std::endl;
        return false;
    }
}

void ModelManager::registerModel(const ModelInfo& info) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_models[info.name] = info;
}

bool ModelManager::isModelAvailable(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_models.find(modelName);
    if (it == m_models.end()) {
        return false;
    }
    return fs::exists(it->second.localPath);
}

std::string ModelManager::getModelPath(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_models.find(modelName);
    if (it != m_models.end() && fs::exists(it->second.localPath)) {
        return it->second.localPath;
    }
    return "";
}

bool ModelManager::downloadModel(const std::string& modelName,
                                 ProgressCallback callback) {
    ModelInfo info;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_models.find(modelName);
        if (it == m_models.end()) {
            if (callback) {
                DownloadProgress progress;
                progress.errorMessage = "Model not registered: " + modelName;
                callback(progress);
            }
            return false;
        }
        info = it->second;
    }

    // Check if already downloaded
    if (fs::exists(info.localPath)) {
        if (callback) {
            DownloadProgress progress;
            progress.completed = true;
            progress.success = true;
            progress.percentage = 100.0f;
            progress.status = "Model already exists";
            callback(progress);
        }
        return true;
    }

    // Create parent directory if needed
    fs::path parentPath = fs::path(info.localPath).parent_path();
    if (!parentPath.empty() && !fs::exists(parentPath)) {
        fs::create_directories(parentPath);
    }

    // Download the file
    return downloadFile(info.url, info.localPath, callback);
}

size_t ModelManager::downloadAllModels(ProgressCallback callback) {
    std::vector<std::string> modelNames = getRegisteredModels();
    size_t successCount = 0;

    for (size_t i = 0; i < modelNames.size(); ++i) {
        const auto& name = modelNames[i];

        if (callback) {
            DownloadProgress progress;
            progress.status = "Downloading " + name + " (" +
                              std::to_string(i + 1) + "/" +
                              std::to_string(modelNames.size()) + ")";
            callback(progress);
        }

        if (downloadModel(name, callback)) {
            successCount++;
        }
    }

    return successCount;
}

std::future<bool> ModelManager::downloadModelAsync(const std::string& modelName,
                                                   ProgressCallback callback) {
    return std::async(std::launch::async, [this, modelName, callback]() {
        return this->downloadModel(modelName, callback);
    });
}

bool ModelManager::verifyModel(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_models.find(modelName);
    if (it == m_models.end()) {
        return false;
    }

    const auto& info = it->second;
    if (!fs::exists(info.localPath)) {
        return false;
    }

    // Check file size if specified
    if (info.expectedSize > 0) {
        size_t actualSize = fs::file_size(info.localPath);
        if (actualSize != info.expectedSize) {
            return false;
        }
    }

    // Check SHA256 if specified
    if (!info.sha256.empty()) {
        std::string actualHash = calculateSHA256(info.localPath);
        return actualHash == info.sha256;
    }

    return true;
}

std::vector<std::string> ModelManager::getRegisteredModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_models.size());
    for (const auto& [name, info] : m_models) {
        names.push_back(name);
    }
    return names;
}

std::optional<ModelInfo> ModelManager::getModelInfo(
    const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_models.find(modelName);
    if (it != m_models.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ModelManager::hasRequiredModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [name, info] : m_models) {
        if (info.required && !fs::exists(info.localPath)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> ModelManager::getMissingRequiredModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> missing;
    for (const auto& [name, info] : m_models) {
        if (info.required && !fs::exists(info.localPath)) {
            missing.push_back(name);
        }
    }
    return missing;
}

bool ModelManager::deleteModel(const std::string& modelName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_models.find(modelName);
    if (it == m_models.end()) {
        return false;
    }

    try {
        if (fs::exists(it->second.localPath)) {
            fs::remove(it->second.localPath);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to delete model: " << e.what() << std::endl;
        return false;
    }
}

void ModelManager::clearAllModels() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [name, info] : m_models) {
        try {
            if (fs::exists(info.localPath)) {
                fs::remove(info.localPath);
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to delete " << name << ": " << e.what()
                      << std::endl;
        }
    }
}

size_t ModelManager::getTotalModelsSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t totalSize = 0;
    for (const auto& [name, info] : m_models) {
        if (fs::exists(info.localPath)) {
            totalSize += fs::file_size(info.localPath);
        }
    }
    return totalSize;
}

void ModelManager::registerDefaultModels() {
    // EAST text detection model
    ModelInfo eastModel;
    eastModel.name = "east_text_detection";
    eastModel.url = EAST_MODEL_URL;
    eastModel.localPath = m_modelsDir + "/east_text_detection.pb";
    eastModel.required = false;  // Not strictly required
    eastModel.description =
        "EAST text detection model for locating text regions";
    registerModel(eastModel);

    // ESPCN super resolution model
    ModelInfo espcnModel;
    espcnModel.name = "espcn_x4";
    espcnModel.url = ESPCN_MODEL_URL;
    espcnModel.localPath = m_modelsDir + "/ESPCN_x4.pb";
    espcnModel.required = false;
    espcnModel.description = "ESPCN 4x super resolution model";
    registerModel(espcnModel);
}

bool ModelManager::downloadDictionary(const std::string& language,
                                      ProgressCallback callback) {
    std::string url;
    std::string localPath = m_dictDir + "/" + language + ".txt";

    // Currently only English is supported from the default source
    if (language == "eng" || language == "english") {
        url = ENGLISH_DICT_URL;
        localPath = m_dictDir + "/english.txt";
    } else {
        if (callback) {
            DownloadProgress progress;
            progress.errorMessage =
                "Dictionary not available for language: " + language;
            callback(progress);
        }
        return false;
    }

    // Check if already exists
    if (fs::exists(localPath)) {
        if (callback) {
            DownloadProgress progress;
            progress.completed = true;
            progress.success = true;
            progress.percentage = 100.0f;
            progress.status = "Dictionary already exists";
            callback(progress);
        }
        return true;
    }

    // Create directory if needed
    if (!fs::exists(m_dictDir)) {
        fs::create_directories(m_dictDir);
    }

    return downloadFile(url, localPath, callback);
}

bool ModelManager::isDictionaryAvailable(const std::string& language) const {
    std::string localPath;
    if (language == "eng" || language == "english") {
        localPath = m_dictDir + "/english.txt";
    } else {
        localPath = m_dictDir + "/" + language + ".txt";
    }
    return fs::exists(localPath);
}

std::string ModelManager::getDictionaryPath(const std::string& language) const {
    std::string localPath;
    if (language == "eng" || language == "english") {
        localPath = m_dictDir + "/english.txt";
    } else {
        localPath = m_dictDir + "/" + language + ".txt";
    }

    if (fs::exists(localPath)) {
        return localPath;
    }
    return "";
}

bool ModelManager::downloadFile(const std::string& url,
                                const std::string& localPath,
                                ProgressCallback callback) {
    if (callback) {
        DownloadProgress progress;
        progress.status = "Starting download...";
        callback(progress);
    }

    bool result = httpDownload(url, localPath, callback);

    if (callback) {
        DownloadProgress progress;
        progress.completed = true;
        progress.success = result;
        progress.percentage = result ? 100.0f : 0.0f;
        progress.status = result ? "Download complete" : "Download failed";
        callback(progress);
    }

    return result;
}

std::string ModelManager::calculateSHA256(const std::string& filePath) const {
    // Simple SHA256 calculation (for full implementation, use OpenSSL or
    // similar) This is a placeholder - in production, use a proper crypto
    // library
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    // Read file and create a simple hash (not a real SHA256)
    // For production, use OpenSSL: SHA256_Init, SHA256_Update, SHA256_Final
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    size_t hash = 0;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash = hash * 31 + static_cast<unsigned char>(buffer[i]);
        }
    }

    oss << std::setw(16) << hash;
    return oss.str();
}

bool ModelManager::httpDownload(const std::string& url,
                                const std::string& localPath,
                                ProgressCallback callback) {
#ifdef _WIN32
    return downloadWindows(url, localPath, callback);
#else
    return downloadUnix(url, localPath, callback);
#endif
}

#ifdef _WIN32
bool ModelManager::downloadWindows(const std::string& url,
                                   const std::string& localPath,
                                   ProgressCallback callback) {
    // Parse URL
    std::wstring wUrl(url.begin(), url.end());

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {0};
    wchar_t urlPath[2048] = {0};

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        return false;
    }

    // Open session
    HINTERNET hSession =
        WinHttpOpen(L"AtomOCR/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        return false;
    }

    // Connect to host
    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Create request
    DWORD flags =
        (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Open output file
    std::ofstream outFile(localPath, std::ios::binary);
    if (!outFile.is_open()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read data
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    size_t totalRead = 0;
    std::vector<char> buffer(8192);

    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) &&
           bytesAvailable > 0) {
        if (bytesAvailable > buffer.size()) {
            buffer.resize(bytesAvailable);
        }

        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable,
                            &bytesRead)) {
            outFile.write(buffer.data(), bytesRead);
            totalRead += bytesRead;

            if (callback) {
                DownloadProgress progress;
                progress.bytesDownloaded = totalRead;
                progress.status = "Downloading...";
                callback(progress);
            }
        }
    }

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return totalRead > 0;
}
#else
bool ModelManager::downloadUnix(const std::string& url,
                                const std::string& localPath,
                                ProgressCallback callback) {
    // Use curl or wget command-line tools
    std::string command;

    // Check for curl first
    if (std::system("which curl > /dev/null 2>&1") == 0) {
        command = "curl -L -o \"" + localPath + "\" \"" + url + "\" 2>&1";
    } else if (std::system("which wget > /dev/null 2>&1") == 0) {
        command = "wget -O \"" + localPath + "\" \"" + url + "\" 2>&1";
    } else {
        std::cerr << "Neither curl nor wget is available for downloading"
                  << std::endl;
        return false;
    }

    if (callback) {
        DownloadProgress progress;
        progress.status = "Downloading using system tools...";
        callback(progress);
    }

    int result = std::system(command.c_str());

    if (result != 0) {
        // Try to clean up partial download
        if (fs::exists(localPath)) {
            fs::remove(localPath);
        }
        return false;
    }

    return fs::exists(localPath) && fs::file_size(localPath) > 0;
}
#endif

std::unique_ptr<ModelManager> createDefaultModelManager() {
    auto manager = std::make_unique<ModelManager>("./models", "./dict");
    manager->initialize();
    manager->registerDefaultModels();
    return manager;
}

}  // namespace atom::image::ocr
