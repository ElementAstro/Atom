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

#include <condition_variable>
#include <fstream>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>

#include "atom/error/exception.hpp"

namespace atom::web {

constexpr long TIMEOUT_MS = 1000;

class CurlWrapper::Impl {
public:
    Impl();
    ~Impl();

    auto setUrl(const std::string &url) -> CurlWrapper::Impl &;
    auto setRequestMethod(const std::string &method) -> CurlWrapper::Impl &;
    auto addHeader(const std::string &key, const std::string &value)
        -> CurlWrapper::Impl &;
    auto setOnErrorCallback(std::function<void(CURLcode)> callback)
        -> CurlWrapper::Impl &;
    auto setOnResponseCallback(
        std::function<void(const std::string &)> callback)
        -> CurlWrapper::Impl &;
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

    static auto writeCallback(void *contents, size_t size, size_t nmemb,
                              void *userp) -> size_t;
    static auto readCallback(void *ptr, size_t size, size_t nmemb, void *userp)
        -> size_t;
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

auto CurlWrapper::addHeader(const std::string &key, const std::string &value)
    -> CurlWrapper & {
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

auto CurlWrapper::setSSLOptions(bool verifyPeer, bool verifyHost)
    -> CurlWrapper & {
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

CurlWrapper::Impl::Impl()
    : multiHandle_(curl_multi_init()), headersList_(nullptr) {
    spdlog::info("CurlWrapper::Impl constructor called");
    curl_global_init(CURL_GLOBAL_ALL);
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
    if (headersList_) {
        curl_slist_free_all(headersList_);
    }
    curl_easy_cleanup(handle_);
    curl_multi_cleanup(multiHandle_);
    curl_global_cleanup();
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

auto CurlWrapper::Impl::addHeader(const std::string &key,
                                  const std::string &value)
    -> CurlWrapper::Impl & {
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

auto CurlWrapper::Impl::setSSLOptions(bool verifyPeer, bool verifyHost)
    -> CurlWrapper::Impl & {
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
    std::lock_guard lock(mutex_);
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

    std::thread([this]() {
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

        cv_.notify_one();
    }).detach();

    return *this;
}

void CurlWrapper::Impl::waitAll() {
    spdlog::info("Waiting for all asynchronous requests to complete");
    std::unique_lock lock(mutex_);
    cv_.wait(lock);
    spdlog::info("All asynchronous requests completed");
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

}  // namespace atom::web
