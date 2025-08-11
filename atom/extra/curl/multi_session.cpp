#include "multi_session.hpp"

#include <cstring>

namespace atom::extra::curl {
MultiSession::MultiSession() : multi_handle_(curl_multi_init()) {
    if (!multi_handle_) {
        throw Error(CURLE_FAILED_INIT,
                    "Failed to initialize curl multi handle");
    }
}

MultiSession::~MultiSession() {
    if (multi_handle_) {
        curl_multi_cleanup(multi_handle_);
    }
}

// 添加请求
void MultiSession::add_request(
    const Request& request, std::function<void(Response)> callback,
    std::function<void(const Error&)> error_callback) {
    CURL* handle = curl_easy_init();
    if (!handle) {
        throw Error(CURLE_FAILED_INIT, "Failed to initialize curl handle");
    }

    // 为每个请求创建一个上下文
    auto context = std::make_shared<RequestContext>();
    context->request = request;
    context->callback = std::move(callback);
    context->error_callback = std::move(error_callback);
    context->handle = handle;

    // 设置请求选项，和Session类似
    setup_request(request, context.get());

    // 添加到multi handle
    CURLMcode mc = curl_multi_add_handle(multi_handle_, handle);
    if (mc != CURLM_OK) {
        curl_easy_cleanup(handle);
        throw Error(mc, "Failed to add handle to multi session");
    }

    handles_[handle] = context;
}

// 执行所有请求并等待完成
void MultiSession::perform() {
    int still_running = 0;
    CURLMcode mc = curl_multi_perform(multi_handle_, &still_running);

    if (mc != CURLM_OK && mc != CURLM_CALL_MULTI_PERFORM) {
        throw Error(mc, "curl_multi_perform failed");
    }

    // 当仍有请求在进行时进行循环
    while (still_running) {
        // 设置超时时间
        int numfds = 0;
        mc = curl_multi_wait(multi_handle_, nullptr, 0, 1000, &numfds);

        if (mc != CURLM_OK) {
            throw Error(mc, "curl_multi_wait failed");
        }

        // 继续执行
        mc = curl_multi_perform(multi_handle_, &still_running);

        if (mc != CURLM_OK && mc != CURLM_CALL_MULTI_PERFORM) {
            throw Error(mc, "curl_multi_perform failed");
        }

        // 检查是否有已完成的传输
        check_multi_info();
    }

    // 确保处理所有消息
    check_multi_info();
}

void MultiSession::setup_request(const Request& request,
                                 RequestContext* context) {
    CURL* handle = context->handle;

    // 设置URL
    curl_easy_setopt(handle, CURLOPT_URL, request.url().c_str());

    // 设置错误缓冲区
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, context->error_buffer);

    // 设置回调函数
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &context->response_body);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &context->response_headers);

    // 设置私有指针
    curl_easy_setopt(handle, CURLOPT_PRIVATE, context);

    // 设置HTTP方法
    switch (request.method()) {
        case Request::Method::GET:
            curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
            break;
        case Request::Method::POST:
            curl_easy_setopt(handle, CURLOPT_POST, 1L);
            if (!request.body().empty()) {
                curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                                 request.body().data());
                curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                                 request.body().size());
            }
            break;
        case Request::Method::PUT:
            curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!request.body().empty()) {
                curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                                 request.body().data());
                curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                                 request.body().size());
            }
            break;
        case Request::Method::DELETE:
            curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case Request::Method::PATCH:
            curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PATCH");
            if (!request.body().empty()) {
                curl_easy_setopt(handle, CURLOPT_POSTFIELDS,
                                 request.body().data());
                curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                                 request.body().size());
            }
            break;
        case Request::Method::HEAD:
            curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
            break;
        case Request::Method::OPTIONS:
            curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
    }

    // 设置请求头
    context->headers = nullptr;
    for (const auto& [name, value] : request.headers()) {
        std::string header = name + ": " + value;
        context->headers = curl_slist_append(context->headers, header.c_str());
    }

    if (context->headers) {
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, context->headers);
    }

    // 设置超时
    if (request.timeout()) {
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS,
                         request.timeout()->count());
    }

    // 设置连接超时
    if (request.connection_timeout()) {
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS,
                         request.connection_timeout()->count());
    }

    // 设置重定向
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION,
                     request.follow_redirects() ? 1L : 0L);
    if (request.max_redirects()) {
        curl_easy_setopt(handle, CURLOPT_MAXREDIRS, *request.max_redirects());
    }

    // 设置SSL验证
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER,
                     request.verify_ssl() ? 1L : 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST,
                     request.verify_ssl() ? 2L : 0L);

    // 其他选项与Session类似
}

void MultiSession::check_multi_info() {
    CURLMsg* msg = nullptr;
    int msgs_left = 0;

    while ((msg = curl_multi_info_read(multi_handle_, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL* handle = msg->easy_handle;
            CURLcode result = msg->data.result;

            // 查找请求上下文
            auto it = handles_.find(handle);
            if (it != handles_.end()) {
                auto context = it->second;

                try {
                    if (result != CURLE_OK) {
                        throw Error(result, context->error_buffer[0]
                                                ? context->error_buffer
                                                : curl_easy_strerror(result));
                    }

                    long status_code = 0;
                    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE,
                                      &status_code);

                    Response response(status_code,
                                      std::move(context->response_body),
                                      std::move(context->response_headers));

                    // 调用回调
                    if (context->callback) {
                        context->callback(response);
                    }
                } catch (const Error& e) {
                    if (context->error_callback) {
                        context->error_callback(e);
                    }
                }

                // 清理
                if (context->headers) {
                    curl_slist_free_all(context->headers);
                }

                // 从multi handle中移除
                curl_multi_remove_handle(multi_handle_, handle);
                curl_easy_cleanup(handle);
                handles_.erase(it);
            }
        }
    }
}

size_t MultiSession::write_callback(char* ptr, size_t size, size_t nmemb,
                                    void* userdata) {
    size_t realsize = size * nmemb;
    auto* body = static_cast<std::vector<char>*>(userdata);

    size_t current_size = body->size();
    body->resize(current_size + realsize);
    std::memcpy(body->data() + current_size, ptr, realsize);

    return realsize;
}

size_t MultiSession::header_callback(char* buffer, size_t size, size_t nitems,
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
}  // namespace atom::extra::curl
