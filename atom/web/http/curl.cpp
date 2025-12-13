/*
 * curl.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "curl.hpp"

#ifdef USE_GNUTLS
#include <gnutls/gnutls.h>
#else
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#endif

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <stop_token>
#include <thread>

#include <spdlog/spdlog.h>

#include "atom/error/exception.hpp"

namespace atom::web {

constexpr long TIMEOUT_MS = 1000;

namespace {
void ensureCurlGlobalInit() {
    static const auto initialized = [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return 0;
    }();
    (void)initialized;
}
}  // namespace

class CurlWrapper::Impl {
public:
    Impl();
    ~Impl();

    auto setUrl(const std::string &url) -> CurlWrapper::Impl &;
    auto setRequestMethod(const std::string &method) -> CurlWrapper::Impl &;
    auto addHeader(const std::string &key,
                   const std::string &value) -> CurlWrapper::Impl &;
    auto setOnErrorCallback(std::function<void(CURLcode)> callback)
        -> CurlWrapper::Impl &;
    auto setOnResponseCallback(std::function<void(const std::string &)>
                                   callback) -> CurlWrapper::Impl &;
    auto setTimeout(long timeout) -> CurlWrapper::Impl &;
    auto setFollowLocation(bool follow) -> CurlWrapper::Impl &;
    auto setRequestBody(const std::string &data) -> CurlWrapper::Impl &;
    auto setUploadFile(const std::string &filePath) -> CurlWrapper::Impl &;
    auto setProxy(const std::string &proxy) -> CurlWrapper::Impl &;
    auto setSSLOptions(bool verifyPeer, bool verifyHost) -> CurlWrapper::Impl &;
    auto perform() -> std::string;
    auto performAsync() -> CurlWrapper::Impl &;
    void waitAll();
    auto setMaxDownloadSpeed(size_t speed) -> CurlWrapper::Impl &;
    auto setMaxUploadSpeed(size_t speed) -> CurlWrapper::Impl &;
    void setProgressCallback(
        std::function<void(size_t, size_t, size_t, size_t)> callback);
    void applyConfig(const RequestConfig &config);
    [[nodiscard]] auto getResponseCode() const -> int;
    [[nodiscard]] auto getEffectiveUrl() const -> std::string;
    [[nodiscard]] auto getContentType() const -> std::string;
    [[nodiscard]] auto getDownloadSize() const -> size_t;
    void reset();
    [[nodiscard]] auto performAsyncCancellable(std::stop_token stopToken)
        -> std::future<HttpResponse>;

private:
    CURL *handle_;
    CURLM *multiHandle_;
    curl_slist *headersList_;
    std::function<void(CURLcode)> onErrorCallback_;
    std::function<void(const std::string &)> onResponseCallback_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::string responseData_;
    std::string requestBody_;
    std::unique_ptr<std::ifstream> uploadFile_;
    std::thread worker_;
    bool asyncRunning_ = false;
    std::function<void(size_t, size_t, size_t, size_t)> progressCallback_;
    std::chrono::steady_clock::time_point requestStartTime_;

    static auto writeCallback(void *contents, size_t size, size_t nmemb,
                              void *userp) -> size_t;
    static auto readCallback(void *ptr, size_t size, size_t nmemb,
                             void *userp) -> size_t;
    static auto progressCallbackWrapper(void *clientp, curl_off_t dltotal,
                                        curl_off_t dlnow, curl_off_t ultotal,
                                        curl_off_t ulnow) -> int;
    void updateHeaders();
};

CurlWrapper::CurlWrapper() : pImpl_(std::make_unique<Impl>()) {}

CurlWrapper::~CurlWrapper() = default;

auto CurlWrapper::setUrl(const std::string &url) -> CurlWrapper & {
    pImpl_->setUrl(url);
    return *this;
}

auto CurlWrapper::setRequestMethod(const std::string &method) -> CurlWrapper & {
    pImpl_->setRequestMethod(method);
    return *this;
}

auto CurlWrapper::addHeader(const std::string &key,
                            const std::string &value) -> CurlWrapper & {
    pImpl_->addHeader(key, value);
    return *this;
}

auto CurlWrapper::setOnErrorCallback(std::function<void(CURLcode)> callback)
    -> CurlWrapper & {
    pImpl_->setOnErrorCallback(std::move(callback));
    return *this;
}

auto CurlWrapper::setOnResponseCallback(
    std::function<void(const std::string &)> callback) -> CurlWrapper & {
    pImpl_->setOnResponseCallback(std::move(callback));
    return *this;
}

auto CurlWrapper::setTimeout(long timeout) -> CurlWrapper & {
    pImpl_->setTimeout(timeout);
    return *this;
}

auto CurlWrapper::setFollowLocation(bool follow) -> CurlWrapper & {
    pImpl_->setFollowLocation(follow);
    return *this;
}

auto CurlWrapper::setRequestBody(const std::string &data) -> CurlWrapper & {
    pImpl_->setRequestBody(data);
    return *this;
}

auto CurlWrapper::setUploadFile(const std::string &filePath) -> CurlWrapper & {
    pImpl_->setUploadFile(filePath);
    return *this;
}

auto CurlWrapper::setProxy(const std::string &proxy) -> CurlWrapper & {
    pImpl_->setProxy(proxy);
    return *this;
}

auto CurlWrapper::setSSLOptions(bool verifyPeer,
                                bool verifyHost) -> CurlWrapper & {
    pImpl_->setSSLOptions(verifyPeer, verifyHost);
    return *this;
}

auto CurlWrapper::perform() -> std::string { return pImpl_->perform(); }

auto CurlWrapper::performAsync() -> CurlWrapper & {
    pImpl_->performAsync();
    return *this;
}

void CurlWrapper::waitAll() { pImpl_->waitAll(); }

auto CurlWrapper::setMaxDownloadSpeed(size_t speed) -> CurlWrapper & {
    pImpl_->setMaxDownloadSpeed(speed);
    return *this;
}

void CurlWrapper::setOnErrorCallback(
    std::function<void(const std::string &)> callback) {
    pImpl_->setOnErrorCallback([callback](CURLcode code) {
        if (callback) {
            callback(curl_easy_strerror(code));
        }
    });
}

void CurlWrapper::setOnResponseCallback(
    std::function<void(const std::string &)> callback) {
    pImpl_->setOnResponseCallback(std::move(callback));
}

void CurlWrapper::setProgressCallback(
    std::function<void(size_t, size_t, size_t, size_t)> callback) {
    pImpl_->setProgressCallback(std::move(callback));
}

void CurlWrapper::setMaxUploadSpeed(long bytesPerSecond) {
    pImpl_->setMaxUploadSpeed(static_cast<size_t>(bytesPerSecond));
}

void CurlWrapper::applyConfig(const RequestConfig &config) {
    pImpl_->applyConfig(config);
}

auto CurlWrapper::getResponseCode() const -> int {
    return pImpl_->getResponseCode();
}

auto CurlWrapper::getEffectiveUrl() const -> std::string {
    return pImpl_->getEffectiveUrl();
}

auto CurlWrapper::getContentType() const -> std::string {
    return pImpl_->getContentType();
}

auto CurlWrapper::getDownloadSize() const -> size_t {
    return pImpl_->getDownloadSize();
}

void CurlWrapper::reset() { pImpl_->reset(); }

auto CurlWrapper::performAsyncCancellable(std::stop_token stopToken)
    -> std::future<HttpResponse> {
    return pImpl_->performAsyncCancellable(std::move(stopToken));
}

auto CurlWrapper::get(std::string_view url,
                      const RequestConfig &config) -> HttpResponse {
    CurlWrapper wrapper;
    wrapper.setUrl(std::string(url));
    wrapper.setRequestMethod("GET");
    wrapper.applyConfig(config);

    HttpResponse response;
    auto startTime = std::chrono::steady_clock::now();

    try {
        response.body = wrapper.perform();
        response.statusCode = wrapper.getResponseCode();
        response.effectiveUrl = wrapper.getEffectiveUrl();
        response.contentType = wrapper.getContentType();
        response.downloadedBytes = wrapper.getDownloadSize();
    } catch (const std::exception &e) {
        spdlog::error("GET request failed: {}", e.what());
        response.statusCode = 0;
        response.statusMessage = e.what();
    }

    auto endTime = std::chrono::steady_clock::now();
    response.responseTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    return response;
}

auto CurlWrapper::post(std::string_view url, std::string_view body,
                       std::string_view contentType,
                       const RequestConfig &config) -> HttpResponse {
    CurlWrapper wrapper;
    wrapper.setUrl(std::string(url));
    wrapper.setRequestMethod("POST");
    wrapper.setRequestBody(std::string(body));
    wrapper.addHeader("Content-Type", std::string(contentType));
    wrapper.applyConfig(config);

    HttpResponse response;
    auto startTime = std::chrono::steady_clock::now();

    try {
        response.body = wrapper.perform();
        response.statusCode = wrapper.getResponseCode();
        response.effectiveUrl = wrapper.getEffectiveUrl();
        response.contentType = wrapper.getContentType();
        response.downloadedBytes = wrapper.getDownloadSize();
    } catch (const std::exception &e) {
        spdlog::error("POST request failed: {}", e.what());
        response.statusCode = 0;
        response.statusMessage = e.what();
    }

    auto endTime = std::chrono::steady_clock::now();
    response.responseTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    return response;
}

auto CurlWrapper::put(std::string_view url, std::string_view body,
                      std::string_view contentType,
                      const RequestConfig &config) -> HttpResponse {
    CurlWrapper wrapper;
    wrapper.setUrl(std::string(url));
    wrapper.setRequestMethod("PUT");
    wrapper.setRequestBody(std::string(body));
    wrapper.addHeader("Content-Type", std::string(contentType));
    wrapper.applyConfig(config);

    HttpResponse response;
    auto startTime = std::chrono::steady_clock::now();

    try {
        response.body = wrapper.perform();
        response.statusCode = wrapper.getResponseCode();
        response.effectiveUrl = wrapper.getEffectiveUrl();
        response.contentType = wrapper.getContentType();
        response.downloadedBytes = wrapper.getDownloadSize();
    } catch (const std::exception &e) {
        spdlog::error("PUT request failed: {}", e.what());
        response.statusCode = 0;
        response.statusMessage = e.what();
    }

    auto endTime = std::chrono::steady_clock::now();
    response.responseTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    return response;
}

auto CurlWrapper::del(std::string_view url,
                      const RequestConfig &config) -> HttpResponse {
    CurlWrapper wrapper;
    wrapper.setUrl(std::string(url));
    wrapper.setRequestMethod("DELETE");
    wrapper.applyConfig(config);

    HttpResponse response;
    auto startTime = std::chrono::steady_clock::now();

    try {
        response.body = wrapper.perform();
        response.statusCode = wrapper.getResponseCode();
        response.effectiveUrl = wrapper.getEffectiveUrl();
        response.contentType = wrapper.getContentType();
        response.downloadedBytes = wrapper.getDownloadSize();
    } catch (const std::exception &e) {
        spdlog::error("DELETE request failed: {}", e.what());
        response.statusCode = 0;
        response.statusMessage = e.what();
    }

    auto endTime = std::chrono::steady_clock::now();
    response.responseTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    return response;
}

auto CurlWrapper::downloadFile(
    std::string_view url, std::string_view filePath,
    std::function<void(size_t, size_t)> progressCallback,
    std::stop_token stopToken) -> bool {
    try {
        std::ofstream outFile(std::string(filePath), std::ios::binary);
        if (!outFile) {
            spdlog::error("Failed to open file for writing: {}", filePath);
            return false;
        }

        CurlWrapper wrapper;
        wrapper.setUrl(std::string(url));
        wrapper.setRequestMethod("GET");

        if (progressCallback) {
            wrapper.setProgressCallback(
                [&progressCallback](size_t dl, size_t dlTotal, size_t, size_t) {
                    progressCallback(dl, dlTotal);
                });
        }

        std::string response = wrapper.perform();

        if (stopToken.stop_requested()) {
            spdlog::info("Download cancelled");
            outFile.close();
            std::filesystem::remove(std::string(filePath));
            return false;
        }

        outFile.write(response.data(),
                      static_cast<std::streamsize>(response.size()));
        outFile.close();

        spdlog::info("File downloaded successfully: {}", filePath);
        return true;

    } catch (const std::exception &e) {
        spdlog::error("Download failed: {}", e.what());
        return false;
    }
}

CurlWrapper::Impl::Impl()
    : handle_(nullptr),
      multiHandle_(curl_multi_init()),
      headersList_(nullptr),
      worker_(),
      asyncRunning_(false) {
    spdlog::info("CurlWrapper::Impl constructor called");
    ensureCurlGlobalInit();
    if (!multiHandle_) {
        spdlog::error("Failed to initialize CURL multi handle");
        THROW_CURL_INITIALIZATION_ERROR(
            "Failed to initialize CURL multi handle.");
    }
    handle_ = curl_easy_init();
    if (handle_ == nullptr) {
        spdlog::error("Failed to initialize CURL");
        THROW_CURL_INITIALIZATION_ERROR("Failed to initialize CURL.");
    }
    curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);
    spdlog::info("CurlWrapper::Impl initialized successfully");
}

CurlWrapper::Impl::~Impl() {
    spdlog::info("CurlWrapper::Impl destructor called");
    waitAll();
    if (headersList_) {
        curl_slist_free_all(headersList_);
        headersList_ = nullptr;
    }
    if (handle_) {
        curl_easy_cleanup(handle_);
        handle_ = nullptr;
    }
    if (multiHandle_) {
        curl_multi_cleanup(multiHandle_);
        multiHandle_ = nullptr;
    }
    spdlog::info("CurlWrapper::Impl cleaned up successfully");
}

auto CurlWrapper::Impl::setUrl(const std::string &url) -> CurlWrapper::Impl & {
    spdlog::info("Setting URL: {}", url);
    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
    return *this;
}

auto CurlWrapper::Impl::setRequestMethod(const std::string &method)
    -> CurlWrapper::Impl & {
    spdlog::info("Setting HTTP method: {}", method);
    if (method == "GET") {
        curl_easy_setopt(handle_, CURLOPT_HTTPGET, 1L);
    } else if (method == "POST") {
        curl_easy_setopt(handle_, CURLOPT_POST, 1L);
    } else {
        curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, method.c_str());
    }
    return *this;
}

auto CurlWrapper::Impl::addHeader(
    const std::string &key, const std::string &value) -> CurlWrapper::Impl & {
    spdlog::info("Adding header: {}: {}", key, value);
    std::string header = key + ": " + value;
    headersList_ = curl_slist_append(headersList_, header.c_str());
    updateHeaders();
    return *this;
}

void CurlWrapper::Impl::updateHeaders() {
    if (headersList_) {
        curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headersList_);
    }
}

auto CurlWrapper::Impl::setOnErrorCallback(
    std::function<void(CURLcode)> callback) -> CurlWrapper::Impl & {
    spdlog::info("Setting onError callback");
    onErrorCallback_ = std::move(callback);
    return *this;
}

auto CurlWrapper::Impl::setOnResponseCallback(
    std::function<void(const std::string &)> callback) -> CurlWrapper::Impl & {
    spdlog::info("Setting onResponse callback");
    onResponseCallback_ = std::move(callback);
    return *this;
}

auto CurlWrapper::Impl::setTimeout(long timeout) -> CurlWrapper::Impl & {
    spdlog::info("Setting timeout: {}", timeout);
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT, timeout);
    return *this;
}

auto CurlWrapper::Impl::setFollowLocation(bool follow) -> CurlWrapper::Impl & {
    spdlog::info("Setting follow location: {}", follow);
    curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L);
    return *this;
}

auto CurlWrapper::Impl::setRequestBody(const std::string &data)
    -> CurlWrapper::Impl & {
    spdlog::info("Setting request body (size: {} bytes)", data.size());
    requestBody_ = data;
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, requestBody_.c_str());
    curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE, requestBody_.size());
    return *this;
}

auto CurlWrapper::Impl::setUploadFile(const std::string &filePath)
    -> CurlWrapper::Impl & {
    spdlog::info("Setting upload file: {}", filePath);
    uploadFile_ = std::make_unique<std::ifstream>(filePath, std::ios::binary);
    if (!uploadFile_->is_open()) {
        spdlog::error("Failed to open file: {}", filePath);
        throw std::runtime_error("Failed to open file for upload.");
    }

    uploadFile_->seekg(0, std::ios::end);
    auto fileSize = uploadFile_->tellg();
    uploadFile_->seekg(0, std::ios::beg);

    curl_easy_setopt(handle_, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(handle_, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(handle_, CURLOPT_READDATA, uploadFile_.get());
    curl_easy_setopt(handle_, CURLOPT_INFILESIZE_LARGE,
                     static_cast<curl_off_t>(fileSize));
    return *this;
}

auto CurlWrapper::Impl::readCallback(void *ptr, size_t size, size_t nmemb,
                                     void *userp) -> size_t {
    auto *file = static_cast<std::ifstream *>(userp);
    if (!file || !file->is_open()) {
        return 0;
    }

    size_t totalSize = size * nmemb;
    file->read(static_cast<char *>(ptr), totalSize);
    return static_cast<size_t>(file->gcount());
}

auto CurlWrapper::Impl::setProxy(const std::string &proxy)
    -> CurlWrapper::Impl & {
    spdlog::info("Setting proxy: {}", proxy);
    curl_easy_setopt(handle_, CURLOPT_PROXY, proxy.c_str());
    return *this;
}

auto CurlWrapper::Impl::setSSLOptions(bool verifyPeer,
                                      bool verifyHost) -> CurlWrapper::Impl & {
    spdlog::info("Setting SSL options: verifyPeer={}, verifyHost={}",
                 verifyPeer, verifyHost);
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, verifyPeer ? 1L : 0L);
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST, verifyHost ? 2L : 0L);
    return *this;
}

auto CurlWrapper::Impl::perform() -> std::string {
    spdlog::info("Performing synchronous request");
    std::lock_guard lock(mutex_);
    responseData_.clear();
    responseData_.reserve(4096);

    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &responseData_);

    CURLcode res = curl_easy_perform(handle_);
    if (res != CURLE_OK) {
        spdlog::error("CURL request failed: {}", curl_easy_strerror(res));
        if (onErrorCallback_) {
            onErrorCallback_(res);
        }
        THROW_CURL_RUNTIME_ERROR("CURL perform failed.");
    }

    if (onResponseCallback_) {
        onResponseCallback_(responseData_);
    }

    return responseData_;
}

auto CurlWrapper::Impl::performAsync() -> CurlWrapper::Impl & {
    spdlog::info("Performing asynchronous request");
    {
        std::unique_lock lock(mutex_);
        if (asyncRunning_) {
            spdlog::info(
                "Previous asynchronous request still running, waiting");
            cv_.wait(lock, [this]() { return !asyncRunning_; });
        }

        responseData_.clear();
        responseData_.reserve(4096);

        curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &responseData_);

        CURLMcode multiCode = curl_multi_add_handle(multiHandle_, handle_);
        if (multiCode != CURLM_OK) {
            spdlog::error("curl_multi_add_handle failed: {}",
                          curl_multi_strerror(multiCode));
            THROW_CURL_RUNTIME_ERROR("Failed to add handle to multi handle.");
        }

        asyncRunning_ = true;
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    worker_ = std::thread([this]() {
        int stillRunning = 0;
        curl_multi_perform(multiHandle_, &stillRunning);

        while (stillRunning > 0) {
            int numfds;
            CURLMcode multiCode =
                curl_multi_wait(multiHandle_, nullptr, 0, TIMEOUT_MS, &numfds);
            if (multiCode != CURLM_OK) {
                spdlog::error("curl_multi_wait failed: {}",
                              curl_multi_strerror(multiCode));
                break;
            }
            curl_multi_perform(multiHandle_, &stillRunning);
        }

        CURLMsg *msg;
        int msgsLeft;
        while ((msg = curl_multi_info_read(multiHandle_, &msgsLeft)) !=
               nullptr) {
            if (msg->msg == CURLMSG_DONE) {
                CURL *easyHandle = msg->easy_handle;
                char *url = nullptr;
                curl_easy_getinfo(easyHandle, CURLINFO_EFFECTIVE_URL, &url);
                spdlog::info("Completed request: {}", url ? url : "unknown");

                if (msg->data.result != CURLE_OK) {
                    spdlog::error("Async request failed: {}",
                                  curl_easy_strerror(msg->data.result));
                    if (onErrorCallback_) {
                        onErrorCallback_(msg->data.result);
                    }
                } else {
                    if (onResponseCallback_) {
                        onResponseCallback_(responseData_);
                    }
                }

                curl_multi_remove_handle(multiHandle_, easyHandle);
            }
        }

        {
            std::lock_guard lock(mutex_);
            asyncRunning_ = false;
        }
        cv_.notify_one();
    });

    return *this;
}

void CurlWrapper::Impl::waitAll() {
    spdlog::info("Waiting for all asynchronous requests to complete");
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]() { return !asyncRunning_; });
    }
    spdlog::info("All asynchronous requests completed");
    if (worker_.joinable()) {
        worker_.join();
    }
}

auto CurlWrapper::Impl::writeCallback(void *contents, size_t size, size_t nmemb,
                                      void *userp) -> size_t {
    size_t totalSize = size * nmemb;
    auto *str = static_cast<std::string *>(userp);
    str->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

auto CurlWrapper::Impl::setMaxDownloadSpeed(size_t speed)
    -> CurlWrapper::Impl & {
    spdlog::info("Setting max download speed: {} bytes/sec", speed);
    curl_easy_setopt(handle_, CURLOPT_MAX_RECV_SPEED_LARGE,
                     static_cast<curl_off_t>(speed));
    return *this;
}

auto CurlWrapper::Impl::setMaxUploadSpeed(size_t speed) -> CurlWrapper::Impl & {
    spdlog::info("Setting max upload speed: {} bytes/sec", speed);
    curl_easy_setopt(handle_, CURLOPT_MAX_SEND_SPEED_LARGE,
                     static_cast<curl_off_t>(speed));
    return *this;
}

void CurlWrapper::Impl::setProgressCallback(
    std::function<void(size_t, size_t, size_t, size_t)> callback) {
    progressCallback_ = std::move(callback);
    if (progressCallback_) {
        curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION,
                         progressCallbackWrapper);
        curl_easy_setopt(handle_, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 0L);
    } else {
        curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 1L);
    }
}

auto CurlWrapper::Impl::progressCallbackWrapper(void *clientp,
                                                curl_off_t dltotal,
                                                curl_off_t dlnow,
                                                curl_off_t ultotal,
                                                curl_off_t ulnow) -> int {
    auto *impl = static_cast<CurlWrapper::Impl *>(clientp);
    if (impl && impl->progressCallback_) {
        impl->progressCallback_(
            static_cast<size_t>(dlnow), static_cast<size_t>(dltotal),
            static_cast<size_t>(ulnow), static_cast<size_t>(ultotal));
    }
    return 0;
}

void CurlWrapper::Impl::applyConfig(const RequestConfig &config) {
    setTimeout(static_cast<long>(config.timeout.count()));
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT,
                     static_cast<long>(config.connectTimeout.count()));
    setFollowLocation(config.followRedirects);
    if (config.followRedirects) {
        curl_easy_setopt(handle_, CURLOPT_MAXREDIRS,
                         static_cast<long>(config.maxRedirects));
    }
    setSSLOptions(config.verifySSL, config.verifyHost);

    if (!config.userAgent.empty()) {
        curl_easy_setopt(handle_, CURLOPT_USERAGENT, config.userAgent.c_str());
    }

    if (!config.proxy.empty()) {
        setProxy(config.proxy);
    }

    if (config.basicAuth.has_value()) {
        std::string auth =
            config.basicAuth->first + ":" + config.basicAuth->second;
        curl_easy_setopt(handle_, CURLOPT_USERPWD, auth.c_str());
    }

    if (config.bearerToken.has_value()) {
        addHeader("Authorization", "Bearer " + *config.bearerToken);
    }
}

auto CurlWrapper::Impl::getResponseCode() const -> int {
    long responseCode = 0;
    curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &responseCode);
    return static_cast<int>(responseCode);
}

auto CurlWrapper::Impl::getEffectiveUrl() const -> std::string {
    char *url = nullptr;
    curl_easy_getinfo(handle_, CURLINFO_EFFECTIVE_URL, &url);
    return url ? std::string(url) : "";
}

auto CurlWrapper::Impl::getContentType() const -> std::string {
    char *contentType = nullptr;
    curl_easy_getinfo(handle_, CURLINFO_CONTENT_TYPE, &contentType);
    return contentType ? std::string(contentType) : "";
}

auto CurlWrapper::Impl::getDownloadSize() const -> size_t {
    curl_off_t size = 0;
    curl_easy_getinfo(handle_, CURLINFO_SIZE_DOWNLOAD_T, &size);
    return static_cast<size_t>(size);
}

void CurlWrapper::Impl::reset() {
    waitAll();
    if (headersList_) {
        curl_slist_free_all(headersList_);
        headersList_ = nullptr;
    }
    curl_easy_reset(handle_);
    curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);
    responseData_.clear();
    requestBody_.clear();
    uploadFile_.reset();
    progressCallback_ = nullptr;
    onErrorCallback_ = nullptr;
    onResponseCallback_ = nullptr;
}

auto CurlWrapper::Impl::performAsyncCancellable(std::stop_token stopToken)
    -> std::future<HttpResponse> {
    return std::async(std::launch::async, [this, stopToken]() -> HttpResponse {
        HttpResponse response;
        auto startTime = std::chrono::steady_clock::now();

        try {
            {
                std::lock_guard lock(mutex_);
                responseData_.clear();
                responseData_.reserve(4096);
                curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeCallback);
                curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &responseData_);
            }

            CURLcode res = CURLE_OK;
            int stillRunning = 0;

            CURLMcode multiCode = curl_multi_add_handle(multiHandle_, handle_);
            if (multiCode != CURLM_OK) {
                response.statusMessage = curl_multi_strerror(multiCode);
                return response;
            }

            curl_multi_perform(multiHandle_, &stillRunning);

            while (stillRunning > 0 && !stopToken.stop_requested()) {
                int numfds;
                multiCode =
                    curl_multi_wait(multiHandle_, nullptr, 0, 100, &numfds);
                if (multiCode != CURLM_OK) {
                    break;
                }
                curl_multi_perform(multiHandle_, &stillRunning);
            }

            if (stopToken.stop_requested()) {
                curl_multi_remove_handle(multiHandle_, handle_);
                response.statusMessage = "Request cancelled";
                return response;
            }

            CURLMsg *msg;
            int msgsLeft;
            while ((msg = curl_multi_info_read(multiHandle_, &msgsLeft)) !=
                   nullptr) {
                if (msg->msg == CURLMSG_DONE) {
                    res = msg->data.result;
                    curl_multi_remove_handle(multiHandle_, msg->easy_handle);
                }
            }

            if (res == CURLE_OK) {
                response.body = responseData_;
                response.statusCode = getResponseCode();
                response.effectiveUrl = getEffectiveUrl();
                response.contentType = getContentType();
                response.downloadedBytes = getDownloadSize();
            } else {
                response.statusMessage = curl_easy_strerror(res);
            }

        } catch (const std::exception &e) {
            response.statusMessage = e.what();
        }

        auto endTime = std::chrono::steady_clock::now();
        response.responseTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        return response;
    });
}

}  // namespace atom::web
