#include "websocket.hpp"

namespace atom::extra::curl {
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

    size_t sent = 0;
    CURLcode result;

    // Create WebSocket frame header
    std::vector<char> frame;
    frame.reserve(message.size() + 10);  // Max header size is 10 bytes

    // First byte: FIN + Opcode
    frame.push_back(
        0x80 | (binary ? 0x02 : 0x01));  // 0x80=FIN, 0x01=text, 0x02=binary

    // Second byte: Mask + Payload length
    if (message.size() <= 125) {
        frame.push_back(static_cast<char>(message.size()));
    } else if (message.size() <= 65535) {
        frame.push_back(126);
        frame.push_back((message.size() >> 8) & 0xFF);
        frame.push_back(message.size() & 0xFF);
    } else {
        frame.push_back(127);
        uint64_t len = message.size();
        for (int i = 7; i >= 0; i--) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }

    // Add message data
    frame.insert(frame.end(), message.begin(), message.end());

    // Send frame
    size_t sent_total = 0;
    while (sent_total < frame.size()) {
        result = curl_easy_send(handle_, frame.data() + sent_total,
                                frame.size() - sent_total, &sent);
        if (result != CURLE_OK) {
            break;
        }
        sent_total += sent;
    }

    return result == CURLE_OK && sent_total == frame.size();
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
            // Basic frame parsing
            if (static_cast<unsigned char>(buffer[0]) == 0x88) {  // Close frame
                uint16_t close_code = 1005;  // No status code
                std::string reason;

                if (received >= 4) {  // Skip 2 bytes header
                    close_code = (static_cast<uint16_t>(buffer[2]) << 8) |
                                 static_cast<uint8_t>(buffer[3]);
                    if (received > 4) {
                        reason = std::string(buffer.data() + 4, received - 4);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    running_ = false;
                    connected_ = false;
                }

                if (close_callback_) {
                    close_callback_(close_code, reason);
                }

                break;
            } else if (static_cast<unsigned char>(buffer[0]) == 0x81 ||
                       static_cast<unsigned char>(buffer[0]) ==
                           0x82) {  // Text or Binary frame
                // Data frame
                bool is_binary =
                    (static_cast<unsigned char>(buffer[0]) == 0x82);
                if (message_callback_) {
                    message_callback_(std::string(buffer.data(), received),
                                      is_binary);
                }
            }
            // 忽略其他控制帧
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

    size_t sent = 0;
    // Workaround: send close frame payload directly since WebSocket frame
    // API is unavailable.
    CURLcode result =
        curl_easy_send(handle_, payload.data(), payload.size(), &sent);
    (void)result;
}
}  // namespace atom::extra::curl
