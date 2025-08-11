#include "atom/extra/beast/http.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;
using json = nlohmann::json;

PYBIND11_MODULE(http, m) {
    m.doc() = "HTTP client module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const boost::system::system_error& e) {
            PyErr_SetString(PyExc_ConnectionError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // HTTP verb enum
    py::enum_<http::verb>(m, "HttpVerb",
                          R"(HTTP request method verbs.

Enum representing standard HTTP request methods.
)")
        .value("GET", http::verb::get, "HTTP GET method")
        .value("POST", http::verb::post, "HTTP POST method")
        .value("PUT", http::verb::put, "HTTP PUT method")
        .value("DELETE", http::verb::delete_, "HTTP DELETE method")
        .value("HEAD", http::verb::head, "HTTP HEAD method")
        .value("OPTIONS", http::verb::options, "HTTP OPTIONS method")
        .value("PATCH", http::verb::patch, "HTTP PATCH method")
        .value("CONNECT", http::verb::connect, "HTTP CONNECT method")
        .value("TRACE", http::verb::trace, "HTTP TRACE method");

    // HttpClient class binding
    py::class_<HttpClient, std::shared_ptr<HttpClient>>(
        m, "HttpClient",
        R"(HTTP client for making HTTP requests.

This class provides methods to send HTTP requests and receive responses.
It supports synchronous and asynchronous operations, as well as JSON
handling, file uploads and downloads, and more.

Args:
    io_context: The I/O context to use for asynchronous operations.

Examples:
    >>> from atom.http import HttpClient, HttpVerb
    >>> import asyncio
    >>>
    >>> # Synchronous request
    >>> client = HttpClient()
    >>> response = client.request(HttpVerb.GET, "example.com", "80", "/")
    >>> print(response.body())
    >>>
    >>> # JSON request
    >>> json_response = client.json_request(HttpVerb.POST, "api.example.com",
    >>>                                    "443", "/data", {"key": "value"})
    >>> print(json_response)
)")
        .def(py::init<net::io_context&>(), py::arg("io_context") = py::none(),
             R"(Constructs an HttpClient with an optional I/O context.

If no I/O context is provided, a default one is created internally.

Args:
    io_context: The I/O context to use for asynchronous operations.
)")
        .def("set_default_header", &HttpClient::setDefaultHeader,
             py::arg("key"), py::arg("value"),
             R"(Sets a default header for all requests.

Args:
    key: The header key.
    value: The header value.
)")
        .def("set_timeout", &HttpClient::setTimeout, py::arg("timeout"),
             R"(Sets the timeout duration for the HTTP operations.

Args:
    timeout: The timeout duration in seconds.
)")
        .def(
            "request",
            [](HttpClient& self, http::verb method, std::string_view host,
               std::string_view port, std::string_view target, int version,
               std::string_view content_type, std::string_view body,
               const std::unordered_map<std::string, std::string>& headers) {
                return self.request<http::string_body>(
                    method, host, port, target, version, content_type, body,
                    headers);
            },
            py::arg("method"), py::arg("host"), py::arg("port"),
            py::arg("target"), py::arg("version") = 11,
            py::arg("content_type") = "", py::arg("body") = "",
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends a synchronous HTTP request.

Args:
    method: The HTTP method (verb).
    host: The server host.
    port: The server port.
    target: The target URI.
    version: The HTTP version (default is 11).
    content_type: The content type of the request body.
    body: The request body.
    headers: Additional headers to include in the request.

Returns:
    The HTTP response.

Raises:
    ValueError: If host or port is empty
    ConnectionError: On connection or request failure
)")
        .def(
            "json_request", &HttpClient::jsonRequest, py::arg("method"),
            py::arg("host"), py::arg("port"), py::arg("target"),
            py::arg("json_body") = py::dict(),
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends a synchronous HTTP request with a JSON body and returns a JSON response.

Args:
    method: The HTTP method (verb).
    host: The server host.
    port: The server port.
    target: The target URI.
    json_body: The JSON body of the request (optional).
    headers: Additional headers to include in the request.

Returns:
    The JSON response.

Raises:
    ValueError: If host or port is empty
    ConnectionError: On connection or request failure
    json.JSONDecodeError: If JSON parsing fails
)")
        .def("upload_file", &HttpClient::uploadFile, py::arg("host"),
             py::arg("port"), py::arg("target"), py::arg("filepath"),
             py::arg("field_name") = "file",
             R"(Uploads a file to the server.

Args:
    host: The server host.
    port: The server port.
    target: The target URI.
    filepath: The path to the file to upload.
    field_name: The field name for the file (default is "file").

Returns:
    The HTTP response.

Raises:
    ValueError: If host, port, or filepath is empty
    ConnectionError: On connection or request failure
    RuntimeError: If file cannot be read
)")
        .def("download_file", &HttpClient::downloadFile, py::arg("host"),
             py::arg("port"), py::arg("target"), py::arg("filepath"),
             R"(Downloads a file from the server.

Args:
    host: The server host.
    port: The server port.
    target: The target URI.
    filepath: The path to save the downloaded file.

Raises:
    ValueError: If host, port, or filepath is empty
    ConnectionError: On connection or request failure
    RuntimeError: If file cannot be written
)")
        .def(
            "request_with_retry",
            [](HttpClient& self, http::verb method, std::string_view host,
               std::string_view port, std::string_view target, int retry_count,
               int version, std::string_view content_type,
               std::string_view body,
               const std::unordered_map<std::string, std::string>& headers) {
                return self.requestWithRetry<http::string_body>(
                    method, host, port, target, retry_count, version,
                    content_type, body, headers);
            },
            py::arg("method"), py::arg("host"), py::arg("port"),
            py::arg("target"), py::arg("retry_count") = 3,
            py::arg("version") = 11, py::arg("content_type") = "",
            py::arg("body") = "",
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends a synchronous HTTP request with retry logic.

Args:
    method: The HTTP method (verb).
    host: The server host.
    port: The server port.
    target: The target URI.
    retry_count: The number of retry attempts (default is 3).
    version: The HTTP version (default is 11).
    content_type: The content type of the request body.
    body: The request body.
    headers: Additional headers to include in the request.

Returns:
    The HTTP response.

Raises:
    ValueError: If host or port is empty
    ConnectionError: On connection or request failure after all retries
)")
        .def(
            "batch_request",
            [](HttpClient& self,
               const std::vector<std::tuple<http::verb, std::string,
                                            std::string, std::string>>&
                   requests,
               const std::unordered_map<std::string, std::string>& headers) {
                return self.batchRequest<http::string_body>(requests, headers);
            },
            py::arg("requests"),
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends multiple synchronous HTTP requests in a batch.

Args:
    requests: A list of tuples containing (method, host, port, target) for each request.
    headers: Additional headers to include in each request.

Returns:
    A list of HTTP responses.

Raises:
    ValueError: If any host or port is empty
    Note: Individual request failures will not raise exceptions,
          but will return empty responses in the result list.
)")
        .def("run_with_thread_pool", &HttpClient::runWithThreadPool,
             py::arg("num_threads"),
             R"(Runs the I/O context with a thread pool.

Args:
    num_threads: The number of threads in the pool.
)")
        .def(
            "async_request",
            [](HttpClient& self, http::verb method, std::string_view host,
               std::string_view port, std::string_view target,
               py::function handler, int version, std::string_view content_type,
               std::string_view body,
               const std::unordered_map<std::string, std::string>& headers) {
                self.asyncRequest<http::string_body>(
                    method, host, port, target,
                    [handler = std::move(handler)](
                        beast::error_code ec,
                        http::response<http::string_body> res) {
                        py::gil_scoped_acquire acquire;
                        handler(ec.value(), std::move(res));
                    },
                    version, content_type, body, headers);
            },
            py::arg("method"), py::arg("host"), py::arg("port"),
            py::arg("target"), py::arg("handler"), py::arg("version") = 11,
            py::arg("content_type") = "", py::arg("body") = "",
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends an asynchronous HTTP request.

Args:
    method: The HTTP method (verb).
    host: The server host.
    port: The server port.
    target: The target URI.
    handler: The callback function to call when the operation completes.
             Should accept two parameters: error_code (int) and response.
    version: The HTTP version (default is 11).
    content_type: The content type of the request body.
    body: The request body.
    headers: Additional headers to include in the request.

Raises:
    ValueError: If host or port is empty
)")
        .def(
            "async_json_request",
            [](HttpClient& self, http::verb method, std::string_view host,
               std::string_view port, std::string_view target,
               py::function handler, const py::object& json_body,
               const std::unordered_map<std::string, std::string>& headers) {
                // Convert Python dict to nlohmann::json
                json cpp_json = json::object();
                if (!json_body.is_none()) {
                    py::str json_str = py::str(
                        py::module::import("json").attr("dumps")(json_body));
                    cpp_json = json::parse(static_cast<std::string>(json_str));
                }

                self.asyncJsonRequest(
                    method, host, port, target,
                    [handler = std::move(handler)](beast::error_code ec,
                                                   json j) {
                        py::gil_scoped_acquire acquire;
                        // Convert nlohmann::json to Python dict
                        py::object py_json = py::none();
                        if (!ec) {
                            py::module json_module = py::module::import("json");
                            py_json = json_module.attr("loads")(j.dump());
                        }
                        handler(ec.value(), py_json);
                    },
                    cpp_json, headers);
            },
            py::arg("method"), py::arg("host"), py::arg("port"),
            py::arg("target"), py::arg("handler"),
            py::arg("json_body") = py::none(),
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends an asynchronous HTTP request with a JSON body and returns a JSON response.

Args:
    method: The HTTP method (verb).
    host: The server host.
    port: The server port.
    target: The target URI.
    handler: The callback function to call when the operation completes.
             Should accept two parameters: error_code (int) and json response.
    json_body: The JSON body of the request (optional).
    headers: Additional headers to include in the request.

Raises:
    ValueError: If host or port is empty
)")
        .def(
            "async_batch_request",
            [](HttpClient& self,
               const std::vector<std::tuple<http::verb, std::string,
                                            std::string, std::string>>&
                   requests,
               py::function handler,
               const std::unordered_map<std::string, std::string>& headers) {
                self.asyncBatchRequest(
                    requests,
                    [handler = std::move(handler)](
                        std::vector<http::response<http::string_body>>
                            responses) {
                        py::gil_scoped_acquire acquire;
                        handler(std::move(responses));
                    },
                    headers);
            },
            py::arg("requests"), py::arg("handler"),
            py::arg("headers") = std::unordered_map<std::string, std::string>(),
            R"(Sends multiple asynchronous HTTP requests in a batch.

Args:
    requests: A list of tuples containing (method, host, port, target) for each request.
    handler: The callback function to call when all operations complete.
             Should accept one parameter: a list of responses.
    headers: Additional headers to include in each request.

Raises:
    ValueError: If any host or port is empty
)")
        .def(
            "async_download_file",
            [](HttpClient& self, std::string_view host, std::string_view port,
               std::string_view target, std::string_view filepath,
               py::function handler) {
                self.asyncDownloadFile(host, port, target, filepath,
                                       [handler = std::move(handler)](
                                           beast::error_code ec, bool success) {
                                           py::gil_scoped_acquire acquire;
                                           handler(ec.value(), success);
                                       });
            },
            py::arg("host"), py::arg("port"), py::arg("target"),
            py::arg("filepath"), py::arg("handler"),
            R"(Asynchronously downloads a file from the server.

Args:
    host: The server host.
    port: The server port.
    target: The target URI.
    filepath: The path to save the downloaded file.
    handler: The callback function to call when the operation completes.
             Should accept two parameters: error_code (int) and success (bool).

Raises:
    ValueError: If host, port, or filepath is empty
)");

    // HTTP response binding for use with the responses returned by HttpClient
    py::class_<http::response<http::string_body>>(m, "HttpResponse",
                                                  R"(HTTP response class.

This class represents an HTTP response, providing access to status codes,
headers, and body content.
)")
        .def(
            "body",
            [](const http::response<http::string_body>& res) {
                return res.body();
            },
            "Gets the body of the response as a string.")
        .def_property_readonly(
            "status_code",
            [](const http::response<http::string_body>& res) {
                return static_cast<int>(res.result());
            },
            "Gets the HTTP status code of the response.")
        .def_property_readonly(
            "version",
            [](const http::response<http::string_body>& res) {
                return res.version();
            },
            "Gets the HTTP version of the response.")
        .def(
            "get_header",
            [](const http::response<http::string_body>& res,
               const std::string& key) -> py::object {
                auto it = res.find(key);
                if (it == res.end()) {
                    return py::none();
                }
                return py::str(it->value());
            },
            py::arg("key"), "Gets a specific header value by key.")
        .def(
            "headers",
            [](const http::response<http::string_body>& res) {
                py::dict headers;
                for (const auto& field : res) {
                    headers[py::str(field.name_string())] =
                        py::str(field.value());
                }
                return headers;
            },
            "Gets all headers as a dictionary.");
}
