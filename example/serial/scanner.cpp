#include "atom/serial/scanner.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace atom::serial;

/**
 * 展示如何使用 SerialPortScanner 类的基本功能
 * 
 * 本示例展示:
 * 1. 异步列出可用端口
 * 2. 获取特定端口的详细信息
 * 3. 注册自定义设备检测器
 * 4. 使用配置选项
 */
int main() {
    try {
        std::cout << "SerialPortScanner 示例程序\n";
        std::cout << "========================\n\n";

        // 创建一个带自定义配置的扫描器
        SerialPortScanner::ScannerConfig config;
        config.detect_ch340 = true;
        config.include_virtual_ports = false;
        config.timeout = std::chrono::milliseconds(2000);
        
        SerialPortScanner scanner(config);
        
        // 注册一个自定义设备检测器
        scanner.register_device_detector(
            "FTDI", 
            [](uint16_t vid, uint16_t pid, std::string_view description) -> std::pair<bool, std::string> {
                // 检测是否为FTDI设备
                if (vid == 0x0403) {
                    return {true, "FTDI Device"};
                }
                
                // 检查描述
                std::string lower_desc;
                lower_desc.resize(description.size());
                std::transform(description.begin(), description.end(), lower_desc.begin(),
                              [](unsigned char c) { return std::tolower(c); });
                              
                if (lower_desc.find("ftdi") != std::string::npos) {
                    return {true, "FTDI (Detected by Description)"};
                }
                
                return {false, ""};
            }
        );
        
        // 异步列出可用端口
        std::cout << "正在异步列出可用端口...\n";
        std::atomic<bool> done = false;
        
        scanner.list_available_ports_async(
            [&done](SerialPortScanner::Result<std::vector<SerialPortScanner::PortInfo>> result) {
                if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(result)) {
                    const auto& ports = std::get<std::vector<SerialPortScanner::PortInfo>>(result);
                    
                    std::cout << "找到 " << ports.size() << " 个串口:\n";
                    for (const auto& port : ports) {
                        std::cout << " - " << port.device << ": " << port.description;
                        if (port.is_ch340) {
                            std::cout << " (CH340 设备: " << port.ch340_model << ")";
                        }
                        std::cout << std::endl;
                    }
                } else {
                    const auto& error = std::get<SerialPortScanner::ErrorInfo>(result);
                    std::cerr << "错误: " << error.message 
                              << " (代码: " << error.error_code << ")\n";
                }
                
                done = true;
            }
        );
        
        // 等待异步操作完成
        while (!done) {
            std::cout << "等待扫描完成...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        std::cout << "\n正在同步列出可用端口...\n";
        auto sync_result = scanner.list_available_ports();
        
        if (std::holds_alternative<std::vector<SerialPortScanner::PortInfo>>(sync_result)) {
            const auto& ports = std::get<std::vector<SerialPortScanner::PortInfo>>(sync_result);
            
            // 如果找到端口，获取第一个端口的详细信息
            if (!ports.empty()) {
                std::string first_port = ports.front().device;
                std::cout << "\n获取 " << first_port << " 的详细信息:\n";
                
                auto details_result = scanner.get_port_details(first_port);
                
                if (std::holds_alternative<std::optional<SerialPortScanner::PortDetails>>(details_result)) {
                    const auto& maybe_details = std::get<std::optional<SerialPortScanner::PortDetails>>(details_result);
                    
                    if (maybe_details) {
                        const auto& details = *maybe_details;
                        std::cout << "  设备名称: " << details.device_name << "\n";
                        std::cout << "  描述: " << details.description << "\n";
                        std::cout << "  硬件 ID: " << details.hardware_id << "\n";
                        std::cout << "  VID: " << details.vid << "\n";
                        std::cout << "  PID: " << details.pid << "\n";
                        
                        if (!details.serial_number.empty())
                            std::cout << "  序列号: " << details.serial_number << "\n";
                        
                        if (!details.manufacturer.empty())
                            std::cout << "  制造商: " << details.manufacturer << "\n";
                        
                        if (!details.product.empty())
                            std::cout << "  产品: " << details.product << "\n";
                        
                        if (!details.location.empty())
                            std::cout << "  位置: " << details.location << "\n";
                        
                        if (!details.interface.empty())
                            std::cout << "  接口: " << details.interface << "\n";
                        
                        if (details.is_ch340) {
                            std::cout << "  CH340 设备: " << details.ch340_model << "\n";
                            std::cout << "  推荐波特率: " << details.recommended_baud_rates << "\n";
                            std::cout << "  附注: " << details.notes << "\n";
                        }
                    } else {
                        std::cout << "  未找到该端口的详细信息\n";
                    }
                } else {
                    const auto& error = std::get<SerialPortScanner::ErrorInfo>(details_result);
                    std::cerr << "获取详细信息时出错: " << error.message 
                              << " (代码: " << error.error_code << ")\n";
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}