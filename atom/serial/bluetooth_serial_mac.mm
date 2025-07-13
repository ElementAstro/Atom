#ifdef __APPLE__

#include "BluetoothSerial.h"
#include <Foundation/Foundation.h>
#include <IOBluetooth/IOBluetooth.h>
#include <IOKit/IOReturn.h>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

// RFCOMM委托类用于处理蓝牙通信事件
@interface RFCOMMDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate> {
  serial::BluetoothSerialImpl *owner;
  std::queue<std::vector<uint8_t>> readQueue;
  std::mutex queueMutex;
  std::atomic<bool> dataAvailable;
  std::function<void(std::vector<uint8_t>)> asyncCallback;
  std::atomic<bool> hasAsyncCallback;
}

@property(nonatomic, assign) serial::BluetoothSerialImpl *owner;
@property(nonatomic, strong) IOBluetoothRFCOMMChannel *rfcommChannel;

- (id)initWithOwner:(serial::BluetoothSerialImpl *)owner;
- (void)setAsyncReadCallback:
    (std::function<void(std::vector<uint8_t>)>)callback;
- (bool)hasData;
- (std::vector<uint8_t>)readData;
- (size_t)available;

@end

@implementation RFCOMMDelegate

@synthesize owner;
@synthesize rfcommChannel;

- (id)initWithOwner:(serial::BluetoothSerialImpl *)theOwner {
  if (self = [super init]) {
    owner = theOwner;
    dataAvailable = false;
    hasAsyncCallback = false;
  }
  return self;
}

- (void)setAsyncReadCallback:
    (std::function<void(std::vector<uint8_t>)>)callback {
  asyncCallback = callback;
  hasAsyncCallback = true;
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel
                     data:(void *)dataPointer
                   length:(size_t)dataLength {
  const uint8_t *byteData = static_cast<const uint8_t *>(dataPointer);
  std::vector<uint8_t> receivedData(byteData, byteData + dataLength);

  if (hasAsyncCallback) {
    asyncCallback(receivedData);
  } else {
    std::lock_guard<std::mutex> lock(queueMutex);
    readQueue.push(receivedData);
    dataAvailable = true;
  }

  // 更新统计信息
  if (owner) {
    owner->updateReadStats(dataLength);
  }
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel {
  if (owner) {
    owner->handleDisconnect();
  }
}

- (bool)hasData {
  std::lock_guard<std::mutex> lock(queueMutex);
  return !readQueue.empty();
}

- (std::vector<uint8_t>)readData {
  std::lock_guard<std::mutex> lock(queueMutex);
  if (readQueue.empty()) {
    return {};
  }

  std::vector<uint8_t> data = readQueue.front();
  readQueue.pop();
  dataAvailable = !readQueue.empty();
  return data;
}

- (size_t)available {
  std::lock_guard<std::mutex> lock(queueMutex);
  size_t total = 0;
  std::queue<std::vector<uint8_t>> tempQueue = readQueue; // 复制队列以便遍历

  while (!tempQueue.empty()) {
    total += tempQueue.front().size();
    tempQueue.pop();
  }

  return total;
}

@end

namespace serial {

// 在类定义中添加委托和设备对象
class BluetoothSerialImpl {
public:
  BluetoothSerialImpl()
      : scanThread_(), stopScan_(false), asyncReadThread_(),
        stopAsyncRead_(false), stats_{}, connectionListener_(nullptr) {
    delegate = [[RFCOMMDelegate alloc] initWithOwner:this];
  }

  ~BluetoothSerialImpl() {
    stopAsyncWorker();
    disconnect();
  }

  bool isBluetoothEnabled() const {
    IOBluetoothHostController *controller =
        [IOBluetoothHostController defaultController];
    return controller != nil &&
           [controller powerState] == kBluetoothHCIPowerStateON;
  }

  void enableBluetooth(bool enable) {
    throw BluetoothException(
        "在macOS上程序无法直接开启/关闭蓝牙适配器，需用户通过系统设置操作");
  }

  std::vector<BluetoothDeviceInfo> scanDevices(std::chrono::seconds timeout) {
    std::vector<BluetoothDeviceInfo> devices;

    IOBluetoothDeviceInquiry *inquiry =
        [IOBluetoothDeviceInquiry inquiryWithDelegate:nil];
    if (inquiry == nil) {
      throw BluetoothException("无法创建蓝牙设备查询");
    }

    // 设置搜索时间
    [inquiry setInquiryLength:timeout.count()];

    // 开始查询
    IOReturn result = [inquiry start];
    if (result != kIOReturnSuccess) {
      throw BluetoothException("启动蓝牙设备扫描失败: " +
                               std::to_string(result));
    }

    // 等待查询完成
    NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:timeout.count()];
    while ([inquiry isInquiryRunning] &&
           [timeoutDate timeIntervalSinceNow] > 0) {
      [[NSRunLoop currentRunLoop]
             runMode:NSDefaultRunLoopMode
          beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];

      if (stopScan_) {
        [inquiry stop];
        break;
      }
    }

    // 获取结果
    NSArray *foundDevices = [inquiry foundDevices];
    for (IOBluetoothDevice *device in foundDevices) {
      BluetoothDeviceInfo info;
      info.name =
          [device.name UTF8String] ? [device.name UTF8String] : "Unknown";
      info.address = [device.addressString UTF8String];
      info.paired = [device isPaired];
      devices.push_back(info);
    }

    return devices;
  }

  void scanDevicesAsync(
      std::function<void(const BluetoothDeviceInfo &)> onDeviceFound,
      std::function<void()> onScanComplete, std::chrono::seconds timeout) {

    // 确保之前的扫描已停止
    stopScan_ = true;
    if (scanThread_.joinable()) {
      scanThread_.join();
    }

    stopScan_ = false;
    scanThread_ = std::thread([this, onDeviceFound, onScanComplete, timeout]() {
      try {
        auto devices = scanDevices(timeout);

        if (!stopScan_) {
          for (const auto &device : devices) {
            if (stopScan_)
              break;
            onDeviceFound(device);
          }

          onScanComplete();
        }
      } catch (const std::exception &e) {
        std::cerr << "蓝牙扫描异常: " << e.what() << std::endl;
        if (!stopScan_) {
          onScanComplete();
        }
      }
    });
  }

  void stopScan() {
    stopScan_ = true;
    if (scanThread_.joinable()) {
      scanThread_.join();
    }
  }

  void connect(const std::string &address, const BluetoothConfig &config) {
    // 断开现有连接
    disconnect();

    config_ = config;

    // 转换地址为NSString
    NSString *macAddress = [NSString stringWithUTF8String:address.c_str()];

    // 根据地址查找设备
    IOBluetoothDevice *device =
        [IOBluetoothDevice deviceWithAddressString:macAddress];
    if (!device) {
      throw BluetoothException("未找到指定的蓝牙设备: " + address);
    }

    // 打开RFCOMM通道
    BluetoothRFCOMMChannelID channelID = 1; // 默认通道
    IOBluetoothRFCOMMChannel *rfcommChannel = nil;
    IOReturn status = [device openRFCOMMChannelSync:&rfcommChannel
                                      withChannelID:channelID
                                           delegate:delegate];

    if (status != kIOReturnSuccess) {
      throw BluetoothException("无法打开RFCOMM通道: " + std::to_string(status));
    }

    // 设置代理的通道
    [delegate setRfcommChannel:rfcommChannel];

    // 记录已连接设备
    BluetoothDeviceInfo connInfo;
    connInfo.name =
        [device.name UTF8String] ? [device.name UTF8String] : "Unknown";
    connInfo.address = address;
    connInfo.paired = [device isPaired];
    connectedDevice_ = connInfo;

    // 通知连接成功
    if (connectionListener_) {
      connectionListener_(true);
    }
  }

  void disconnect() {
    if ([delegate rfcommChannel]) {
      [[delegate rfcommChannel] closeChannel];
      [delegate setRfcommChannel:nil];

      if (connectionListener_) {
        connectionListener_(false);
      }

      connectedDevice_.reset();
    }
  }

  bool isConnected() const {
    return [delegate rfcommChannel] != nil && [[delegate rfcommChannel] isOpen];
  }

  std::optional<BluetoothDeviceInfo> getConnectedDevice() const {
    return connectedDevice_;
  }

  bool pair(const std::string &address, const std::string &pin) {
    NSString *macAddress = [NSString stringWithUTF8String:address.c_str()];
    IOBluetoothDevice *device =
        [IOBluetoothDevice deviceWithAddressString:macAddress];

    if (!device) {
      return false;
    }

    if ([device isPaired]) {
      return true; // 已经配对
    }

    // macOS通常会通过系统UI处理配对，此处仅显示提示
    std::cerr << "请通过macOS系统UI完成设备配对过程" << std::endl;
    return false;
  }

  bool unpair(const std::string &address) {
    NSString *macAddress = [NSString stringWithUTF8String:address.c_str()];
    IOBluetoothDevice *device =
        [IOBluetoothDevice deviceWithAddressString:macAddress];

    if (!device || ![device isPaired]) {
      return false;
    }

    // macOS需要通过系统UI取消配对
    std::cerr << "请通过macOS系统UI取消设备配对" << std::endl;
    return false;
  }

  std::vector<BluetoothDeviceInfo> getPairedDevices() {
    std::vector<BluetoothDeviceInfo> pairedDevices;

    NSArray *devices = [IOBluetoothDevice pairedDevices];
    for (IOBluetoothDevice *device in devices) {
      BluetoothDeviceInfo info;
      info.name =
          [device.name UTF8String] ? [device.name UTF8String] : "Unknown";
      info.address = [device.addressString UTF8String];
      info.paired = true;
      pairedDevices.push_back(info);
    }

    return pairedDevices;
  }

  std::vector<uint8_t> read(size_t maxBytes) {
    if (!isConnected()) {
      throw BluetoothException("设备未连接");
    }

    if (![delegate hasData]) {
      return {};
    }

    std::vector<uint8_t> data = [delegate readData];
    if (maxBytes > 0 && data.size() > maxBytes) {
      data.resize(maxBytes);
    }

    return data;
  }

  std::vector<uint8_t> readExactly(size_t bytes,
                                   std::chrono::milliseconds timeout) {
    if (!isConnected()) {
      throw BluetoothException("设备未连接");
    }

    std::vector<uint8_t> result;
    auto endTime = std::chrono::steady_clock::now() + timeout;

    while (result.size() < bytes) {
      if (std::chrono::steady_clock::now() > endTime) {
        throw BluetoothException("读取超时");
      }

      if ([delegate hasData]) {
        std::vector<uint8_t> chunk = [delegate readData];
        result.insert(result.end(), chunk.begin(), chunk.end());
      } else {
        // 等待一段时间后再次检查
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }

    // 如果读取的数据超过请求的字节数，只返回所需部分
    if (result.size() > bytes) {
      result.resize(bytes);
    }

    return result;
  }

  void asyncRead(size_t maxBytes,
                 std::function<void(std::vector<uint8_t>)> callback) {
    if (!isConnected()) {
      throw BluetoothException("设备未连接");
    }

    // 设置回调函数，让RFCOMM代理直接调用
    [delegate setAsyncReadCallback:callback];
  }

  std::vector<uint8_t> readAvailable() {
    if (!isConnected()) {
      throw BluetoothException("设备未连接");
    }

    std::vector<uint8_t> result;

    while ([delegate hasData]) {
      std::vector<uint8_t> chunk = [delegate readData];
      result.insert(result.end(), chunk.begin(), chunk.end());
    }

    return result;
  }

  size_t write(std::span<const uint8_t> data) {
    if (!isConnected()) {
      throw BluetoothException("设备未连接");
    }

    IOBluetoothRFCOMMChannel *channel = [delegate rfcommChannel];
    if (!channel) {
      throw BluetoothException("RFCOMM通道无效");
    }

    IOReturn result =
        [channel writeSync:(void *)data.data() length:data.size()];
    if (result != kIOReturnSuccess) {
      throw BluetoothException("写入数据失败: " + std::to_string(result));
    }

    // 更新统计信息
    stats_.bytesSent += data.size();
    stats_.packetsSent++;

    return data.size();
  }

  void flush() {
    // IOBluetooth不直接支持flush操作，通常数据会立即发送
  }

  size_t available() const {
    if (!isConnected()) {
      return 0;
    }

    return [delegate available];
  }

  void setConnectionListener(std::function<void(bool connected)> listener) {
    connectionListener_ = std::move(listener);
  }

  BluetoothSerial::Statistics getStatistics() const { return stats_; }

  // 用于委托回调的方法
  void handleDisconnect() {
    if (connectionListener_) {
      connectionListener_(false);
    }
    connectedDevice_.reset();
  }

  void updateReadStats(size_t bytes) {
    stats_.bytesReceived += bytes;
    stats_.packetsReceived++;
  }

private:
  RFCOMMDelegate *delegate;
  std::thread scanThread_;
  std::atomic<bool> stopScan_;
  std::thread asyncReadThread_;
  std::atomic<bool> stopAsyncRead_;
  BluetoothConfig config_;
  std::optional<BluetoothDeviceInfo> connectedDevice_;
  BluetoothSerial::Statistics stats_;
  std::function<void(bool)> connectionListener_;

  void stopAsyncWorker() {
    if (asyncReadThread_.joinable()) {
      stopAsyncRead_ = true;
      asyncReadThread_.join();
    }
  }
};

// 在这里添加BluetoothSerial类的实现...

} // namespace serial

#endif // __APPLE__
