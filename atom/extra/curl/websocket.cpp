#include "websocket.hpp"
#include "session.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace atom::extra::curl {
namespace {
bool sendWebSocketFrame(CURL* handle, unsigned char opcode, const char* data,
                        std::size_t length) {
    if (!handle) {
        return false;
    }

    std::vector<unsigned char> frame;
    frame.reserve(length + 14);

    frame.push_back(static_cast<unsigned char>(0x80 | (opcode & 0x0F)));

    std::uint64_t payloadLen = length;
    if (payloadLen <= 125) {
        frame.push_back(static_cast<unsigned char>(0x80 | payloadLen));
    } else if (payloadLen <= 0xFFFF) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<unsigned char>((payloadLen >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(payloadLen & 0xFF));
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(
                static_cast<unsigned char>((payloadLen >> (i * 8)) & 0xFF));
        }
    }

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uint8_t maskKey[4];
    for (int i = 0; i < 4; ++i) {
        maskKey[i] = static_cast<std::uint8_t>(rng() & 0xFF);
    }

    frame.insert(frame.end(), maskKey, maskKey + 4);

    std::size_t payloadStart = frame.size();
    frame.resize(frame.size() + payloadLen);
    for (std::size_t i = 0; i < payloadLen; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        frame[payloadStart + i] =
            static_cast<unsigned char>(c ^ maskKey[i % 4]);
    }

    std::size_t sentTotal = 0;
    while (sentTotal < frame.size()) {
        std::size_t sent = 0;
        CURLcode result = curl_easy_send(handle, frame.data() + sentTotal,
                                         frame.size() - sentTotal, &sent);
        if (result != CURLE_OK || sent == 0) {
            return false;
        }
        sentTotal += sent;
    }

    return sentTotal == frame.size();
}
}  // namespace

WebSocket::WebSocket() : handle_(nullptr), running_(false), connected_(false) {}

WebSocket::~WebSocket() {
    close();
    if (handle_) {
        curl_easy_cleanup(handle_);
    }
}

bool WebSocket::connect(const std::string& url,
                        const std::map<std::string, std::string>& headers) {
    if (connected_ || running_) {
        return false;
    }

    url_ = url;
    ensure_curl_global_init();
    handle_ = curl_easy_init();
    if (!handle_) {
        return false;
    }

    // 设置 libcurl 选项
    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle_, CURLOPT_CONNECT_ONLY, 2L);  // WebSocket

    // 设置请求头
    struct curl_slist* header_list = nullptr;
    for (const auto& [name, value] : headers) {
        std::string header = name + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }

    // 添加 WebSocket 特定的头
    header_list = curl_slist_append(header_list, "Connection: Upgrade");
    header_list = curl_slist_append(header_list, "Upgrade: websocket");
    header_list = curl_slist_append(header_list, "Sec-WebSocket-Version: 13");

    curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, header_list);

    // 执行连接
    CURLcode result = curl_easy_perform(handle_);
    curl_slist_free_all(header_list);

    if (result != CURLE_OK) {
        curl_easy_cleanup(handle_);
        handle_ = nullptr;
        if (connect_callback_) {
            connect_callback_(false);
        }
        return false;
    }

    connected_ = true;
    running_ = true;

    // 启动接收线程
    receive_thread_ = std::thread(&WebSocket::receive_loop, this);

    if (connect_callback_) {
        connect_callback_(true);
    }

    return true;
}

void WebSocket::close(int code, const std::string& reason) {
    if (!connected_) {
        return;
    }

    // 发送关闭帧
    send_close_frame(code, reason);

    // 停止接收线程
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    condition_.notify_all();

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    connected_ = false;

    if (close_callback_) {
        close_callback_(code, reason);
    }
}

bool WebSocket::send(const std::string& message, bool binary) {
    if (!connected_ || !handle_) {
        return false;
    }

    return sendWebSocketFrame(handle_, binary ? 0x02u : 0x01u, message.data(),
                              message.size());
}

void WebSocket::on_message(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void WebSocket::on_connect(ConnectCallback callback) {
    connect_callback_ = std::move(callback);
}

void WebSocket::on_close(CloseCallback callback) {
    close_callback_ = std::move(callback);
}

void WebSocket::receive_loop() {
    const size_t buffer_size = 65536;
    std::vector<char> buffer(buffer_size);

    while (running_) {
        size_t received = 0;
        CURLcode result =
            curl_easy_recv(handle_, buffer.data(), buffer.size(), &received);

        if (result != CURLE_OK) {
            break;
        }

        if (received > 0) {
            size_t offset = 0;
            while (offset + 2 <= received) {
                unsigned char b1 = static_cast<unsigned char>(buffer[offset]);
                unsigned char b2 =
                    static_cast<unsigned char>(buffer[offset + 1]);

                unsigned char opcode = static_cast<unsigned char>(b1 & 0x0F);
                bool masked = (b2 & 0x80) != 0;
                std::uint64_t payloadLen =
                    static_cast<std::uint64_t>(b2 & 0x7F);
                std::size_t headerLen = 2;

                if (payloadLen == 126) {
                    if (offset + headerLen + 2 > received) {
                        break;
                    }
                    payloadLen =
                        (static_cast<std::uint64_t>(
                             static_cast<unsigned char>(buffer[offset + 2]))
                         << 8) |
                        static_cast<std::uint64_t>(
                            static_cast<unsigned char>(buffer[offset + 3]));
                    headerLen += 2;
                } else if (payloadLen == 127) {
                    if (offset + headerLen + 8 > received) {
                        break;
                    }
                    payloadLen = 0;
                    for (int i = 0; i < 8; ++i) {
                        payloadLen = (payloadLen << 8) |
                                     static_cast<std::uint64_t>(
                                         static_cast<unsigned char>(
                                             buffer[offset + 2 + i]));
                    }
                    headerLen += 8;
                }

                std::uint8_t maskKey[4] = {0, 0, 0, 0};
                if (masked) {
                    if (offset + headerLen + 4 > received) {
                        break;
                    }
                    for (int i = 0; i < 4; ++i) {
                        maskKey[i] = static_cast<std::uint8_t>(
                            static_cast<unsigned char>(
                                buffer[offset + headerLen + i]));
                    }
                    headerLen += 4;
                }

                if (offset + headerLen + payloadLen > received) {
                    break;
                }

                std::string payload;
                payload.resize(static_cast<std::size_t>(payloadLen));
                for (std::size_t i = 0; i < payloadLen; ++i) {
                    unsigned char c = static_cast<unsigned char>(
                        buffer[offset + headerLen + i]);
                    if (masked) {
                        c = static_cast<unsigned char>(c ^ maskKey[i % 4]);
                    }
                    payload[i] = static_cast<char>(c);
                }

                offset += headerLen + static_cast<std::size_t>(payloadLen);

                if (opcode == 0x08) {
                    int closeCode = 1005;
                    std::string reason;
                    if (payloadLen >= 2) {
                        closeCode = (static_cast<int>(
                                         static_cast<unsigned char>(payload[0]))
                                     << 8) |
                                    static_cast<int>(
                                        static_cast<unsigned char>(payload[1]));
                        if (payloadLen > 2) {
                            reason.assign(payload.begin() + 2, payload.end());
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        running_ = false;
                        connected_ = false;
                    }

                    if (close_callback_) {
                        close_callback_(closeCode, reason);
                    }

                    return;
                }

                if (opcode == 0x01 || opcode == 0x02) {
                    bool isBinary = (opcode == 0x02);
                    if (message_callback_) {
                        message_callback_(payload, isBinary);
                    }
                }
            }
        }
    }
}

void WebSocket::send_close_frame(int code, const std::string& reason) {
    if (!connected_ || !handle_) {
        return;
    }

    std::vector<char> payload(reason.size() + 2);
    payload[0] = static_cast<char>((code >> 8) & 0xFF);
    payload[1] = static_cast<char>(code & 0xFF);
    std::memcpy(payload.data() + 2, reason.data(), reason.size());

    (void)sendWebSocketFrame(handle_, 0x08u, payload.data(), payload.size());
}
}  // namespace atom::extra::curl
