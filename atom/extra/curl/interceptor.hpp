#ifndef ATOM_EXTRA_CURL_INTERCEPTOR_HPP
#define ATOM_EXTRA_CURL_INTERCEPTOR_HPP

#include <curl/curl.h>

class Request;
class Response;

class Interceptor {
public:
    virtual ~Interceptor() = default;

    // 请求前拦截
    virtual void before_request(CURL* handle, const Request& request) = 0;

    // 响应后拦截
    virtual void after_response(CURL* handle, const Request& request,
                                const Response& response) = 0;
};

#endif