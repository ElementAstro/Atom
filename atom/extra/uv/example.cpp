/**
 * @file example.cpp
 * @brief Comprehensive example demonstrating all UV components
 */

#include "uv_utils.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

using namespace uv_utils;
using namespace uv_coro;
using namespace uv_http;
using namespace uv_websocket;
using namespace msgbus;

// Example message types
struct ChatMessage {
    std::string user;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    
    std::string serialize() const {
        return user + "|" + content + "|" + std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                timestamp.time_since_epoch()).count());
    }
    
    static ChatMessage deserialize(const std::string& data) {
        auto parts = helpers::string::split(data, "|");
        ChatMessage msg;
        if (parts.size() >= 3) {
            msg.user = parts[0];
            msg.content = parts[1];
            auto timestamp_sec = std::stoull(parts[2]);
            msg.timestamp = std::chrono::system_clock::time_point(
                std::chrono::seconds(timestamp_sec));
        }
        return msg;
    }
};

struct TaskRequest {
    std::string id;
    std::string command;
    std::vector<std::string> args;
    std::chrono::seconds timeout{30};
    
    std::string serialize() const {
        std::string result = id + "|" + command + "|" + std::to_string(timeout.count());
        for (const auto& arg : args) {
            result += "|" + arg;
        }
        return result;
    }
    
    static TaskRequest deserialize(const std::string& data) {
        auto parts = helpers::string::split(data, "|");
        TaskRequest req;
        if (parts.size() >= 3) {
            req.id = parts[0];
            req.command = parts[1];
            req.timeout = std::chrono::seconds(std::stoull(parts[2]));
            for (size_t i = 3; i < parts.size(); ++i) {
                req.args.push_back(parts[i]);
            }
        }
        return req;
    }
};

// Coroutine examples
Task<void> chat_message_processor(UvApplication& app) {
    spdlog::info("Starting chat message processor...");
    
    auto subscription = app.subscribe_message<ChatMessage>(
        "chat.*", [&app](const ChatMessage& msg) {
            spdlog::info("Processing chat message from {}: {}", msg.user, msg.content);
            
            // Broadcast to WebSocket clients
            if (auto ws_server = app.get_websocket_server()) {
                std::string json_msg = helpers::json::object_to_string({
                    {"type", "chat"},
                    {"user", msg.user},
                    {"content", msg.content},
                    {"timestamp", std::to_string(
                        std::chrono::duration_cast<std::chrono::seconds>(
                            msg.timestamp.time_since_epoch()).count())}
                });
                ws_server->broadcast_text(json_msg);
            }
        });
    
    // Keep the processor running
    while (app.is_running()) {
        co_await sleep_for(1000);
    }
    
    spdlog::info("Chat message processor stopped");
}

Task<void> task_executor(UvApplication& app) {
    spdlog::info("Starting task executor...");
    
    auto subscription = app.subscribe_message<TaskRequest>(
        "tasks.*", [&app](const TaskRequest& req) {
            spdlog::info("Executing task {}: {} with {} args", 
                        req.id, req.command, req.args.size());
            
            UvProcess::ProcessOptions options;
            options.file = req.command;
            options.args = req.args;
            options.timeout = std::chrono::duration_cast<std::chrono::milliseconds>(req.timeout);
            
            auto future = app.execute_process(options);
            
            // Handle result asynchronously
            std::thread([future = std::move(future), req, &app]() mutable {
                try {
                    auto metrics = future.get();
                    
                    std::string result_topic = "task_results." + req.id;
                    std::string result_data = helpers::json::object_to_string({
                        {"task_id", req.id},
                        {"exit_code", std::to_string(metrics.exit_code)},
                        {"execution_time", std::to_string(metrics.execution_time.count())},
                        {"memory_usage", std::to_string(metrics.peak_memory_usage)},
                        {"success", metrics.exit_code == 0 ? "true" : "false"}
                    });
                    
                    app.publish_message(result_topic, result_data);
                    spdlog::info("Task {} completed with exit code {}", 
                                req.id, metrics.exit_code);
                } catch (const std::exception& e) {
                    spdlog::error("Task {} failed: {}", req.id, e.what());
                }
            }).detach();
        });
    
    while (app.is_running()) {
        co_await sleep_for(1000);
    }
    
    spdlog::info("Task executor stopped");
}

// HTTP handlers
UV_HTTP_HANDLER(handle_api_status) {
    auto monitor = static_cast<UvApplication*>(ctx.get<void*>("app").value_or(nullptr))->get_monitor();
    
    if (monitor) {
        auto system_metrics = monitor->get_system_metrics();
        auto process_metrics = monitor->get_process_metrics();
        
        std::string json_response = helpers::json::object_to_string({
            {"status", "ok"},
            {"uptime", std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - process_metrics.start_time).count())},
            {"cpu_usage", std::to_string(system_metrics.cpu_usage_percent)},
            {"memory_usage", std::to_string(system_metrics.memory_usage_percent)},
            {"process_memory", std::to_string(process_metrics.memory_rss)},
            {"active_connections", "0"} // Would get from WebSocket server
        });
        
        ctx.json(json_response);
    } else {
        ctx.error(500, "Monitoring not available");
    }
}

UV_HTTP_HANDLER(handle_send_message) {
    auto app = static_cast<UvApplication*>(ctx.get<void*>("app").value_or(nullptr));
    if (!app) {
        ctx.error(500, "Application not available");
        return;
    }
    
    // Parse JSON body (simplified)
    auto user = ctx.request.get_query_param("user").value_or("anonymous");
    auto content = ctx.request.get_query_param("content").value_or("");
    
    if (content.empty()) {
        ctx.error(400, "Content is required");
        return;
    }
    
    ChatMessage msg;
    msg.user = user;
    msg.content = content;
    msg.timestamp = std::chrono::system_clock::now();
    
    app->publish_message("chat.general", msg);
    
    ctx.json(helpers::json::object_to_string({
        {"status", "sent"},
        {"message_id", std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                msg.timestamp.time_since_epoch()).count())}
    }));
}

UV_HTTP_HANDLER(handle_execute_task) {
    auto app = static_cast<UvApplication*>(ctx.get<void*>("app").value_or(nullptr));
    if (!app) {
        ctx.error(500, "Application not available");
        return;
    }
    
    auto command = ctx.request.get_query_param("command").value_or("");
    if (command.empty()) {
        ctx.error(400, "Command is required");
        return;
    }
    
    TaskRequest req;
    req.id = "task_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    req.command = command;
    
    // Parse args from query parameters
    for (int i = 1; i <= 10; ++i) {
        auto arg = ctx.request.get_query_param("arg" + std::to_string(i));
        if (arg) {
            req.args.push_back(*arg);
        } else {
            break;
        }
    }
    
    app->publish_message("tasks.execute", req);
    
    ctx.json(helpers::json::object_to_string({
        {"status", "queued"},
        {"task_id", req.id}
    }));
}

// WebSocket handlers
UV_WS_HANDLER(handle_websocket_message) {
    spdlog::info("WebSocket message from {}: {}", 
                conn.get_id(), msg.to_text());
    
    // Echo the message back
    conn.send_text("Echo: " + msg.to_text());
}

void handle_websocket_connection(WebSocketConnection& conn) {
    spdlog::info("New WebSocket connection: {}", conn.get_id());
    
    // Send welcome message
    std::string welcome = helpers::json::object_to_string({
        {"type", "welcome"},
        {"connection_id", conn.get_id()},
        {"timestamp", helpers::get_timestamp()}
    });
    
    conn.send_text(welcome);
}

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting UV Application Example...");
    
    try {
        // Configure the application
        UvApplication::Config config;
        config.enable_http_server = true;
        config.enable_websocket_server = true;
        config.enable_monitoring = true;
        config.enable_process_pool = true;
        
        config.http_config.port = 8080;
        config.websocket_config.port = 8081;
        
        // Create and initialize the application
        UvApplication app(config);
        app.initialize();
        
        // Set up HTTP routes
        app.http_get("/api/status", handle_api_status);
        app.http_post("/api/send_message", handle_send_message);
        app.http_post("/api/execute_task", handle_execute_task);
        
        // Set up WebSocket handlers
        app.websocket_on_connection(handle_websocket_connection);
        app.websocket_on_message(handle_websocket_message);
        
        // Start background coroutines
        auto chat_processor = chat_message_processor(app);
        auto task_exec = task_executor(app);
        
        // Set up signal handlers
        app.on_signal(SIGINT, [&app]() {
            spdlog::info("Received SIGINT, shutting down...");
            app.shutdown();
        });
        
        app.on_signal(SIGTERM, [&app]() {
            spdlog::info("Received SIGTERM, shutting down...");
            app.shutdown();
        });
        
        spdlog::info("Application started successfully!");
        spdlog::info("HTTP server: http://localhost:8080");
        spdlog::info("WebSocket server: ws://localhost:8081");
        spdlog::info("Try: curl 'http://localhost:8080/api/status'");
        spdlog::info("Try: curl -X POST 'http://localhost:8080/api/send_message?user=test&content=hello'");
        
        // Run the application
        return app.run();
        
    } catch (const std::exception& e) {
        spdlog::error("Application error: {}", e.what());
        return 1;
    }
}
