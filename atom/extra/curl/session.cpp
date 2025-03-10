#include "session.hpp"
#include <filesystem>

#include "connection_pool.hpp"
#include "error.hpp"
#include "request.hpp"

namespace atom::extra::curl {
Session::Session()
    : connection_pool_(nullptr), cache_(nullptr), rate_limiter_(nullptr) {
    curl_global_init(CURL_GLOBAL_ALL);
    handle_ = curl_easy_init();
    if (!handle_) {
        throw Error(CURLE_FAILED_INIT, "Failed to initialize curl");
    }
}

Session::Session(ConnectionPool* pool)
    : connection_pool_(pool), cache_(nullptr), rate_limiter_(nullptr) {
    curl_global_init(CURL_GLOBAL_ALL);
    handle_ = pool ? pool->acquire() : curl_easy_init();
    if (!handle_) {
        throw Error(CURLE_FAILED_INIT, "Failed to initialize curl");
    }
}

Session::~Session() {
    if (handle_) {
        if (connection_pool_) {
            connection_pool_->release(handle_);
        } else {
            curl_easy_cleanup(handle_);
        }
    }
    curl_global_cleanup();
}

Session::Session(Session&& other) noexcept
    : handle_(other.handle_),
      connection_pool_(other.connection_pool_),
      cache_(other.cache_),
      rate_limiter_(other.rate_limiter_),
      interceptors_(std::move(other.interceptors_)) {
    other.handle_ = nullptr;
    other.connection_pool_ = nullptr;
    other.cache_ = nullptr;
    other.rate_limiter_ = nullptr;
}

Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        if (handle_) {
            if (connection_pool_) {
                connection_pool_->release(handle_);
            } else {
                curl_easy_cleanup(handle_);
            }
        }
        handle_ = other.handle_;
        connection_pool_ = other.connection_pool_;
        cache_ = other.cache_;
        rate_limiter_ = other.rate_limiter_;
        interceptors_ = std::move(other.interceptors_);
        other.handle_ = nullptr;
        other.connection_pool_ = nullptr;
        other.cache_ = nullptr;
        other.rate_limiter_ = nullptr;
    }
    return *this;
}

void Session::add_interceptor(std::shared_ptr<Interceptor> interceptor) {
    interceptors_.push_back(std::move(interceptor));
}

void Session::set_cache(Cache* cache) { cache_ = cache; }

void Session::set_rate_limiter(RateLimiter* limiter) {
    rate_limiter_ = limiter;
}

Response Session::execute(const Request& request) {
    if (cache_ && request.method() == Request::Method::GET) {
        auto cached_response = cache_->get(request.url());
        if (cached_response) {
            return *cached_response;
        }
        auto validation_headers = cache_->get_validation_headers(request.url());
        Request modified_request = request;
        for (const auto& [name, value] : validation_headers) {
            modified_request.header(name, value);
        }

        Response response = execute_internal(modified_request);

        if (response.status_code() == 304) {
            cache_->handle_not_modified(request.url());
            auto cached_response = cache_->get(request.url());
            if (cached_response) {
                return *cached_response;
            }
        } else if (response.ok()) {
            cache_->set(request.url(), response);
        }

        return response;
    }

    return execute_internal(request);
}

std::future<Response> Session::execute_async(const Request& request) {
    return std::async(std::launch::async,
                      [this, request]() { return execute(request); });
}

Response Session::get(std::string_view url) {
    Request req;
    req.method(Request::Method::GET).url(url);
    return execute(req);
}

Response Session::get(std::string_view url,
                      const std::map<std::string, std::string>& params) {
    std::string full_url = std::string(url);

    // 添加查询参数
    if (!params.empty()) {
        full_url += (full_url.find('?') == std::string::npos) ? '?' : '&';

        bool first = true;
        for (const auto& [key, value] : params) {
            if (!first) {
                full_url += '&';
            }
            full_url += url_encode(key) + '=' + url_encode(value);
            first = false;
        }
    }

    return get(full_url);
}

Response Session::post(std::string_view url, std::string_view body,
                       std::string_view content_type) {
    Request req;
    req.method(Request::Method::POST)
        .url(url)
        .body(body)
        .header("Content-Type", content_type);
    return execute(req);
}

Response Session::post_form(std::string_view url,
                            const std::map<std::string, std::string>& params) {
    std::string body;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first) {
            body += '&';
        }
        body += url_encode(key) + '=' + url_encode(value);
        first = false;
    }

    return post(url, body, "application/x-www-form-urlencoded");
}

Response Session::post_json(std::string_view url, std::string_view json) {
    return post(url, json, "application/json");
}

Response Session::put(std::string_view url, std::string_view body,
                      std::string_view content_type) {
    Request req;
    req.method(Request::Method::PUT)
        .url(url)
        .body(body)
        .header("Content-Type", content_type);
    return execute(req);
}

Response Session::del(std::string_view url) {
    Request req;
    req.method(Request::Method::DELETE).url(url);
    return execute(req);
}

Response Session::patch(std::string_view url, std::string_view body,
                        std::string_view content_type) {
    Request req;
    req.method(Request::Method::PATCH)
        .url(url)
        .body(body)
        .header("Content-Type", content_type);
    return execute(req);
}

Response Session::head(std::string_view url) {
    Request req;
    req.method(Request::Method::HEAD).url(url);
    return execute(req);
}

Response Session::options(std::string_view url) {
    Request req;
    req.method(Request::Method::OPTIONS).url(url);
    return execute(req);
}

Response Session::download(std::string_view url, std::string_view filepath,
                           std::optional<curl_off_t> resume_from) {
    Request req;
    req.method(Request::Method::GET).url(url);

    if (resume_from) {
        req.resume_from(*resume_from);
    }

    std::filesystem::create_directories(
        std::filesystem::path(filepath).parent_path());

    FILE* file = nullptr;
    if (resume_from) {
        file = fopen(std::string(filepath).c_str(), "a+b");  // 追加模式
    } else {
        file = fopen(std::string(filepath).c_str(), "wb");  // 写入模式
    }

    if (!file) {
        throw Error(CURLE_WRITE_ERROR, "Failed to open file for writing: " +
                                           std::string(filepath));
    }

    reset();
    setup_request(req);

    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, file_write_callback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, file);

    Response response = perform();
    fclose(file);

    return response;
}

Response Session::upload(std::string_view url, std::string_view filepath,
                         std::string_view field_name,
                         std::optional<curl_off_t> resume_from) {
    MultipartForm form;
    form.add_file(field_name, filepath);

    Request req;
    req.method(Request::Method::POST).url(url).multipart_form(form);

    if (resume_from) {
        req.resume_from(*resume_from);
    }

    return execute(req);
}

void Session::set_progress_callback(
    std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)>
        callback) {
    progress_callback_.callback = std::move(callback);
    curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION,
                     &ProgressCallback::xferinfo);
    curl_easy_setopt(handle_, CURLOPT_XFERINFODATA, &progress_callback_);
    curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 0L);
}

std::string Session::url_encode(std::string_view str) {
    char* encoded =
        curl_easy_escape(nullptr, str.data(), static_cast<int>(str.length()));
    if (!encoded) {
        return std::string(str);
    }

    std::string result(encoded);
    curl_free(encoded);
    return result;
}

std::string Session::url_decode(std::string_view str) {
    char* decoded = curl_easy_unescape(nullptr, str.data(),
                                       static_cast<int>(str.length()), nullptr);
    if (!decoded) {
        return std::string(str);
    }

    std::string result(decoded);
    curl_free(decoded);
    return result;
}

std::shared_ptr<WebSocket> Session::create_websocket(
    const std::string& url, const std::map<std::string, std::string>& headers) {
    auto ws = std::make_shared<WebSocket>();
    if (ws->connect(url, headers)) {
        return ws;
    }
    return nullptr;
}

int Session::ProgressCallback::xferinfo(void* clientp, curl_off_t dltotal,
                                        curl_off_t dlnow, curl_off_t ultotal,
                                        curl_off_t ulnow) {
    auto* callback = static_cast<ProgressCallback*>(clientp);
    return callback->callback(dltotal, dlnow, ultotal, ulnow);
};

void Session::reset() {
    curl_easy_reset(handle_);
    response_body_.clear();
    response_headers_.clear();
    error_buffer_[0] = 0;
}

void Session::setup_request(const Request& request) {
    for (const auto& interceptor : interceptors_) {
        interceptor->before_request(handle_, request);
    }

    for (const auto& interceptor : request.interceptors()) {
        interceptor->before_request(handle_, request);
    }

    curl_easy_setopt(handle_, CURLOPT_URL, request.url().c_str());

    curl_easy_setopt(handle_, CURLOPT_ERRORBUFFER, error_buffer_);

    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &response_body_);
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, &response_headers_);

    switch (request.method()) {
        case Request::Method::GET:
            curl_easy_setopt(handle_, CURLOPT_HTTPGET, 1L);
            break;
        case Request::Method::POST:
            curl_easy_setopt(handle_, CURLOPT_POST, 1L);
            if (!request.body().empty()) {
                curl_easy_setopt(handle_, CURLOPT_POSTFIELDS,
                                 request.body().data());
                curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE,
                                 request.body().size());
            }
            break;
        case Request::Method::PUT:
            curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!request.body().empty()) {
                curl_easy_setopt(handle_, CURLOPT_POSTFIELDS,
                                 request.body().data());
                curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE,
                                 request.body().size());
            }
            break;
        case Request::Method::DELETE:
            curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case Request::Method::PATCH:
            curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "PATCH");
            if (!request.body().empty()) {
                curl_easy_setopt(handle_, CURLOPT_POSTFIELDS,
                                 request.body().data());
                curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE,
                                 request.body().size());
            }
            break;
        case Request::Method::HEAD:
            curl_easy_setopt(handle_, CURLOPT_NOBODY, 1L);
            break;
        case Request::Method::OPTIONS:
            curl_easy_setopt(handle_, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
    }

    struct curl_slist* headers = nullptr;
    for (const auto& [name, value] : request.headers()) {
        std::string header = name + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    if (headers) {
        curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers);
    }

    if (request.timeout()) {
        curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS,
                         request.timeout()->count());
    }

    if (request.connection_timeout()) {
        curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS,
                         request.connection_timeout()->count());
    }

    curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION,
                     request.follow_redirects() ? 1L : 0L);
    if (request.max_redirects()) {
        curl_easy_setopt(handle_, CURLOPT_MAXREDIRS, *request.max_redirects());
    }

    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER,
                     request.verify_ssl() ? 1L : 0L);
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST,
                     request.verify_ssl() ? 2L : 0L);

    if (request.ca_path()) {
        curl_easy_setopt(handle_, CURLOPT_CAPATH, request.ca_path()->c_str());
    }

    if (request.ca_info()) {
        curl_easy_setopt(handle_, CURLOPT_CAINFO, request.ca_info()->c_str());
    }

    if (request.client_cert() && request.client_key()) {
        curl_easy_setopt(handle_, CURLOPT_SSLCERT,
                         request.client_cert()->c_str());
        curl_easy_setopt(handle_, CURLOPT_SSLKEY,
                         request.client_key()->c_str());
    }

    if (request.proxy()) {
        curl_easy_setopt(handle_, CURLOPT_PROXY, request.proxy()->c_str());

        if (request.proxy_type()) {
            curl_easy_setopt(handle_, CURLOPT_PROXYTYPE, *request.proxy_type());
        }

        if (request.proxy_username() && request.proxy_password()) {
            curl_easy_setopt(handle_, CURLOPT_PROXYUSERNAME,
                             request.proxy_username()->c_str());
            curl_easy_setopt(handle_, CURLOPT_PROXYPASSWORD,
                             request.proxy_password()->c_str());
        }
    }

    if (request.username() && request.password()) {
        curl_easy_setopt(handle_, CURLOPT_USERNAME,
                         request.username()->c_str());
        curl_easy_setopt(handle_, CURLOPT_PASSWORD,
                         request.password()->c_str());
    }

    if (request.form()) {
        curl_easy_setopt(handle_, CURLOPT_MIMEPOST, request.form());
    }

    // Cookie
    for (const auto& cookie : request.cookies()) {
        curl_easy_setopt(handle_, CURLOPT_COOKIE, cookie.to_string().c_str());
    }

    if (request.user_agent()) {
        curl_easy_setopt(handle_, CURLOPT_USERAGENT,
                         request.user_agent()->c_str());
    }

    if (request.accept_encoding()) {
        curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING,
                         request.accept_encoding()->c_str());
    }

    if (request.low_speed_limit() && request.low_speed_time()) {
        curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_LIMIT,
                         *request.low_speed_limit());
        curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_TIME,
                         *request.low_speed_time());
    }

    if (request.resume_from()) {
        curl_easy_setopt(handle_, CURLOPT_RESUME_FROM_LARGE,
                         *request.resume_from());
    }

    if (request.http_version()) {
        curl_easy_setopt(handle_, CURLOPT_HTTP_VERSION,
                         *request.http_version());
    }
}

Response Session::execute_internal(const Request& request) {
    if (rate_limiter_) {
        rate_limiter_->wait();
    }

    int retries_left = request.retries();
    while (true) {
        try {
            reset();
            setup_request(request);
            Response response = perform();

            if (request.cookie_jar()) {
                std::string domain;
                {
                    CURLU* url_handle = curl_url();
                    curl_url_set(url_handle, CURLUPART_URL,
                                 request.url().c_str(), 0);
                    char* host;
                    curl_url_get(url_handle, CURLUPART_HOST, &host, 0);
                    if (host) {
                        domain = host;
                        curl_free(host);
                    }
                    curl_url_cleanup(url_handle);
                }

                request.cookie_jar()->parse_cookies_from_headers(
                    response.headers(), domain);
            }

            for (const auto& interceptor : interceptors_) {
                interceptor->after_response(handle_, request, response);
            }

            for (const auto& interceptor : request.interceptors()) {
                interceptor->after_response(handle_, request, response);
            }

            return response;
        } catch (const Error& e) {
            if (retries_left > 0 && request.retry_on_error()) {
                retries_left--;
                std::this_thread::sleep_for(request.retry_delay());
                continue;
            }
            throw;
        }
    }
}

Response Session::perform() {
    CURLcode res = curl_easy_perform(handle_);
    if (res != CURLE_OK) {
        std::string error_msg =
            error_buffer_[0] ? error_buffer_ : curl_easy_strerror(res);
        throw Error(res, error_msg);
    }

    long status_code = 0;
    curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &status_code);

    return Response(status_code, std::move(response_body_),
                    std::move(response_headers_));
}

size_t Session::write_callback(char* ptr, size_t size, size_t nmemb,
                               void* userdata) {
    size_t realsize = size * nmemb;
    auto* body = static_cast<std::vector<char>*>(userdata);

    size_t current_size = body->size();
    body->resize(current_size + realsize);
    std::memcpy(body->data() + current_size, ptr, realsize);

    return realsize;
}

size_t Session::header_callback(char* buffer, size_t size, size_t nitems,
                                void* userdata) {
    size_t realsize = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);

    std::string header(buffer, realsize);
    size_t pos = header.find(':');
    if (pos != std::string::npos) {
        std::string name = header.substr(0, pos);
        std::string value = header.substr(pos + 1);

        // 修剪空白
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t\r\n") + 1);

        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        (*headers)[name] = value;
    }

    return realsize;
}

size_t Session::file_write_callback(char* ptr, size_t size, size_t nmemb,
                                    void* userdata) {
    size_t written = fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
    return written;
}
};  // namespace atom::extra::curl
