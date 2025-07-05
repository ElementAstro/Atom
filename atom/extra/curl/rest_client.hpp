#ifndef ATOM_EXTRA_CURL_REST_CLIENT_HPP
#define ATOM_EXTRA_CURL_REST_CLIENT_HPP

#include <coroutine>
#include <iostream>
#include "error.hpp"
#include "interceptor.hpp"
#include "request.hpp"
#include "response.hpp"
#include "session.hpp"

namespace atom::extra::curl {
/**
 * @brief Performs an HTTP GET request to the specified URL.
 *
 * This function uses a thread-local Session object to execute the GET request.
 *
 * @param url The URL to send the GET request to.
 * @return A Response object containing the server's response.
 */
inline Response get(std::string_view url) {
    static thread_local Session session;
    return session.get(url);
}

/**
 * @brief Performs an HTTP POST request to the specified URL with the given
 * body.
 *
 * This function uses a thread-local Session object to execute the POST request.
 *
 * @param url The URL to send the POST request to.
 * @param body The body of the POST request.
 * @param content_type The content type of the body (default is
 * "application/json").
 * @return A Response object containing the server's response.
 */
inline Response post(std::string_view url, std::string_view body,
                     std::string_view content_type = "application/json") {
    static thread_local Session session;
    return session.post(url, body, content_type);
}

/**
 * @brief Performs an HTTP PUT request to the specified URL with the given body.
 *
 * This function uses a thread-local Session object to execute the PUT request.
 *
 * @param url The URL to send the PUT request to.
 * @param body The body of the PUT request.
 * @param content_type The content type of the body (default is
 * "application/json").
 * @return A Response object containing the server's response.
 */
inline Response put(std::string_view url, std::string_view body,
                    std::string_view content_type = "application/json") {
    static thread_local Session session;
    return session.put(url, body, content_type);
}

/**
 * @brief Performs an HTTP DELETE request to the specified URL.
 *
 * This function uses a thread-local Session object to execute the DELETE
 * request.
 *
 * @param url The URL to send the DELETE request to.
 * @return A Response object containing the server's response.
 */
inline Response del(std::string_view url) {
    static thread_local Session session;
    return session.del(url);
}

/**
 * @brief Concept that checks if a type can handle a Response object.
 *
 * This concept is satisfied if the type `T` can be called with a `Response`
 * object.
 */
template <typename T>
concept ResponseHandler = requires(T t, const Response& response) {
    { t(response) };
};

/**
 * @brief Concept that checks if a type can handle an Error object.
 *
 * This concept is satisfied if the type `T` can be called with an `Error`
 * object.
 */
template <typename T>
concept ErrorHandler = requires(T t, const Error& error) {
    { t(error) };
};

/**
 * @brief Executes an HTTP request and calls the provided handlers based on the
 * result.
 *
 * This function executes the given request using a Session object. If the
 * request is successful, the `on_success` handler is called with the Response.
 * If an error occurs, the `on_error` handler is called with the Error.
 *
 * @param request The Request object to execute.
 * @param on_success The handler to call on a successful response.
 * @param on_error The handler to call on an error.
 */
template <ResponseHandler OnSuccess, ErrorHandler OnError>
void fetch(const Request& request, OnSuccess&& on_success, OnError&& on_error) {
    try {
        Session session;
        Response response = session.execute(request);
        on_success(response);
    } catch (const Error& error) {
        on_error(error);
    }
}

/**
 * @brief A simple Task class for coroutine support.
 *
 * This class allows asynchronous operations to be performed using coroutines.
 * @tparam T The type of the result that the Task will return.
 */
template <typename T>
class Task {
public:
    /**
     * @brief Promise type for the Task coroutine.
     */
    struct promise_type {
        T result;
        std::exception_ptr exception;

        /**
         * @brief Returns the Task object associated with this promise.
         * @return A Task object.
         */
        Task get_return_object() {
            return Task(
                std::coroutine_handle<promise_type>::from_promise(*this));
        }

        /**
         * @brief Determines the initial suspension behavior of the coroutine.
         * @return std::suspend_never to indicate that the coroutine should not
         * suspend initially.
         */
        std::suspend_never initial_suspend() noexcept { return {}; }

        /**
         * @brief Determines the final suspension behavior of the coroutine.
         * @return std::suspend_always to indicate that the coroutine should
         * always suspend upon completion.
         */
        std::suspend_always final_suspend() noexcept { return {}; }

        /**
         * @brief Sets the return value of the coroutine.
         * @param value The value to return.
         */
        void return_value(T value) { result = std::move(value); }

        /**
         * @brief Handles uncaught exceptions within the coroutine.
         */
        void unhandled_exception() { exception = std::current_exception(); }
    };

    /**
     * @brief Constructs a Task from a coroutine handle.
     * @param handle The coroutine handle.
     */
    explicit Task(std::coroutine_handle<promise_type> handle)
        : handle_(handle) {}

    /**
     * @brief Destroys the Task and releases the coroutine handle.
     */
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    /**
     * @brief Retrieves the result of the coroutine.
     *
     * If an exception occurred during the coroutine's execution, it is
     * rethrown.
     * @return The result of the coroutine.
     * @throws std::exception_ptr if an exception occurred within the coroutine.
     */
    T result() const {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return handle_.promise().result;
    }

private:
    std::coroutine_handle<promise_type> handle_;
};

/**
 * @brief Awaitable struct for performing HTTP requests within coroutines.
 *
 * This struct allows you to `co_await` an HTTP request, suspending the
 * coroutine until the request is complete.
 */
struct Awaitable {
    Request request;
    Response response;
    Error* error = nullptr;

    /**
     * @brief Constructs an Awaitable object with the given request.
     * @param req The HTTP request to perform.
     */
    Awaitable(Request req) : request(std::move(req)), response(0, {}, {}) {}

    /**
     * @brief Determines if the awaitable is ready to resume immediately.
     * @return Always returns false, as the request is never immediately ready.
     */
    bool await_ready() const noexcept { return false; }

    /**
     * @brief Suspends the coroutine and performs the HTTP request.
     *
     * This method is called when the coroutine is suspended. It executes the
     * HTTP request using a Session object and resumes the coroutine when the
     * request is complete. If an error occurs, it stores the error and resumes
     * the coroutine.
     *
     * @param handle The coroutine handle to resume.
     */
    void await_suspend(std::coroutine_handle<> handle) {
        try {
            Session session;
            response = session.execute(request);
            handle.resume();
        } catch (const Error& e) {
            error = new Error(e);
            handle.resume();
        } catch (...) {
            handle.resume();
            throw;
        }
    }

    /**
     * @brief Resumes the coroutine and returns the HTTP response.
     *
     * This method is called when the coroutine is resumed. It returns the HTTP
     * response if the request was successful. If an error occurred, it throws
     * the error.
     *
     * @return The HTTP response.
     * @throws Error if an error occurred during the request.
     */
    Response await_resume() {
        if (error) {
            Error e = *error;
            delete error;
            throw e;
        }
        return response;
    }
};

/**
 * @brief Creates an Awaitable object for the given request.
 * @param request The HTTP request to perform.
 * @return An Awaitable object.
 */
inline Awaitable fetch(Request request) {
    return Awaitable(std::move(request));
}

/**
 * @brief Asynchronously performs an HTTP request using coroutines.
 *
 * This function allows you to perform an HTTP request asynchronously using
 * coroutines. It returns a Task object that represents the asynchronous
 * operation.
 *
 * @param request The HTTP request to perform.
 * @return A Task object that represents the asynchronous operation.
 * @throws Error if an error occurred during the request.
 */
inline Task<Response> fetch_async(Request request) {
    try {
        Response response = co_await fetch(std::move(request));
        co_return response;
    } catch (const Error& e) {
        throw e;
    }
}

/**
 * @brief An interceptor that logs HTTP requests and responses to an output
 * stream.
 */
class LoggingInterceptor : public Interceptor {
public:
    /**
     * @brief Constructs a LoggingInterceptor with the given output stream.
     * @param out The output stream to log to (default is std::cout).
     */
    explicit LoggingInterceptor(std::ostream& out = std::cout) : out_(out) {}

    /**
     * @brief Logs the HTTP request before it is sent.
     *
     * This method is called before the HTTP request is sent. It logs the
     * request method, URL, headers, and body to the output stream.
     *
     * @param handle The CURL handle.
     * @param request The HTTP request.
     */
    void before_request([[maybe_unused]] CURL* handle,
                        const Request& request) override {
        out_ << "Request: " << to_string(request.method()) << " "
             << request.url() << std::endl;

        for (const auto& [name, value] : request.headers()) {
            out_ << "  " << name << ": " << value << std::endl;
        }

        if (!request.body().empty()) {
            out_ << "  Body: "
                 << std::string(request.body().data(),
                                std::min(request.body().size(), size_t(100)))
                 << (request.body().size() > 100 ? "..." : "") << std::endl;
        }
    }

    /**
     * @brief Logs the HTTP response after it is received.
     *
     * This method is called after the HTTP response is received. It logs the
     * response status code, headers, and body to the output stream.
     *
     * @param handle The CURL handle.
     * @param request The HTTP request.
     * @param response The HTTP response.
     */
    void after_response([[maybe_unused]] CURL* handle,
                        [[maybe_unused]] const Request& request,
                        const Response& response) override {
        out_ << "Response: " << response.status_code() << std::endl;

        for (const auto& [name, value] : response.headers()) {
            out_ << "  " << name << ": " << value << std::endl;
        }

        if (!response.body().empty()) {
            out_ << "  Body: "
                 << std::string(response.body().data(),
                                std::min(response.body().size(), size_t(100)))
                 << (response.body().size() > 100 ? "..." : "") << std::endl;
        }
    }

private:
    std::ostream& out_;

    /**
     * @brief Converts a Request::Method enum to a string.
     * @param method The Request::Method enum value.
     * @return The string representation of the method.
     */
    static std::string to_string(Request::Method method) {
        switch (method) {
            case Request::Method::GET:
                return "GET";
            case Request::Method::POST:
                return "POST";
            case Request::Method::PUT:
                return "PUT";
            case Request::Method::DELETE:
                return "DELETE";
            case Request::Method::PATCH:
                return "PATCH";
            case Request::Method::HEAD:
                return "HEAD";
            case Request::Method::OPTIONS:
                return "OPTIONS";
            default:
                return "UNKNOWN";
        }
    }
};

/**
 * @brief A REST client that simplifies making HTTP requests to a RESTful API.
 *
 * This class provides a convenient way to make HTTP requests to a RESTful API.
 * It handles setting the base URL, default headers, caching, and rate limiting.
 */
class RestClient {
public:
    /**
     * @brief Constructs a RestClient with the given base URL.
     * @param base_url The base URL of the RESTful API.
     */
    RestClient(std::string base_url) : base_url_(std::move(base_url)) {
        session_.set_cache(&cache_);
        session_.set_rate_limiter(&rate_limiter_);
        session_.add_interceptor(std::make_shared<LoggingInterceptor>());
    }

    /**
     * @brief Performs an HTTP GET request to the specified path.
     * @param path The path to append to the base URL.
     * @param params The query parameters to add to the URL.
     * @return A Response object containing the server's response.
     */
    Response get(std::string_view path,
                 const std::map<std::string, std::string>& params = {}) {
        return session_.get(make_url(path), params);
    }

    /**
     * @brief Performs an HTTP POST request to the specified path with the given
     * JSON body.
     * @param path The path to append to the base URL.
     * @param json The JSON body of the POST request.
     * @return A Response object containing the server's response.
     */
    Response post(std::string_view path, std::string_view json) {
        return session_.post(make_url(path), json);
    }

    /**
     * @brief Performs an HTTP PUT request to the specified path with the given
     * JSON body.
     * @param path The path to append to the base URL.
     * @param json The JSON body of the PUT request.
     * @return A Response object containing the server's response.
     */
    Response put(std::string_view path, std::string_view json) {
        return session_.put(make_url(path), json);
    }

    /**
     * @brief Performs an HTTP DELETE request to the specified path.
     * @param path The path to append to the base URL.
     * @return A Response object containing the server's response.
     */
    Response del(std::string_view path) { return session_.del(make_url(path)); }

    /**
     * @brief Sets a default header to be included in all requests.
     * @param name The name of the header.
     * @param value The value of the header.
     */
    void set_header(std::string_view name, std::string_view value) {
        default_headers_[std::string(name)] = std::string(value);
    }

    /**
     * @brief Sets the authorization token to be included in the "Authorization"
     * header.
     * @param token The authorization token.
     */
    void set_auth_token(std::string_view token) {
        set_header("Authorization", "Bearer " + std::string(token));
    }

    /**
     * @brief Sets the rate limit for the client.
     * @param requests_per_second The maximum number of requests per second.
     */
    void set_rate_limit(double requests_per_second) {
        rate_limiter_.set_rate(requests_per_second);
    }

    /**
     * @brief Clears the cache.
     */
    void clear_cache() { cache_.clear(); }

private:
    std::string base_url_;
    Session session_;
    std::map<std::string, std::string> default_headers_;
    Cache cache_;
    RateLimiter rate_limiter_{10.0};  // 默认每秒10个请求

    /**
     * @brief Creates a full URL by combining the base URL and the given path.
     * @param path The path to append to the base URL.
     * @return The full URL.
     */
    std::string make_url(std::string_view path) {
        if (path.empty()) {
            return base_url_;
        }

        if (path[0] == '/') {
            return base_url_ + std::string(path);
        } else {
            return base_url_ + "/" + std::string(path);
        }
    }
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_REST_CLIENT_HPP
