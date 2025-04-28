#include "password_manager.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include "atom/algorithm/base.hpp"  // 用于base64编码/解码
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// 常量定义
namespace {
constexpr std::string_view PM_VERSION = "1.0.0";
constexpr std::string_view PM_SERVICE_NAME = "AtomPasswordManager";
constexpr size_t PM_SALT_SIZE = 16;
constexpr size_t PM_IV_SIZE = 12;   // AES-GCM标准IV大小
constexpr size_t PM_TAG_SIZE = 16;  // AES-GCM标准标签大小
constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;
constexpr std::string_view VERIFICATION_PREFIX = "ATOM_PM_VERIFICATION_";
}  // namespace

namespace atom::secret {

std::vector<std::string> PasswordManager::searchPasswords(std::string_view query) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot search passwords: PasswordManager is locked");
        return {};
    }
    
    if (query.empty()) {
        LOG_F(WARNING, "Empty search query, returning all keys.");
        std::vector<std::string> allKeys;
        allKeys.reserve(cachedPasswords.size());
        for (const auto& pair : cachedPasswords) {
            allKeys.push_back(pair.first);
        }
        return allKeys;
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for search");
            return {};
        }
        
        std::vector<std::string> results;
        std::string lowerQuery(query);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        for (const auto& [key, entry] : cachedPasswords) {
            // 转换为小写进行不区分大小写的搜索
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            // 搜索标题、用户名、网址和标签
            std::string lowerTitle = entry.username;
            std::string lowerUsername = entry.username;
            std::string lowerUrl = entry.url;
            
            std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUsername.begin(), lowerUsername.end(), lowerUsername.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            if (lowerKey.find(lowerQuery) != std::string::npos ||
                lowerTitle.find(lowerQuery) != std::string::npos ||
                lowerUsername.find(lowerQuery) != std::string::npos ||
                lowerUrl.find(lowerQuery) != std::string::npos) {
                results.push_back(key);
                continue;
            }
            
            // 搜索标签
            for (const auto& tag : entry.tags) {
                std::string lowerTag = tag;
                std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(),
                              [](unsigned char c) { return std::tolower(c); });
                if (lowerTag.find(lowerQuery) != std::string::npos) {
                    results.push_back(key);
                    break;
                }
            }
        }

        LOG_F(INFO, "Search for '{}' returned {} results", query.data(), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Search passwords error: {}", e.what());
        return {};
    }
}

// 补充filterByCategory中的循环实现
std::vector<std::string> PasswordManager::filterByCategory(PasswordCategory category) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot filter passwords: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for category filtering");
            return {};
        }

        std::vector<std::string> results;
        for (const auto& [key, entry] : cachedPasswords) {
            if (entry.category == category) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Filter by category {} returned {} results",
              static_cast<int>(category), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Filter by category error: {}", e.what());
        return {};
    }
}

// 实现生成密码的方法
std::string PasswordManager::generatePassword(int length, bool includeSpecial,
                                             bool includeNumbers,
                                             bool includeMixedCase) {
    // 无需锁定用于生成，但updateActivity需要锁定
    // 先调用updateActivity
    {
        std::unique_lock lock(mutex);
        if (!isUnlocked.load(std::memory_order_relaxed)) {
            LOG_F(ERROR, "Cannot generate password: PasswordManager is locked");
            return "";
        }
        updateActivity();
    }

    if (length < settings.minPasswordLength) {
        LOG_F(WARNING, "Requested password length {} is less than minimum {}, using minimum",
              length, settings.minPasswordLength);
        length = settings.minPasswordLength;
    }
    if (length <= 0) {
        LOG_F(ERROR, "Invalid password length: {}", length);
        return "";
    }

    // 字符集
    const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string digits = "0123456789";
    const std::string special = "!@#$%^&*()-_=+[]{}\\|;:'\",.<>/?`~";
    
    std::string charPool;
    std::vector<char> requiredChars;

    charPool += lower;
    requiredChars.push_back(lower[0]);  // 临时占位，稍后会替换为随机字符
    
    if (includeMixedCase || settings.requireMixedCase) {
        charPool += upper;
        requiredChars.push_back(upper[0]);  // 临时占位
    }
    
    if (includeNumbers || settings.requireNumbers) {
        charPool += digits;
        requiredChars.push_back(digits[0]);  // 临时占位
    }
    
    if (includeSpecial || settings.requireSpecialChars) {
        charPool += special;
        requiredChars.push_back(special[0]);  // 临时占位
    }

    if (charPool.empty()) {
        LOG_F(ERROR, "No character set selected for password generation");
        return "";
    }

    // 使用C++随机引擎来获得更好的可移植性和控制
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> pool_dist(0, charPool.length() - 1);
    std::uniform_int_distribution<size_t> lower_dist(0, lower.length() - 1);
    std::uniform_int_distribution<size_t> upper_dist(0, upper.length() - 1);
    std::uniform_int_distribution<size_t> digit_dist(0, digits.length() - 1);
    std::uniform_int_distribution<size_t> special_dist(0, special.length() - 1);

    std::string password(length, ' ');
    size_t requiredCount = requiredChars.size();

    // 首先填充必需的字符
    requiredChars[0] = lower[lower_dist(generator)];
    size_t reqIdx = 1;
    
    if (includeMixedCase || settings.requireMixedCase) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = upper[upper_dist(generator)];
        }
    }

    if (includeNumbers || settings.requireNumbers) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = digits[digit_dist(generator)];
        }
    }

    if (includeSpecial || settings.requireSpecialChars) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = special[special_dist(generator)];
        }
    }

    // 填充密码的剩余长度
    for (int i = 0; i < length; ++i) {
        password[i] = charPool[pool_dist(generator)];
    }

    // 将必需的字符随机放入密码中
    std::vector<size_t> positions(length);
    std::iota(positions.begin(), positions.end(), 0);
    std::shuffle(positions.begin(), positions.end(), generator);

    for (size_t i = 0; i < requiredCount && i < static_cast<size_t>(length); ++i) {
        password[positions[i]] = requiredChars[i];
    }

    // 最后再次打乱整个密码
    std::shuffle(password.begin(), password.end(), generator);

    LOG_F(INFO, "Generated password of length {}", length);
    return password;
}

// 实现密码强度评估
PasswordStrength PasswordManager::evaluatePasswordStrength(std::string_view password) const {
    // 无需锁定进行评估，这是一个const方法
    // updateActivity(); // 读取强度可能不算作活动

    const size_t len = password.length();
    if (len == 0) {
        return PasswordStrength::VeryWeak;
    }

    int score = 0;
    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // 熵近似评分（非常粗略）
    if (len >= 8) {
        score += 1;
    }

    if (len >= 12) {
        score += 1;
    }

    if (len >= 16) {
        score += 1;
    }

    // 检查字符类型
    for (char c : password) {
        if (!hasLower && std::islower(static_cast<unsigned char>(c))) {
            hasLower = true;
        } else if (!hasUpper && std::isupper(static_cast<unsigned char>(c))) {
            hasUpper = true;
        } else if (!hasDigit && std::isdigit(static_cast<unsigned char>(c))) {
            hasDigit = true;
        } else if (!hasSpecial && !std::isalnum(static_cast<unsigned char>(c))) {
            hasSpecial = true;
        }
        
        // 如果已找到所有类型，可以提前结束循环
        if (hasLower && hasUpper && hasDigit && hasSpecial) {
            break;
        }
    }

    int charTypes = 0;
    if (hasLower) {
        charTypes++;
    }

    if (hasUpper) {
        charTypes++;
    }

    if (hasDigit) {
        charTypes++;
    }

    if (hasSpecial) {
        charTypes++;
    }

    // 根据字符类型加分
    if (charTypes >= 2) {
        score += 1;
    }

    if (charTypes >= 3) {
        score += 1;
    }

    if (charTypes >= 4) {
        score += 1;
    }

    // 对常见模式的惩罚（简单检查）
    try {
        // 检查是否全是数字
        if (std::regex_match(std::string(password), std::regex("^\\d+$"))) {
            score -= 1;
        }
        
        // 检查是否全是字母
        if (std::regex_match(std::string(password), std::regex("^[a-zA-Z]+$"))) {
            score -= 1;
        }
        
        // 检查重复字符（如果超过25%的字符是相同的）
        std::map<char, int> charCount;
        for (char c : password) {
            charCount[c]++;
        }
        
        for (const auto& [_, count] : charCount) {
            if (static_cast<double>(count) / len > 0.25) {
                score -= 1;
                break;
            }
        }
        
        // 检查键盘顺序（简单版本）
        const std::string qwertyRows[] = {
            "qwertyuiop", "asdfghjkl", "zxcvbnm"
        };
        
        std::string lowerPass = std::string(password);
        std::transform(lowerPass.begin(), lowerPass.end(), lowerPass.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        for (const auto& row : qwertyRows) {
            for (size_t i = 0; i <= row.length() - 3; ++i) {
                std::string pattern = row.substr(i, 3);
                if (lowerPass.find(pattern) != std::string::npos) {
                    score -= 1;
                    break;
                }
            }
        }
    } catch (const std::regex_error& e) {
        LOG_F(ERROR, "Regex error in password strength evaluation: {}", e.what());
        // 不要因为正则表达式错误而使整个评估失败
    }

    // 将分数映射到强度等级
    if (score <= 1) {
        return PasswordStrength::VeryWeak;
    }

    if (score == 2) {
        return PasswordStrength::Weak;
    }

    if (score == 3) {
        return PasswordStrength::Medium;
    }

    if (score == 4) {
        return PasswordStrength::Strong;
    }

    // score >= 5
    return PasswordStrength::VeryStrong;
}

// 实现密码导出功能
bool PasswordManager::exportPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot export passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Export password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 确保缓存已完全加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for export");
            return false;
        }
        
        // 准备导出数据
        nlohmann::json exportData;
        exportData["version"] = PM_VERSION;
        exportData["entries"] = nlohmann::json::array();
        
        // 添加所有密码条目
        for (const auto& [key, entry] : cachedPasswords) {
            nlohmann::json entryJson;
            entryJson["platform_key"] = key;
            entryJson["title"] = entry.title;
            entryJson["username"] = entry.username;
            entryJson["password"] = entry.password; // 未加密状态下的密码
            entryJson["url"] = entry.url;
            entryJson["notes"] = entry.notes;
            entryJson["category"] = static_cast<int>(entry.category);
            entryJson["tags"] = entry.tags;
            entryJson["created"] = std::chrono::system_clock::to_time_t(entry.created);
            entryJson["modified"] = std::chrono::system_clock::to_time_t(entry.modified);
            entryJson["expires"] = std::chrono::system_clock::to_time_t(entry.expires);
            
            // 添加前一个密码版本的历史记录
            nlohmann::json previousJson = nlohmann::json::array();
            for (const auto& prevPwd : entry.previousPasswords) {
                nlohmann::json pwdJson;
                pwdJson["password"] = prevPwd.password;
                pwdJson["changed"] = std::chrono::system_clock::to_time_t(prevPwd.changed);
                previousJson.push_back(pwdJson);
            }
            entryJson["previous_passwords"] = previousJson;
            
            // 添加到导出数据中
            exportData["entries"].push_back(entryJson);
        }
        
        // 序列化导出数据
        std::string serializedData = exportData.dump(2); // 使用2空格缩进

        // 生成盐和IV
        std::vector<unsigned char> salt(PM_SALT_SIZE);
        std::vector<unsigned char> iv(PM_IV_SIZE);
        
        if (RAND_bytes(salt.data(), salt.size()) != 1 ||
            RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random data for export encryption");
            return false;
        }
        
        // 从导出密码派生密钥
        std::vector<unsigned char> exportKey = deriveKey(password, salt, DEFAULT_PBKDF2_ITERATIONS);
        
        // 使用AES-GCM加密序列化数据
        std::vector<unsigned char> encryptedData;
        std::vector<unsigned char> tag(PM_TAG_SIZE);
        
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              exportKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        encryptedData.resize(serializedData.size() + EVP_MAX_BLOCK_LENGTH);
        int outLen = 0;
        
        if (EVP_EncryptUpdate(ctx.get(), encryptedData.data(), &outLen,
                             reinterpret_cast<const unsigned char*>(serializedData.data()),
                             serializedData.size()) != 1) {
            LOG_F(ERROR, "Failed to encrypt data for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to finalize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        outLen += finalLen;
        encryptedData.resize(outLen);
        
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to get authentication tag for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        // 构建最终的导出文件结构
        nlohmann::json exportFile;
        exportFile["format"] = "ATOM_PASSWORD_EXPORT";
        exportFile["version"] = PM_VERSION;
        exportFile["salt"] = algorithm::base64Encode(salt)->to_string();
        exportFile["iv"] = algorithm::base64Encode(iv)->to_string();
        exportFile["tag"] = algorithm::base64Encode(tag)->to_string();
        exportFile["iterations"] = DEFAULT_PBKDF2_ITERATIONS;
        exportFile["data"] = algorithm::base64Encode(encryptedData)->to_string();
        
        // 写入导出文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open export file for writing: {}", filePath.string());
            secureWipe(exportKey);
            return false;
        }
        
        outFile << exportFile.dump(2);
        outFile.close();
        
        // 安全擦除导出密钥
        secureWipe(exportKey);
        
        LOG_F(INFO, "Successfully exported {} password entries to {}", 
              cachedPasswords.size(), filePath.string());
        
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Export passwords error: {}", e.what());
        return false;
    }
}

// 实现密码导入功能
bool PasswordManager::importPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot import passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Import password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 读取导入文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open import file for reading: {}", filePath.string());
            return false;
        }
        
        std::string fileContents((std::istreambuf_iterator<char>(inFile)), 
                                std::istreambuf_iterator<char>());
        inFile.close();
        
        if (fileContents.empty()) {
            LOG_F(ERROR, "Import file is empty: {}", filePath.string());
            return false;
        }
        
        // 解析导入文件JSON
        nlohmann::json importFile = nlohmann::json::parse(fileContents);
        
        // 验证文件格式
        if (!importFile.contains("format") || 
            importFile["format"] != "ATOM_PASSWORD_EXPORT") {
            LOG_F(ERROR, "Invalid import file format");
            return false;
        }
        
        // 提取加密参数
        std::vector<unsigned char> salt = 
            algorithm::base64Decode(importFile["salt"].get<std::string>()).value();
        std::vector<unsigned char> iv = 
            algorithm::base64Decode(importFile["iv"].get<std::string>()).value();
        std::vector<unsigned char> tag = 
            algorithm::base64Decode(importFile["tag"].get<std::string>()).value();
        std::vector<unsigned char> encryptedData = 
            algorithm::base64Decode(importFile["data"].get<std::string>()).value();
        int iterations = importFile["iterations"].get<int>();
        
        // 从导入密码派生密钥
        std::vector<unsigned char> importKey = deriveKey(password, salt, iterations);
        
        // 使用AES-GCM解密数据
        std::vector<unsigned char> decryptedData(encryptedData.size());
        int outLen = 0;
        
        SslCipherContext ctx;
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              importKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize decryption for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        // 设置预期的标签
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to set authentication tag for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        if (EVP_DecryptUpdate(ctx.get(), decryptedData.data(), &outLen,
                             encryptedData.data(), encryptedData.size()) != 1) {
            LOG_F(ERROR, "Failed to decrypt data for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx.get(), decryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to verify imported data: Incorrect password or corrupted data");
            secureWipe(importKey);
            return false;
        }
        
        outLen += finalLen;
        decryptedData.resize(outLen);
        
        // 安全擦除导入密钥
        secureWipe(importKey);
        
        // 解析解密后的JSON数据
        std::string decryptedJson(decryptedData.begin(), decryptedData.end());
        nlohmann::json importData = nlohmann::json::parse(decryptedJson);
        
        if (!importData.contains("entries") || !importData["entries"].is_array()) {
            LOG_F(ERROR, "Import file has invalid structure: missing entries array");
            return false;
        }
        
        // 导入每个密码条目
        int importedCount = 0;
        int skippedCount = 0;
        
        for (const auto& entryJson : importData["entries"]) {
            // 创建密码条目
            PasswordEntry entry;
            std::string platformKey = entryJson["platform_key"].get<std::string>();
            
            // 填充条目数据
            entry.title = entryJson["title"].get<std::string>();
            entry.username = entryJson["username"].get<std::string>();
            entry.password = entryJson["password"].get<std::string>();
            entry.url = entryJson["url"].get<std::string>();
            
            if (entryJson.contains("notes")) {
                entry.notes = entryJson["notes"].get<std::string>();
            }
            
            if (entryJson.contains("category")) {
                entry.category = static_cast<PasswordCategory>(entryJson["category"].get<int>());
            }
            
            if (entryJson.contains("tags") && entryJson["tags"].is_array()) {
                entry.tags = entryJson["tags"].get<std::vector<std::string>>();
            }
            
            // 转换时间戳
            if (entryJson.contains("created")) {
                std::time_t created = entryJson["created"].get<std::time_t>();
                entry.created = std::chrono::system_clock::from_time_t(created);
            } else {
                entry.created = std::chrono::system_clock::now();
            }
            
            if (entryJson.contains("modified")) {
                std::time_t modified = entryJson["modified"].get<std::time_t>();
                entry.modified = std::chrono::system_clock::from_time_t(modified);
            } else {
                entry.modified = std::chrono::system_clock::now();
            }
            
            if (entryJson.contains("expires")) {
                std::time_t expires = entryJson["expires"].get<std::time_t>();
                entry.expires = std::chrono::system_clock::from_time_t(expires);
            }
            
            // 导入历史密码
            if (entryJson.contains("previous_passwords") && 
                entryJson["previous_passwords"].is_array()) {
                for (const auto& prevJson : entryJson["previous_passwords"]) {
                    PreviousPassword prev;
                    prev.password = prevJson["password"].get<std::string>();
                    
                    if (prevJson.contains("changed")) {
                        std::time_t changed = prevJson["changed"].get<std::time_t>();
                        prev.changed = std::chrono::system_clock::from_time_t(changed);
                    } else {
                        prev.changed = std::chrono::system_clock::now();
                    }
                    
                    entry.previousPasswords.push_back(prev);
                }
            }
            
            // 导入策略：跳过或覆盖已存在的条目
            bool skipExisting = false; // 可以通过参数控制是否跳过已存在的条目
            
            if (skipExisting && cachedPasswords.find(platformKey) != cachedPasswords.end()) {
                skippedCount++;
                continue;
            }
            
            // 存储密码条目
            if (storePassword(platformKey, entry)) {
                importedCount++;
            } else {
                LOG_F(WARNING, "Failed to import password entry: {}", platformKey);
                skippedCount++;
            }
        }
        
        LOG_F(INFO, "Import complete: {} entries imported, {} entries skipped",
              importedCount, skippedCount);
        
        return importedCount > 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Import passwords error: {}", e.what());
        return false;
    }
}

// 实现检查过期密码的方法
std::vector<std::string> PasswordManager::checkExpiredPasswords() {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot check expired passwords: PasswordManager is locked");
        return {};
    }

    if (!settings.notifyOnPasswordExpiry || settings.passwordExpiryDays <= 0) {
        LOG_F(INFO, "Password expiry checking is disabled");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for expiry check");
            return {};
        }
        
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        
        // 计算警告阈值（即将过期的天数）
        auto warningThreshold = std::chrono::hours(settings.passwordExpiryDays * 24);
        
        // 存储即将过期的密码键
        std::vector<std::string> expiredKeys;
        
        for (const auto& [key, entry] : cachedPasswords) {
            // 如果密码有明确的过期时间
            if (entry.expires != std::chrono::system_clock::time_point{}) {
                if (entry.expires <= now) {
                    // 密码已经过期
                    expiredKeys.push_back(key);
                    continue;
                }
            }
            
            // 根据最后修改时间和过期策略检查
            auto lastModified = entry.modified;
            if (lastModified == std::chrono::system_clock::time_point{}) {
                // 如果没有修改时间，使用创建时间
                lastModified = entry.created;
            }
            
            // 如果密码年龄超过了阈值
            if (now - lastModified >= warningThreshold) {
                expiredKeys.push_back(key);
            }
        }
        
        LOG_F(INFO, "Found {} expired or soon-to-expire passwords", expiredKeys.size());
        return expiredKeys;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Check expired passwords error: {}", e.what());
        return {};
    }
}

// 实现Windows平台特定的存储方法
#if defined(_WIN32)
bool PasswordManager::storeToWindowsCredentialManager(
    std::string_view target, std::string_view encryptedData) const {
    // 无需锁定，这是一个const方法访问外部系统

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    cred.TargetName = const_cast<LPWSTR>(wideTarget.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(encryptedData.length());
    // CredentialBlob需要非const指针
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(encryptedData.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    // 使用固定的、非敏感的用户名或派生一个？为简单起见使用固定值。
    static const std::wstring pmUser = L"AtomPasswordManagerUser";
    cred.UserName = const_cast<LPWSTR>(pmUser.c_str());

    if (CredWriteW(&cred, 0)) {
        LOG_F(INFO, "Successfully stored data to Windows Credential Manager for target: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        LOG_F(ERROR, "Failed to store data to Windows Credential Manager: Error code {}", lastError);
        return false;
    }
}

std::string PasswordManager::retrieveFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    PCREDENTIALW pCred = nullptr;
    std::string result = "";

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for retrieval");
        return result;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        if (pCred) {
            if (pCred->CredentialBlobSize > 0 && pCred->CredentialBlob) {
                // 将凭据数据转换为std::string
                result = std::string(
                    reinterpret_cast<const char*>(pCred->CredentialBlob),
                    pCred->CredentialBlobSize);
            }
            CredFree(pCred);
        }
        return result;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            LOG_F(INFO, "No credential found in Windows Credential Manager for target: {}",
                  std::string(target).c_str());
        } else {
            LOG_F(ERROR, "Failed to retrieve data from Windows Credential Manager: Error code {}",
                  lastError);
        }
        return result;
    }
}

bool PasswordManager::deleteFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for deletion");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        LOG_F(INFO, "Successfully deleted credential from Windows Credential Manager: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            // 如果凭据不存在，也视为删除成功（幂等操作）
            LOG_F(INFO, "No credential found to delete in Windows Credential Manager: {}",
                  std::string(target).c_str());
            return true;
        } else {
            LOG_F(ERROR, "Failed to delete credential from Windows Credential Manager: Error code {}",
                  lastError);
            return false;
        }
    }
}

std::vector<std::string> PasswordManager::getAllWindowsCredentials() const {
    // 无需锁定，这是一个const方法访问外部系统

    std::vector<std::string> results;
    DWORD count = 0;
    PCREDENTIALW* pCredentials = nullptr;

    // 枚举匹配模式的凭据（例如，"AtomPasswordManager*"）
    // 使用通配符可能需要特定的权限或配置。
    // 更安全的方法可能是存储一个索引凭据。
    // 为简单起见，假设枚举有效或使用与文件后备相似的索引方法。
    // 使用固定前缀进行枚举：
    std::wstring filter =
        std::wstring(PM_SERVICE_NAME.begin(), PM_SERVICE_NAME.end()) +
        L"*";

    if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
        // 处理枚举结果
        for (DWORD i = 0; i < count; i++) {
            if (pCredentials[i]) {
                // 将宽字符目标名称转换为UTF-8字符串
                int targetLen = WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                                   nullptr, 0, nullptr, nullptr);
                if (targetLen > 0) {
                    std::string targetName(targetLen - 1, 0); // 减1是为了不包括结尾的null
                    WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                       &targetName[0], targetLen, nullptr, nullptr);
                    
                    // 根据需要从目标名称中提取实际键
                    // 例如，如果目标名称格式为"AtomPasswordManager_key"，则去除前缀
                    std::string prefix = std::string(PM_SERVICE_NAME) + "_";
                    if (targetName.find(prefix) == 0) {
                        results.push_back(targetName.substr(prefix.length()));
                    } else {
                        // 没有预期的前缀，使用完整目标名称
                        results.push_back(targetName);
                    }
                }
            }
        }
        CredFree(pCredentials);
    } else {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) { // 没找到不是错误
            LOG_F(ERROR, "Failed to enumerate Windows credentials: Error code {}", lastError);
        }
    }
    return results;
}
#endif // defined(_WIN32)

#if defined(__APPLE__)
// 实现macOS平台特定的存储方法

// 辅助函数用于macOS状态码
std::string GetMacOSStatusString(OSStatus status) {
    // 考虑使用SecCopyErrorMessageString来获得更具描述性的错误（如果可用）
    return "macOS Error: " + std::to_string(status);
}

bool PasswordManager::storeToMacKeychain(std::string_view service,
                                         std::string_view account,
                                         std::string_view encryptedData) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);
    CFDataRef cfData =
        CFDataCreate(kCFAllocatorDefault,
                     reinterpret_cast<const UInt8*>(encryptedData.data()),
                     encryptedData.length());

    if (!cfService || !cfAccount || !cfData) {
        LOG_F(ERROR, "Failed to create CF objects for keychain storage");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        if (cfData) CFRelease(cfData);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    // 先检查项目是否已存在
    OSStatus status = SecItemCopyMatching(query, nullptr);
    if (status == errSecSuccess) {
        // 项目已存在，更新它
        CFMutableDictionaryRef updateDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(updateDict, kSecValueData, cfData);
        status = SecItemUpdate(query, updateDict);
        CFRelease(updateDict);
    } else if (status == errSecItemNotFound) {
        // 项目不存在，添加它
        CFDictionarySetValue(query, kSecValueData, cfData);
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);
    CFRelease(cfData);

    if (status != errSecSuccess) {
        LOG_F(ERROR, "Failed to store data to macOS Keychain: {}", GetMacOSStatusString(status));
        return false;
    }

    LOG_F(INFO, "Successfully stored data to macOS Keychain for service:{}/account:{}",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::string PasswordManager::retrieveFromMacKeychain(
    std::string_view service, std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain retrieval");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return "";
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    
    CFDataRef cfData = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfData);

    std::string result = "";
    if (status == errSecSuccess && cfData) {
        // 从CFData转换为std::string
        CFIndex length = CFDataGetLength(cfData);
        if (length > 0) {
            const UInt8* bytes = CFDataGetBytePtr(cfData);
            result = std::string(reinterpret_cast<const char*>(bytes), length);
        }
        CFRelease(cfData);
    } else if (status != errSecItemNotFound) {
        // 未找到项目不是错误，只是返回空字符串
        LOG_F(ERROR, "Failed to retrieve data from macOS Keychain: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    return result;
}

bool PasswordManager::deleteFromMacKeychain(std::string_view service,
                                            std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain deletion");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    OSStatus status = SecItemDelete(query);

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to delete item from macOS Keychain: {}", 
              GetMacOSStatusString(status));
        return false;
    }

    // 返回true如果删除成功或未找到（幂等）
    LOG_F(INFO, "Successfully deleted or confirmed absence of keychain item (service:{}/account:{})",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::vector<std::string> PasswordManager::getAllMacKeychainItems(
    std::string_view service) const {
    // 无需锁定

    std::vector<std::string> results;
    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    
    if (!cfService) {
        LOG_F(ERROR, "Failed to create CF string for keychain enumeration");
        return results;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    
    CFArrayRef cfResults = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfResults);

    if (status == errSecSuccess && cfResults) {
        // 处理匹配项列表
        CFIndex count = CFArrayGetCount(cfResults);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(cfResults, i);
            CFStringRef account = (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            
            if (account) {
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(account), 
                                                                  kCFStringEncodingUTF8) + 1;
                char* buffer = (char*)malloc(maxSize);
                
                if (buffer && CFStringGetCString(account, buffer, maxSize, kCFStringEncodingUTF8)) {
                    results.push_back(std::string(buffer));
                }
                
                if (buffer) {
                    free(buffer);
                }
            }
        }
        CFRelease(cfResults);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to enumerate macOS Keychain items: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);

    return results;
}
#endif // defined(__APPLE__)

#if defined(__linux__) && defined(USE_LIBSECRET)
// 实现Linux平台下的libsecret存储方法

bool PasswordManager::storeToLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name,
    std::string_view encryptedData) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 使用标签和属性存储密码
    gboolean success = secret_password_store_sync(
        &schema,
        SECRET_COLLECTION_DEFAULT,
        attribute_name.data(),
        encryptedData.data(),
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success) {
        if (error) {
            LOG_F(ERROR, "Failed to store data to Linux keyring: %s", error->message);
            g_error_free(error);
        } else {
            LOG_F(ERROR, "Failed to store data to Linux keyring: Unknown error");
        }
        return false;
    }
    
    LOG_F(INFO, "Data stored successfully in Linux keyring (Schema: {}, Key: {})",
          std::string(schema_name).c_str(), std::string(attribute_name).c_str());
    return true;
}

std::string PasswordManager::retrieveFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 根据属性查找密码
    gchar* secret = secret_password_lookup_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    std::string result = "";
    if (secret) {
        result = std::string(secret);
        secret_password_free(secret);
    } else if (error) {
        LOG_F(ERROR, "Failed to retrieve data from Linux keyring: %s", error->message);
        g_error_free(error);
    } else {
        LOG_F(INFO, "No data found in Linux keyring for key: {}", 
              std::string(attribute_name).c_str());
    }
    
    // 如果未找到或出错则返回空字符串
    return result;
}

bool PasswordManager::deleteFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    gboolean success = secret_password_clear_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success && error) {
        LOG_F(ERROR, "Failed to delete data from Linux keyring: %s", error->message);
        g_error_free(error);
        return false;
    }
    
    LOG_F(INFO, "Successfully deleted data from Linux keyring for key: {}", 
          std::string(attribute_name).c_str());
    // 如果删除成功或未找到（幂等操作）则返回true
    return success || !error;
}

std::vector<std::string> PasswordManager::getAllLinuxKeyringItems(
    std::string_view schema_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    std::vector<std::string> results;
    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    // libsecret不提供直接枚举所有项目的方法。
    // 使用已知的索引键来存储键列表。
    std::string indexData = retrieveFromLinuxKeyring(schema_name, "ATOM_PM_INDEX");
    if (!indexData.empty()) {
        try {
            nlohmann::json indexJson = nlohmann::json::parse(indexData);
            if (indexJson.is_array()) {
                for (const auto& key : indexJson) {
                    if (key.is_string()) {
                        results.push_back(key.get<std::string>());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse Linux keyring index: {}", e.what());
        }
    }
    
    return results;
}
#endif // defined(__linux__) && defined(USE_LIBSECRET)

// 文件后备存储实现
#if !defined(_WIN32) && !defined(__APPLE__) && (!defined(__linux__) || !defined(USE_LIBSECRET))
// 如果没有特定平台的实现，使用文件后备

bool PasswordManager::storeToEncryptedFile(
    std::string_view identifier, std::string_view encryptedData) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 确保目录存在
        std::error_code ec;
        if (!std::filesystem::exists(storageDir, ec)) {
            if (!std::filesystem::create_directories(storageDir, ec) && ec) {
                LOG_F(ERROR, "Failed to create storage directory: {}", ec.message());
                return false;
            }
            
            // 在Unix-like系统上设置权限为仅当前用户可访问
            #if !defined(_WIN32)
            chmod(storageDir.c_str(), 0700);
            #endif
        }
        
        // 写入文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filePath.string());
            return false;
        }
        
        outFile.write(encryptedData.data(), encryptedData.size());
        outFile.close();
        
        // 更新索引文件
        updateEncryptedFileIndex(identifier, true);
        
        LOG_F(INFO, "Data stored successfully to file: {}", filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to store data to file: {}", e.what());
        return false;
    }
}

std::string PasswordManager::retrieveFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return "";
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            LOG_F(INFO, "File not found: {}", filePath.string());
            return "";
        }
        
        // 读取文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open file for reading: {}", filePath.string());
            return "";
        }
        
        // 使用迭代器读取整个文件内容
        std::string fileContents(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>());
        inFile.close();
        
        LOG_F(INFO, "Data retrieved successfully from file: {}", filePath.string());
        return fileContents;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to retrieve data from file: {}", e.what());
        return "";
    }
}

bool PasswordManager::deleteFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 删除文件
        std::error_code ec;
        bool removed = std::filesystem::remove(filePath, ec);
        
        if (ec) {
            LOG_F(ERROR, "Failed to delete file: {}", ec.message());
            return false;
        }
        
        // 更新索引文件
        if (removed) {
            updateEncryptedFileIndex(identifier, false);
            LOG_F(INFO, "File deleted successfully: {}", filePath.string());
        } else {
            LOG_F(INFO, "File not found for deletion: {}", filePath.string());
        }
        
        // 文件存在并被删除或文件不存在都视为成功（幂等操作）
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to delete file: {}", e.what());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllEncryptedFileItems() const {
    // 无需互斥锁，这是一个const方法

    std::vector<std::string> keys;
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return keys;
    }

    std::filesystem::path indexPath = storageDir / "index.json";

    try {
        // 如果索引文件存在，从中读取键列表
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        } else {
            // 索引文件不存在，扫描目录
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(storageDir, ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error scanning directory: {}", ec.message());
                    break;
                }
                
                if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                    std::string filename = entry.path().stem().string();
                    // 此处可以实现反向sanitize逻辑（如果需要）
                    // 简单情况下，直接使用文件名（不带扩展名）
                    keys.push_back(filename);
                }
            }
            
            // 创建索引文件
            try {
                nlohmann::json indexJson = keys;
                std::ofstream outFile(indexPath);
                outFile << indexJson.dump(2);
                outFile.close();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to create index file: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error accessing encrypted files: {}", e.what());
    }

    return keys;
}

// 辅助方法：更新加密文件索引
void PasswordManager::updateEncryptedFileIndex(std::string_view identifier, bool add) const {
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        return;
    }
    
    std::filesystem::path indexPath = storageDir / "index.json";
    std::vector<std::string> keys;
    
    // 读取现有索引（如果存在）
    try {
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        }
        
        // 添加或删除标识符
        std::string idStr(identifier);
        if (add) {
            // 如果不存在，则添加
            if (std::find(keys.begin(), keys.end(), idStr) == keys.end()) {
                keys.push_back(idStr);
            }
        } else {
            // 删除（如果存在）
            keys.erase(std::remove(keys.begin(), keys.end(), idStr), keys.end());
        }
        
        // 写回索引文件
        nlohmann::json indexJson = keys;
        std::ofstream outFile(indexPath);
        outFile << indexJson.dump(2);
        outFile.close();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to update index file: {}", e.what());
    }
}
#endif // 文件后备存储实现

// 实现安全擦除方法的通用模板
template <typename T>
void PasswordManager::secureWipe(T& data) noexcept {
    // 对不同类型数据的安全擦除实现
    if constexpr (std::is_same_v<T, std::vector<unsigned char>> ||
                 std::is_same_v<T, std::vector<char>>) {
        // 标准向量类型（字节数组）
// filepath: d:\msys64\home\qwdma\Atom\atom\secret\password_manager.cpp
#include "password_manager.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <regex>
#include <stdexcept>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include "atom/algorithm/base.hpp"  // 用于base64编码/解码
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// 常量定义
namespace {
constexpr std::string_view PM_VERSION = "1.0.0";
constexpr std::string_view PM_SERVICE_NAME = "AtomPasswordManager";
constexpr size_t PM_SALT_SIZE = 16;
constexpr size_t PM_IV_SIZE = 12;   // AES-GCM标准IV大小
constexpr size_t PM_TAG_SIZE = 16;  // AES-GCM标准标签大小
constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;
constexpr std::string_view VERIFICATION_PREFIX = "ATOM_PM_VERIFICATION_";
}  // namespace

namespace atom::secret {

// 补充searchPasswords中的循环实现
std::vector<std::string> PasswordManager::searchPasswords(std::string_view query) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot search passwords: PasswordManager is locked");
        return {};
    }
    
    if (query.empty()) {
        LOG_F(WARNING, "Empty search query, returning all keys.");
        std::vector<std::string> allKeys;
        allKeys.reserve(cachedPasswords.size());
        for (const auto& pair : cachedPasswords) {
            allKeys.push_back(pair.first);
        }
        return allKeys;
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for search");
            return {};
        }
        
        std::vector<std::string> results;
        std::string lowerQuery(query);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        for (const auto& [key, entry] : cachedPasswords) {
            // 转换为小写进行不区分大小写的搜索
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            // 搜索标题、用户名、网址和标签
            std::string lowerUsername = entry.username;
            std::string lowerUrl = entry.url;
            
            std::transform(lowerUsername.begin(), lowerUsername.end(), lowerUsername.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            if (lowerKey.find(lowerQuery) != std::string::npos ||
                lowerUsername.find(lowerQuery) != std::string::npos ||
                lowerUrl.find(lowerQuery) != std::string::npos) {
                results.push_back(key);
                continue;
            }
        }

        LOG_F(INFO, "Search for '{}' returned {} results", query.data(), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Search passwords error: {}", e.what());
        return {};
    }
}

// 补充filterByCategory中的循环实现
std::vector<std::string> PasswordManager::filterByCategory(PasswordCategory category) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot filter passwords: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for category filtering");
            return {};
        }

        std::vector<std::string> results;
        for (const auto& [key, entry] : cachedPasswords) {
            if (entry.category == category) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Filter by category {} returned {} results",
              static_cast<int>(category), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Filter by category error: {}", e.what());
        return {};
    }
}

// 实现生成密码的方法
std::string PasswordManager::generatePassword(int length, bool includeSpecial,
                                             bool includeNumbers,
                                             bool includeMixedCase) {
    // 无需锁定用于生成，但updateActivity需要锁定
    // 先调用updateActivity
    {
        std::unique_lock lock(mutex);
        if (!isUnlocked.load(std::memory_order_relaxed)) {
            LOG_F(ERROR, "Cannot generate password: PasswordManager is locked");
            return "";
        }
        updateActivity();
    }

    if (length < settings.minPasswordLength) {
        LOG_F(WARNING, "Requested password length {} is less than minimum {}, using minimum",
              length, settings.minPasswordLength);
        length = settings.minPasswordLength;
    }
    if (length <= 0) {
        LOG_F(ERROR, "Invalid password length: {}", length);
        return "";
    }

    // 字符集
    const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string digits = "0123456789";
    const std::string special = "!@#$%^&*()-_=+[]{}\\|;:'\",.<>/?`~";
    
    std::string charPool;
    std::vector<char> requiredChars;

    charPool += lower;
    requiredChars.push_back(lower[0]);  // 临时占位，稍后会替换为随机字符
    
    if (includeMixedCase || settings.requireMixedCase) {
        charPool += upper;
        requiredChars.push_back(upper[0]);  // 临时占位
    }
    
    if (includeNumbers || settings.requireNumbers) {
        charPool += digits;
        requiredChars.push_back(digits[0]);  // 临时占位
    }
    
    if (includeSpecial || settings.requireSpecialChars) {
        charPool += special;
        requiredChars.push_back(special[0]);  // 临时占位
    }

    if (charPool.empty()) {
        LOG_F(ERROR, "No character set selected for password generation");
        return "";
    }

    // 使用C++随机引擎来获得更好的可移植性和控制
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> pool_dist(0, charPool.length() - 1);
    std::uniform_int_distribution<size_t> lower_dist(0, lower.length() - 1);
    std::uniform_int_distribution<size_t> upper_dist(0, upper.length() - 1);
    std::uniform_int_distribution<size_t> digit_dist(0, digits.length() - 1);
    std::uniform_int_distribution<size_t> special_dist(0, special.length() - 1);

    std::string password(length, ' ');
    size_t requiredCount = requiredChars.size();

    // 首先填充必需的字符
    requiredChars[0] = lower[lower_dist(generator)];
    size_t reqIdx = 1;
    
    if (includeMixedCase || settings.requireMixedCase) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = upper[upper_dist(generator)];
        }
    }

    if (includeNumbers || settings.requireNumbers) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = digits[digit_dist(generator)];
        }
    }

    if (includeSpecial || settings.requireSpecialChars) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = special[special_dist(generator)];
        }
    }

    // 填充密码的剩余长度
    for (int i = 0; i < length; ++i) {
        password[i] = charPool[pool_dist(generator)];
    }

    // 将必需的字符随机放入密码中
    std::vector<size_t> positions(length);
    std::iota(positions.begin(), positions.end(), 0);
    std::shuffle(positions.begin(), positions.end(), generator);

    for (size_t i = 0; i < requiredCount && i < static_cast<size_t>(length); ++i) {
        password[positions[i]] = requiredChars[i];
    }

    // 最后再次打乱整个密码
    std::shuffle(password.begin(), password.end(), generator);

    LOG_F(INFO, "Generated password of length {}", length);
    return password;
}

// 实现密码强度评估
PasswordStrength PasswordManager::evaluatePasswordStrength(std::string_view password) const {
    // 无需锁定进行评估，这是一个const方法
    // updateActivity(); // 读取强度可能不算作活动

    const size_t len = password.length();
    if (len == 0) {
        return PasswordStrength::VeryWeak;
    }

    int score = 0;
    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // 熵近似评分（非常粗略）
    if (len >= 8) {
        score += 1;
    }

    if (len >= 12) {
        score += 1;
    }

    if (len >= 16) {
        score += 1;
    }

    // 检查字符类型
    for (char c : password) {
        if (!hasLower && std::islower(static_cast<unsigned char>(c))) {
            hasLower = true;
        } else if (!hasUpper && std::isupper(static_cast<unsigned char>(c))) {
            hasUpper = true;
        } else if (!hasDigit && std::isdigit(static_cast<unsigned char>(c))) {
            hasDigit = true;
        } else if (!hasSpecial && !std::isalnum(static_cast<unsigned char>(c))) {
            hasSpecial = true;
        }
        
        // 如果已找到所有类型，可以提前结束循环
        if (hasLower && hasUpper && hasDigit && hasSpecial) {
            break;
        }
    }

    int charTypes = 0;
    if (hasLower) {
        charTypes++;
    }

    if (hasUpper) {
        charTypes++;
    }

    if (hasDigit) {
        charTypes++;
    }

    if (hasSpecial) {
        charTypes++;
    }

    // 根据字符类型加分
    if (charTypes >= 2) {
        score += 1;
    }

    if (charTypes >= 3) {
        score += 1;
    }

    if (charTypes >= 4) {
        score += 1;
    }

    // 对常见模式的惩罚（简单检查）
    try {
        // 检查是否全是数字
        if (std::regex_match(std::string(password), std::regex("^\\d+$"))) {
            score -= 1;
        }
        
        // 检查是否全是字母
        if (std::regex_match(std::string(password), std::regex("^[a-zA-Z]+$"))) {
            score -= 1;
        }
        
        // 检查重复字符（如果超过25%的字符是相同的）
        std::map<char, int> charCount;
        for (char c : password) {
            charCount[c]++;
        }
        
        for (const auto& [_, count] : charCount) {
            if (static_cast<double>(count) / len > 0.25) {
                score -= 1;
                break;
            }
        }
        
        // 检查键盘顺序（简单版本）
        const std::string qwertyRows[] = {
            "qwertyuiop", "asdfghjkl", "zxcvbnm"
        };
        
        std::string lowerPass = std::string(password);
        std::transform(lowerPass.begin(), lowerPass.end(), lowerPass.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        for (const auto& row : qwertyRows) {
            for (size_t i = 0; i <= row.length() - 3; ++i) {
                std::string pattern = row.substr(i, 3);
                if (lowerPass.find(pattern) != std::string::npos) {
                    score -= 1;
                    break;
                }
            }
        }
    } catch (const std::regex_error& e) {
        LOG_F(ERROR, "Regex error in password strength evaluation: {}", e.what());
        // 不要因为正则表达式错误而使整个评估失败
    }

    // 将分数映射到强度等级
    if (score <= 1) {
        return PasswordStrength::VeryWeak;
    }

    if (score == 2) {
        return PasswordStrength::Weak;
    }

    if (score == 3) {
        return PasswordStrength::Medium;
    }

    if (score == 4) {
        return PasswordStrength::Strong;
    }

    // score >= 5
    return PasswordStrength::VeryStrong;
}

// 实现密码导出功能
bool PasswordManager::exportPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot export passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Export password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 确保缓存已完全加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for export");
            return false;
        }
        
        // 准备导出数据
        nlohmann::json exportData;
        exportData["version"] = PM_VERSION;
        exportData["entries"] = nlohmann::json::array();
        
        // 添加所有密码条目
        for (const auto& [key, entry] : cachedPasswords) {
            nlohmann::json entryJson;
            entryJson["platform_key"] = key;
            entryJson["username"] = entry.username;
            entryJson["password"] = entry.password; // 未加密状态下的密码
            entryJson["url"] = entry.url;
            entryJson["notes"] = entry.notes;
            entryJson["category"] = static_cast<int>(entry.category);
            
            // 添加到导出数据中
            exportData["entries"].push_back(entryJson);
        }
        
        // 序列化导出数据
        std::string serializedData = exportData.dump(2); // 使用2空格缩进

        // 生成盐和IV
        std::vector<unsigned char> salt(PM_SALT_SIZE);
        std::vector<unsigned char> iv(PM_IV_SIZE);
        
        if (RAND_bytes(salt.data(), salt.size()) != 1 ||
            RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random data for export encryption");
            return false;
        }
        
        // 从导出密码派生密钥
        std::vector<unsigned char> exportKey = deriveKey(password, salt, DEFAULT_PBKDF2_ITERATIONS);
        
        // 使用AES-GCM加密序列化数据
        std::vector<unsigned char> encryptedData;
        std::vector<unsigned char> tag(PM_TAG_SIZE);
        
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              exportKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        encryptedData.resize(serializedData.size() + EVP_MAX_BLOCK_LENGTH);
        int outLen = 0;
        
        if (EVP_EncryptUpdate(ctx.get(), encryptedData.data(), &outLen,
                             reinterpret_cast<const unsigned char*>(serializedData.data()),
                             serializedData.size()) != 1) {
            LOG_F(ERROR, "Failed to encrypt data for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to finalize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        outLen += finalLen;
        encryptedData.resize(outLen);
        
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to get authentication tag for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        // 构建最终的导出文件结构
        nlohmann::json exportFile;
        exportFile["format"] = "ATOM_PASSWORD_EXPORT";
        exportFile["version"] = PM_VERSION;
        
        // 修复base64编码部分，将vector<unsigned char>显式转换为std::string
        std::string saltBase64 = algorithm::base64Encode(salt);
        std::string ivBase64 = algorithm::base64Encode(iv);
        std::string tagBase64 = algorithm::base64Encode(tag);
        std::string dataBase64 = algorithm::base64Encode(encryptedData);
        
        exportFile["salt"] = saltBase64;
        exportFile["iv"] = ivBase64;
        exportFile["tag"] = tagBase64;
        exportFile["iterations"] = DEFAULT_PBKDF2_ITERATIONS;
        exportFile["data"] = dataBase64;
        
        // 写入导出文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open export file for writing: {}", filePath.string());
            secureWipe(exportKey);
            return false;
        }
        
        outFile << exportFile.dump(2);
        outFile.close();
        
        // 安全擦除导出密钥
        secureWipe(exportKey);
        
        LOG_F(INFO, "Successfully exported {} password entries to {}", 
              cachedPasswords.size(), filePath.string());
        
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Export passwords error: {}", e.what());
        return false;
    }
}

// 实现密码导入功能
bool PasswordManager::importPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot import passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Import password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 读取导入文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open import file for reading: {}", filePath.string());
            return false;
        }
        
        std::string fileContents((std::istreambuf_iterator<char>(inFile)), 
                                std::istreambuf_iterator<char>());
        inFile.close();
        
        if (fileContents.empty()) {
            LOG_F(ERROR, "Import file is empty: {}", filePath.string());
            return false;
        }
        
        // 解析导入文件JSON
        nlohmann::json importFile = nlohmann::json::parse(fileContents);
        
        // 验证文件格式
        if (!importFile.contains("format") || 
            importFile["format"] != "ATOM_PASSWORD_EXPORT") {
            LOG_F(ERROR, "Invalid import file format");
            return false;
        }
        
        // 提取加密参数
        std::vector<unsigned char> salt = 
            algorithm::base64Decode(importFile["salt"].get<std::string>());
        std::vector<unsigned char> iv = 
            algorithm::base64Decode(importFile["iv"].get<std::string>());
        std::vector<unsigned char> tag = 
            algorithm::base64Decode(importFile["tag"].get<std::string>());
        std::vector<unsigned char> encryptedData = 
            algorithm::base64Decode(importFile["data"].get<std::string>());
        int iterations = importFile["iterations"].get<int>();
        
        // 从导入密码派生密钥
        std::vector<unsigned char> importKey = deriveKey(password, salt, iterations);
        
        // 使用AES-GCM解密数据
        std::vector<unsigned char> decryptedData(encryptedData.size());
        int outLen = 0;
        
        SslCipherContext ctx;
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              importKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize decryption for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        // 设置预期的标签
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to set authentication tag for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        if (EVP_DecryptUpdate(ctx.get(), decryptedData.data(), &outLen,
                             encryptedData.data(), encryptedData.size()) != 1) {
            LOG_F(ERROR, "Failed to decrypt data for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx.get(), decryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to verify imported data: Incorrect password or corrupted data");
            secureWipe(importKey);
            return false;
        }
        
        outLen += finalLen;
        decryptedData.resize(outLen);
        
        // 安全擦除导入密钥
        secureWipe(importKey);
        
        // 解析解密后的JSON数据
        std::string decryptedJson(decryptedData.begin(), decryptedData.end());
        nlohmann::json importData = nlohmann::json::parse(decryptedJson);
        
        if (!importData.contains("entries") || !importData["entries"].is_array()) {
            LOG_F(ERROR, "Import file has invalid structure: missing entries array");
            return false;
        }
        
        // 导入每个密码条目
        int importedCount = 0;
        int skippedCount = 0;
        
        for (const auto& entryJson : importData["entries"]) {
            // 创建密码条目
            PasswordEntry entry;
            std::string platformKey = entryJson["platform_key"].get<std::string>();
            
            // 填充条目数据
            entry.username = entryJson["username"].get<std::string>();
            entry.password = entryJson["password"].get<std::string>();
            entry.url = entryJson["url"].get<std::string>();
            
            if (entryJson.contains("notes")) {
                entry.notes = entryJson["notes"].get<std::string>();
            }
            
            if (entryJson.contains("category")) {
                entry.category = static_cast<PasswordCategory>(entryJson["category"].get<int>());
            }
            
            // 转换时间戳
            entry.created = std::chrono::system_clock::now();
            entry.modified = std::chrono::system_clock::now();
            
            // 导入策略：跳过或覆盖已存在的条目
            bool skipExisting = false; // 可以通过参数控制是否跳过已存在的条目
            
            if (skipExisting && cachedPasswords.find(platformKey) != cachedPasswords.end()) {
                skippedCount++;
                continue;
            }
            
            // 存储密码条目
            if (storePassword(platformKey, entry)) {
                importedCount++;
            } else {
                LOG_F(WARNING, "Failed to import password entry: {}", platformKey);
                skippedCount++;
            }
        }
        
        LOG_F(INFO, "Import complete: {} entries imported, {} entries skipped",
              importedCount, skippedCount);
        
        return importedCount > 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Import passwords error: {}", e.what());
        return false;
    }
}

// 实现检查过期密码的方法
std::vector<std::string> PasswordManager::checkExpiredPasswords() {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot check expired passwords: PasswordManager is locked");
        return {};
    }

    if (!settings.notifyOnPasswordExpiry || settings.passwordExpiryDays <= 0) {
        LOG_F(INFO, "Password expiry checking is disabled");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for expiry check");
            return {};
        }
        
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        
        // 计算警告阈值（即将过期的天数）
        auto warningThreshold = std::chrono::hours(settings.passwordExpiryDays * 24);
        
        // 存储即将过期的密码键
        std::vector<std::string> expiredKeys;
        
        for (const auto& [key, entry] : cachedPasswords) {
            // 根据最后修改时间和过期策略检查
            auto lastModified = entry.modified;
            if (lastModified == std::chrono::system_clock::time_point{}) {
                // 如果没有修改时间，使用创建时间
                lastModified = entry.created;
            }
            
            // 如果密码年龄超过了阈值
            if (now - lastModified >= warningThreshold) {
                expiredKeys.push_back(key);
            }
        }
        
        LOG_F(INFO, "Found {} expired or soon-to-expire passwords", expiredKeys.size());
        return expiredKeys;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Check expired passwords error: {}", e.what());
        return {};
    }
}

// 实现Windows平台特定的存储方法
#if defined(_WIN32)
bool PasswordManager::storeToWindowsCredentialManager(
    std::string_view target, std::string_view encryptedData) const {
    // 无需锁定，这是一个const方法访问外部系统

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    cred.TargetName = const_cast<LPWSTR>(wideTarget.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(encryptedData.length());
    // CredentialBlob需要非const指针
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(encryptedData.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    // 使用固定的、非敏感的用户名或派生一个？为简单起见使用固定值。
    static const std::wstring pmUser = L"AtomPasswordManagerUser";
    cred.UserName = const_cast<LPWSTR>(pmUser.c_str());

    if (CredWriteW(&cred, 0)) {
        LOG_F(INFO, "Successfully stored data to Windows Credential Manager for target: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        LOG_F(ERROR, "Failed to store data to Windows Credential Manager: Error code {}", lastError);
        return false;
    }
}

std::string PasswordManager::retrieveFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    PCREDENTIALW pCred = nullptr;
    std::string result = "";

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for retrieval");
        return result;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        if (pCred) {
            if (pCred->CredentialBlobSize > 0 && pCred->CredentialBlob) {
                // 将凭据数据转换为std::string
                result = std::string(
                    reinterpret_cast<const char*>(pCred->CredentialBlob),
                    pCred->CredentialBlobSize);
            }
            CredFree(pCred);
        }
        return result;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            LOG_F(INFO, "No credential found in Windows Credential Manager for target: {}",
                  std::string(target).c_str());
        } else {
            LOG_F(ERROR, "Failed to retrieve data from Windows Credential Manager: Error code {}",
                  lastError);
        }
        return result;
    }
}

bool PasswordManager::deleteFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for deletion");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        LOG_F(INFO, "Successfully deleted credential from Windows Credential Manager: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            // 如果凭据不存在，也视为删除成功（幂等操作）
            LOG_F(INFO, "No credential found to delete in Windows Credential Manager: {}",
                  std::string(target).c_str());
            return true;
        } else {
            LOG_F(ERROR, "Failed to delete credential from Windows Credential Manager: Error code {}",
                  lastError);
            return false;
        }
    }
}

std::vector<std::string> PasswordManager::getAllWindowsCredentials() const {
    // 无需锁定，这是一个const方法访问外部系统

    std::vector<std::string> results;
    DWORD count = 0;
    PCREDENTIALW* pCredentials = nullptr;

    // 枚举匹配模式的凭据（例如，"AtomPasswordManager*"）
    // 使用通配符可能需要特定的权限或配置。
    // 更安全的方法可能是存储一个索引凭据。
    // 为简单起见，假设枚举有效或使用与文件后备相似的索引方法。
    // 使用固定前缀进行枚举：
    std::wstring filter =
        std::wstring(PM_SERVICE_NAME.begin(), PM_SERVICE_NAME.end()) +
        L"*";

    if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
        // 处理枚举结果
        for (DWORD i = 0; i < count; i++) {
            if (pCredentials[i]) {
                // 将宽字符目标名称转换为UTF-8字符串
                int targetLen = WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                                   nullptr, 0, nullptr, nullptr);
                if (targetLen > 0) {
                    std::string targetName(targetLen - 1, 0); // 减1是为了不包括结尾的null
                    WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                       &targetName[0], targetLen, nullptr, nullptr);
                    
                    // 根据需要从目标名称中提取实际键
                    // 例如，如果目标名称格式为"AtomPasswordManager_key"，则去除前缀
                    std::string prefix = std::string(PM_SERVICE_NAME) + "_";
                    if (targetName.find(prefix) == 0) {
                        results.push_back(targetName.substr(prefix.length()));
                    } else {
                        // 没有预期的前缀，使用完整目标名称
                        results.push_back(targetName);
                    }
                }
            }
        }
        CredFree(pCredentials);
    } else {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) { // 没找到不是错误
            LOG_F(ERROR, "Failed to enumerate Windows credentials: Error code {}", lastError);
        }
    }
    return results;
}
#endif // defined(_WIN32)

#if defined(__APPLE__)
// 实现macOS平台特定的存储方法

// 辅助函数用于macOS状态码
std::string GetMacOSStatusString(OSStatus status) {
    // 考虑使用SecCopyErrorMessageString来获得更具描述性的错误（如果可用）
    return "macOS Error: " + std::to_string(status);
}

bool PasswordManager::storeToMacKeychain(std::string_view service,
                                         std::string_view account,
                                         std::string_view encryptedData) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);
    CFDataRef cfData =
        CFDataCreate(kCFAllocatorDefault,
                     reinterpret_cast<const UInt8*>(encryptedData.data()),
                     encryptedData.length());

    if (!cfService || !cfAccount || !cfData) {
        LOG_F(ERROR, "Failed to create CF objects for keychain storage");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        if (cfData) CFRelease(cfData);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    // 先检查项目是否已存在
    OSStatus status = SecItemCopyMatching(query, nullptr);
    if (status == errSecSuccess) {
        // 项目已存在，更新它
        CFMutableDictionaryRef updateDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(updateDict, kSecValueData, cfData);
        status = SecItemUpdate(query, updateDict);
        CFRelease(updateDict);
    } else if (status == errSecItemNotFound) {
        // 项目不存在，添加它
        CFDictionarySetValue(query, kSecValueData, cfData);
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);
    CFRelease(cfData);

    if (status != errSecSuccess) {
        LOG_F(ERROR, "Failed to store data to macOS Keychain: {}", GetMacOSStatusString(status));
        return false;
    }

    LOG_F(INFO, "Successfully stored data to macOS Keychain for service:{}/account:{}",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::string PasswordManager::retrieveFromMacKeychain(
    std::string_view service, std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain retrieval");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return "";
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    
    CFDataRef cfData = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfData);

    std::string result = "";
    if (status == errSecSuccess && cfData) {
        // 从CFData转换为std::string
        CFIndex length = CFDataGetLength(cfData);
        if (length > 0) {
            const UInt8* bytes = CFDataGetBytePtr(cfData);
            result = std::string(reinterpret_cast<const char*>(bytes), length);
        }
        CFRelease(cfData);
    } else if (status != errSecItemNotFound) {
        // 未找到项目不是错误，只是返回空字符串
        LOG_F(ERROR, "Failed to retrieve data from macOS Keychain: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    return result;
}

bool PasswordManager::deleteFromMacKeychain(std::string_view service,
                                            std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain deletion");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    OSStatus status = SecItemDelete(query);

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to delete item from macOS Keychain: {}", 
              GetMacOSStatusString(status));
        return false;
    }

    // 返回true如果删除成功或未找到（幂等）
    LOG_F(INFO, "Successfully deleted or confirmed absence of keychain item (service:{}/account:{})",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::vector<std::string> PasswordManager::getAllMacKeychainItems(
    std::string_view service) const {
    // 无需锁定

    std::vector<std::string> results;
    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    
    if (!cfService) {
        LOG_F(ERROR, "Failed to create CF string for keychain enumeration");
        return results;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    
    CFArrayRef cfResults = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfResults);

    if (status == errSecSuccess && cfResults) {
        // 处理匹配项列表
        CFIndex count = CFArrayGetCount(cfResults);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(cfResults, i);
            CFStringRef account = (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            
            if (account) {
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(account), 
                                                                  kCFStringEncodingUTF8) + 1;
                char* buffer = (char*)malloc(maxSize);
                
                if (buffer && CFStringGetCString(account, buffer, maxSize, kCFStringEncodingUTF8)) {
                    results.push_back(std::string(buffer));
                }
                
                if (buffer) {
                    free(buffer);
                }
            }
        }
        CFRelease(cfResults);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to enumerate macOS Keychain items: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);

    return results;
}
#endif // defined(__APPLE__)

#if defined(__linux__) && defined(USE_LIBSECRET)
// 实现Linux平台下的libsecret存储方法

bool PasswordManager::storeToLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name,
    std::string_view encryptedData) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 使用标签和属性存储密码
    gboolean success = secret_password_store_sync(
        &schema,
        SECRET_COLLECTION_DEFAULT,
        attribute_name.data(),
        encryptedData.data(),
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success) {
        if (error) {
            LOG_F(ERROR, "Failed to store data to Linux keyring: %s", error->message);
            g_error_free(error);
        } else {
            LOG_F(ERROR, "Failed to store data to Linux keyring: Unknown error");
        }
        return false;
    }
    
    LOG_F(INFO, "Data stored successfully in Linux keyring (Schema: {}, Key: {})",
          std::string(schema_name).c_str(), std::string(attribute_name).c_str());
    return true;
}

std::string PasswordManager::retrieveFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 根据属性查找密码
    gchar* secret = secret_password_lookup_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    std::string result = "";
    if (secret) {
        result = std::string(secret);
        secret_password_free(secret);
    } else if (error) {
        LOG_F(ERROR, "Failed to retrieve data from Linux keyring: %s", error->message);
        g_error_free(error);
    } else {
        LOG_F(INFO, "No data found in Linux keyring for key: {}", 
              std::string(attribute_name).c_str());
    }
    
    // 如果未找到或出错则返回空字符串
    return result;
}

bool PasswordManager::deleteFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    gboolean success = secret_password_clear_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success && error) {
        LOG_F(ERROR, "Failed to delete data from Linux keyring: %s", error->message);
        g_error_free(error);
        return false;
    }
    
    LOG_F(INFO, "Successfully deleted data from Linux keyring for key: {}", 
          std::string(attribute_name).c_str());
    // 如果删除成功或未找到（幂等操作）则返回true
    return success || !error;
}

std::vector<std::string> PasswordManager::getAllLinuxKeyringItems(
    std::string_view schema_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    std::vector<std::string> results;
    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    // libsecret不提供直接枚举所有项目的方法。
    // 使用已知的索引键来存储键列表。
    std::string indexData = retrieveFromLinuxKeyring(schema_name, "ATOM_PM_INDEX");
    if (!indexData.empty()) {
        try {
            nlohmann::json indexJson = nlohmann::json::parse(indexData);
            if (indexJson.is_array()) {
                for (const auto& key : indexJson) {
                    if (key.is_string()) {
                        results.push_back(key.get<std::string>());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse Linux keyring index: {}", e.what());
        }
    }
    
    return results;
}
#endif // defined(__linux__) && defined(USE_LIBSECRET)

// 文件后备存储实现
#if !defined(_WIN32) && !defined(__APPLE__) && (!defined(__linux__) || !defined(USE_LIBSECRET))
// 如果没有特定平台的实现，使用文件后备

bool PasswordManager::storeToEncryptedFile(
    std::string_view identifier, std::string_view encryptedData) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 确保目录存在
        std::error_code ec;
        if (!std::filesystem::exists(storageDir, ec)) {
            if (!std::filesystem::create_directories(storageDir, ec) && ec) {
                LOG_F(ERROR, "Failed to create storage directory: {}", ec.message());
                return false;
            }
            
            // 在Unix-like系统上设置权限为仅当前用户可访问
            #if !defined(_WIN32)
            chmod(storageDir.c_str(), 0700);
            #endif
        }
        
        // 写入文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filePath.string());
            return false;
        }
        
        outFile.write(encryptedData.data(), encryptedData.size());
        outFile.close();
        
        // 更新索引文件
        updateEncryptedFileIndex(identifier, true);
        
        LOG_F(INFO, "Data stored successfully to file: {}", filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to store data to file: {}", e.what());
        return false;
    }
}

std::string PasswordManager::retrieveFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return "";
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            LOG_F(INFO, "File not found: {}", filePath.string());
            return "";
        }
        
        // 读取文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open file for reading: {}", filePath.string());
            return "";
        }
        
        // 使用迭代器读取整个文件内容
        std::string fileContents(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>());
        inFile.close();
        
        LOG_F(INFO, "Data retrieved successfully from file: {}", filePath.string());
        return fileContents;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to retrieve data from file: {}", e.what());
        return "";
    }
}

bool PasswordManager::deleteFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 删除文件
        std::error_code ec;
        bool removed = std::filesystem::remove(filePath, ec);
        
        if (ec) {
            LOG_F(ERROR, "Failed to delete file: {}", ec.message());
            return false;
        }
        
        // 更新索引文件
        if (removed) {
            updateEncryptedFileIndex(identifier, false);
            LOG_F(INFO, "File deleted successfully: {}", filePath.string());
        } else {
            LOG_F(INFO, "File not found for deletion: {}", filePath.string());
        }
        
        // 文件存在并被删除或文件不存在都视为成功（幂等操作）
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to delete file: {}", e.what());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllEncryptedFileItems() const {
    // 无需互斥锁，这是一个const方法

    std::vector<std::string> keys;
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return keys;
    }

    std::filesystem::path indexPath = storageDir / "index.json";

    try {
        // 如果索引文件存在，从中读取键列表
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        } else {
            // 索引文件不存在，扫描目录
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(storageDir, ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error scanning directory: {}", ec.message());
                    break;
                }
                
                if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                    std::string filename = entry.path().stem().string();
                    // 此处可以实现反向sanitize逻辑（如果需要）
                    // 简单情况下，直接使用文件名（不带扩展名）
                    keys.push_back(filename);
                }
            }
            
            // 创建索引文件
            try {
                nlohmann::json indexJson = keys;
                std::ofstream outFile(indexPath);
                outFile << indexJson.dump(2);
                outFile.close();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to create index file: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error accessing encrypted files: {}", e.what());
    }

    return keys;
}

// 辅助方法：更新加密文件索引
void PasswordManager::updateEncryptedFileIndex(std::string_view identifier, bool add) const {
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        return;
    }
    
    std::filesystem::path indexPath = storageDir / "index.json";
    std::vector<std::string> keys;
    
    // 读取现有索引（如果存在）
    try {
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        }
        
        // 添加或删除标识符
        std::string idStr(identifier);
        if (add) {
            // 如果不存在，则添加
            if (std::find(keys.begin(), keys.end(), idStr) == keys.end()) {
                keys.push_back(idStr);
            }
        } else {
            // 删除（如果存在）
            keys.erase(std::remove(keys.begin(), keys.end(), idStr), keys.end());
        }
        
        // 写回索引文件
        nlohmann::json indexJson = keys;
        std::ofstream outFile(indexPath);
        outFile << indexJson.dump(2);
        outFile.close();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to update index file: {}", e.what());
    }
}
#endif // 文件后备存储实现

// 实现安全擦除方法的通用模板
template <typename T>
void PasswordManager::secureWipe(T& data) noexcept {
    // 对不同类型数据的安全擦除实现
    if constexpr (std::is_same_v<T, std::vector<unsigned char>> ||
                 std::is_same_v<T, std::vector<char>>) {
        // 标准向量类型（字节数组）
        if (!data.empty()) {
            // 使用随机数据覆盖
            std::memset(data.data(), 0, data.size());
            
            // 可选：多次覆盖使用不同的模式
            #if defined(_WIN32)
            // Windows可能使用SecureZeroMemory，但标准memset应该足够，编译器通常不会优化掉
            #elif defined(__linux__) || defined(__APPLE__)
            // 对于Linux和macOS，volatile指针确保不会被优化掉
            volatile unsigned char* vptr = data.data();
            std::size_t size = data.size();
            for (std::size_t i = 0; i < size; ++i) {
                vptr[i] = 0;
            }
            #endif
            
            // 清空容器
            data.clear();
        }
    } else if constexpr (std::is_pointer_v<T>) {
        // 原始指针处理（需要额外的长度参数，不在此实现）
    } else {
        // 其他类型 - 默认实现
        static_assert(std::is_trivially_destructible_v<T>, 
                     "Only trivially destructible types can be securely wiped with the default implementation");
        std::memset(&data, 0, sizeof(T));
    }
}

// 字符串的特殊处理
template <>
void PasswordManager::secureWipe<std::string>(std::string& data) noexcept {
    if (!data.empty()) {
        // 用零覆盖
        std::memset(&data[0], 0, data.size());
        
        #if defined(_WIN32)
        // Windows使用标准memset
        #elif defined(__linux__) || defined(__APPLE__)
        // 对于Linux和macOS，volatile指针确保不会被优化掉
        volatile char* vptr = &data[0];
        std::size_t size = data.size();
        for (std::size_t i = 0; i < size; ++i) {
            vptr[i] = 0;
        }
        #endif
        
        // 清空字符串
        data.clear();
    }
}

// 实现派生密钥的方法
std::vector<unsigned char> PasswordManager::deriveKey(
    std::string_view masterPassword, std::span<const unsigned char> salt,
    int iterations) const {
    // 无需锁定，这是一个基于输入的const计算

    if (iterations <= 0) {
        LOG_F(WARNING, "Invalid iteration count {}, using default {}", 
              iterations, DEFAULT_PBKDF2_ITERATIONS);
        iterations = DEFAULT_PBKDF2_ITERATIONS;
    }

    std::vector<unsigned char> derivedKey(32); // AES-256需要32字节密钥
    // 使用EVP_KDF函数进行PBKDF2（比PKCS5_PBKDF2_HMAC更推荐）
// filepath: d:\msys64\home\qwdma\Atom\atom\secret\password_manager.cpp
#include "password_manager.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <regex>
#include <stdexcept>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include "atom/algorithm/base.hpp"  // 用于base64编码/解码
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// 常量定义
namespace {
constexpr std::string_view PM_VERSION = "1.0.0";
constexpr std::string_view PM_SERVICE_NAME = "AtomPasswordManager";
constexpr size_t PM_SALT_SIZE = 16;
constexpr size_t PM_IV_SIZE = 12;   // AES-GCM标准IV大小
constexpr size_t PM_TAG_SIZE = 16;  // AES-GCM标准标签大小
constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;
constexpr std::string_view VERIFICATION_PREFIX = "ATOM_PM_VERIFICATION_";
}  // namespace

namespace atom::secret {

// 补充searchPasswords中的循环实现
std::vector<std::string> PasswordManager::searchPasswords(std::string_view query) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot search passwords: PasswordManager is locked");
        return {};
    }
    
    if (query.empty()) {
        LOG_F(WARNING, "Empty search query, returning all keys.");
        std::vector<std::string> allKeys;
        allKeys.reserve(cachedPasswords.size());
        for (const auto& pair : cachedPasswords) {
            allKeys.push_back(pair.first);
        }
        return allKeys;
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for search");
            return {};
        }
        
        std::vector<std::string> results;
        std::string lowerQuery(query);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        for (const auto& [key, entry] : cachedPasswords) {
            // 转换为小写进行不区分大小写的搜索
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            // 搜索标题、用户名、网址和标签
            std::string lowerUsername = entry.username;
            std::string lowerUrl = entry.url;
            
            std::transform(lowerUsername.begin(), lowerUsername.end(), lowerUsername.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            if (lowerKey.find(lowerQuery) != std::string::npos ||
                lowerUsername.find(lowerQuery) != std::string::npos ||
                lowerUrl.find(lowerQuery) != std::string::npos) {
                results.push_back(key);
                continue;
            }
        }

        LOG_F(INFO, "Search for '{}' returned {} results", query.data(), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Search passwords error: {}", e.what());
        return {};
    }
}

// 补充filterByCategory中的循环实现
std::vector<std::string> PasswordManager::filterByCategory(PasswordCategory category) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot filter passwords: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for category filtering");
            return {};
        }

        std::vector<std::string> results;
        for (const auto& [key, entry] : cachedPasswords) {
            if (entry.category == category) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Filter by category {} returned {} results",
              static_cast<int>(category), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Filter by category error: {}", e.what());
        return {};
    }
}

// 实现生成密码的方法
std::string PasswordManager::generatePassword(int length, bool includeSpecial,
                                             bool includeNumbers,
                                             bool includeMixedCase) {
    // 无需锁定用于生成，但updateActivity需要锁定
    // 先调用updateActivity
    {
        std::unique_lock lock(mutex);
        if (!isUnlocked.load(std::memory_order_relaxed)) {
            LOG_F(ERROR, "Cannot generate password: PasswordManager is locked");
            return "";
        }
        updateActivity();
    }

    if (length < settings.minPasswordLength) {
        LOG_F(WARNING, "Requested password length {} is less than minimum {}, using minimum",
              length, settings.minPasswordLength);
        length = settings.minPasswordLength;
    }
    if (length <= 0) {
        LOG_F(ERROR, "Invalid password length: {}", length);
        return "";
    }

    // 字符集
    const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string digits = "0123456789";
    const std::string special = "!@#$%^&*()-_=+[]{}\\|;:'\",.<>/?`~";
    
    std::string charPool;
    std::vector<char> requiredChars;

    charPool += lower;
    requiredChars.push_back(lower[0]);  // 临时占位，稍后会替换为随机字符
    
    if (includeMixedCase || settings.requireMixedCase) {
        charPool += upper;
        requiredChars.push_back(upper[0]);  // 临时占位
    }
    
    if (includeNumbers || settings.requireNumbers) {
        charPool += digits;
        requiredChars.push_back(digits[0]);  // 临时占位
    }
    
    if (includeSpecial || settings.requireSpecialChars) {
        charPool += special;
        requiredChars.push_back(special[0]);  // 临时占位
    }

    if (charPool.empty()) {
        LOG_F(ERROR, "No character set selected for password generation");
        return "";
    }

    // 使用C++随机引擎来获得更好的可移植性和控制
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> pool_dist(0, charPool.length() - 1);
    std::uniform_int_distribution<size_t> lower_dist(0, lower.length() - 1);
    std::uniform_int_distribution<size_t> upper_dist(0, upper.length() - 1);
    std::uniform_int_distribution<size_t> digit_dist(0, digits.length() - 1);
    std::uniform_int_distribution<size_t> special_dist(0, special.length() - 1);

    std::string password(length, ' ');
    size_t requiredCount = requiredChars.size();

    // 首先填充必需的字符
    requiredChars[0] = lower[lower_dist(generator)];
    size_t reqIdx = 1;
    
    if (includeMixedCase || settings.requireMixedCase) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = upper[upper_dist(generator)];
        }
    }

    if (includeNumbers || settings.requireNumbers) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = digits[digit_dist(generator)];
        }
    }

    if (includeSpecial || settings.requireSpecialChars) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = special[special_dist(generator)];
        }
    }

    // 填充密码的剩余长度
    for (int i = 0; i < length; ++i) {
        password[i] = charPool[pool_dist(generator)];
    }

    // 将必需的字符随机放入密码中
    std::vector<size_t> positions(length);
    std::iota(positions.begin(), positions.end(), 0);
    std::shuffle(positions.begin(), positions.end(), generator);

    for (size_t i = 0; i < requiredCount && i < static_cast<size_t>(length); ++i) {
        password[positions[i]] = requiredChars[i];
    }

    // 最后再次打乱整个密码
    std::shuffle(password.begin(), password.end(), generator);

    LOG_F(INFO, "Generated password of length {}", length);
    return password;
}

// 实现密码强度评估
PasswordStrength PasswordManager::evaluatePasswordStrength(std::string_view password) const {
    // 无需锁定进行评估，这是一个const方法
    // updateActivity(); // 读取强度可能不算作活动

    const size_t len = password.length();
    if (len == 0) {
        return PasswordStrength::VeryWeak;
    }

    int score = 0;
    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // 熵近似评分（非常粗略）
    if (len >= 8) {
        score += 1;
    }

    if (len >= 12) {
        score += 1;
    }

    if (len >= 16) {
        score += 1;
    }

    // 检查字符类型
    for (char c : password) {
        if (!hasLower && std::islower(static_cast<unsigned char>(c))) {
            hasLower = true;
        } else if (!hasUpper && std::isupper(static_cast<unsigned char>(c))) {
            hasUpper = true;
        } else if (!hasDigit && std::isdigit(static_cast<unsigned char>(c))) {
            hasDigit = true;
        } else if (!hasSpecial && !std::isalnum(static_cast<unsigned char>(c))) {
            hasSpecial = true;
        }
        
        // 如果已找到所有类型，可以提前结束循环
        if (hasLower && hasUpper && hasDigit && hasSpecial) {
            break;
        }
    }

    int charTypes = 0;
    if (hasLower) {
        charTypes++;
    }

    if (hasUpper) {
        charTypes++;
    }

    if (hasDigit) {
        charTypes++;
    }

    if (hasSpecial) {
        charTypes++;
    }

    // 根据字符类型加分
    if (charTypes >= 2) {
        score += 1;
    }

    if (charTypes >= 3) {
        score += 1;
    }

    if (charTypes >= 4) {
        score += 1;
    }

    // 对常见模式的惩罚（简单检查）
    try {
        // 检查是否全是数字
        if (std::regex_match(std::string(password), std::regex("^\\d+$"))) {
            score -= 1;
        }
        
        // 检查是否全是字母
        if (std::regex_match(std::string(password), std::regex("^[a-zA-Z]+$"))) {
            score -= 1;
        }
        
        // 检查重复字符（如果超过25%的字符是相同的）
        std::map<char, int> charCount;
        for (char c : password) {
            charCount[c]++;
        }
        
        for (const auto& [_, count] : charCount) {
            if (static_cast<double>(count) / len > 0.25) {
                score -= 1;
                break;
            }
        }
        
        // 检查键盘顺序（简单版本）
        const std::string qwertyRows[] = {
            "qwertyuiop", "asdfghjkl", "zxcvbnm"
        };
        
        std::string lowerPass = std::string(password);
        std::transform(lowerPass.begin(), lowerPass.end(), lowerPass.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        for (const auto& row : qwertyRows) {
            for (size_t i = 0; i <= row.length() - 3; ++i) {
                std::string pattern = row.substr(i, 3);
                if (lowerPass.find(pattern) != std::string::npos) {
                    score -= 1;
                    break;
                }
            }
        }
    } catch (const std::regex_error& e) {
        LOG_F(ERROR, "Regex error in password strength evaluation: {}", e.what());
        // 不要因为正则表达式错误而使整个评估失败
    }

    // 将分数映射到强度等级
    if (score <= 1) {
        return PasswordStrength::VeryWeak;
    }

    if (score == 2) {
        return PasswordStrength::Weak;
    }

    if (score == 3) {
        return PasswordStrength::Medium;
    }

    if (score == 4) {
        return PasswordStrength::Strong;
    }

    // score >= 5
    return PasswordStrength::VeryStrong;
}

// 实现密码导出功能
bool PasswordManager::exportPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot export passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Export password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 确保缓存已完全加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for export");
            return false;
        }
        
        // 准备导出数据
        nlohmann::json exportData;
        exportData["version"] = PM_VERSION;
        exportData["entries"] = nlohmann::json::array();
        
        // 添加所有密码条目
        for (const auto& [key, entry] : cachedPasswords) {
            nlohmann::json entryJson;
            entryJson["platform_key"] = key;
            entryJson["username"] = entry.username;
            entryJson["password"] = entry.password; // 未加密状态下的密码
            entryJson["url"] = entry.url;
            entryJson["notes"] = entry.notes;
            entryJson["category"] = static_cast<int>(entry.category);
            
            // 添加到导出数据中
            exportData["entries"].push_back(entryJson);
        }
        
        // 序列化导出数据
        std::string serializedData = exportData.dump(2); // 使用2空格缩进

        // 生成盐和IV
        std::vector<unsigned char> salt(PM_SALT_SIZE);
        std::vector<unsigned char> iv(PM_IV_SIZE);
        
        if (RAND_bytes(salt.data(), salt.size()) != 1 ||
            RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random data for export encryption");
            return false;
        }
        
        // 从导出密码派生密钥
        std::vector<unsigned char> exportKey = deriveKey(password, salt, DEFAULT_PBKDF2_ITERATIONS);
        
        // 使用AES-GCM加密序列化数据
        std::vector<unsigned char> encryptedData;
        std::vector<unsigned char> tag(PM_TAG_SIZE);
        
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              exportKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        encryptedData.resize(serializedData.size() + EVP_MAX_BLOCK_LENGTH);
        int outLen = 0;
        
        if (EVP_EncryptUpdate(ctx.get(), encryptedData.data(), &outLen,
                             reinterpret_cast<const unsigned char*>(serializedData.data()),
                             serializedData.size()) != 1) {
            LOG_F(ERROR, "Failed to encrypt data for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to finalize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        outLen += finalLen;
        encryptedData.resize(outLen);
        
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to get authentication tag for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        // 构建最终的导出文件结构
        nlohmann::json exportFile;
        exportFile["format"] = "ATOM_PASSWORD_EXPORT";
        exportFile["version"] = PM_VERSION;
        
        // 修复base64编码部分，将vector<unsigned char>显式转换为std::string
        std::string saltBase64 = algorithm::base64Encode(salt);
        std::string ivBase64 = algorithm::base64Encode(iv);
        std::string tagBase64 = algorithm::base64Encode(tag);
        std::string dataBase64 = algorithm::base64Encode(encryptedData);
        
        exportFile["salt"] = saltBase64;
        exportFile["iv"] = ivBase64;
        exportFile["tag"] = tagBase64;
        exportFile["iterations"] = DEFAULT_PBKDF2_ITERATIONS;
        exportFile["data"] = dataBase64;
        
        // 写入导出文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open export file for writing: {}", filePath.string());
            secureWipe(exportKey);
            return false;
        }
        
        outFile << exportFile.dump(2);
        outFile.close();
        
        // 安全擦除导出密钥
        secureWipe(exportKey);
        
        LOG_F(INFO, "Successfully exported {} password entries to {}", 
              cachedPasswords.size(), filePath.string());
        
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Export passwords error: {}", e.what());
        return false;
    }
}

// 实现密码导入功能
bool PasswordManager::importPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot import passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Import password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 读取导入文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open import file for reading: {}", filePath.string());
            return false;
        }
        
        std::string fileContents((std::istreambuf_iterator<char>(inFile)), 
                                std::istreambuf_iterator<char>());
        inFile.close();
        
        if (fileContents.empty()) {
            LOG_F(ERROR, "Import file is empty: {}", filePath.string());
            return false;
        }
        
        // 解析导入文件JSON
        nlohmann::json importFile = nlohmann::json::parse(fileContents);
        
        // 验证文件格式
        if (!importFile.contains("format") || 
            importFile["format"] != "ATOM_PASSWORD_EXPORT") {
            LOG_F(ERROR, "Invalid import file format");
            return false;
        }
        
        // 提取加密参数
        std::vector<unsigned char> salt = 
            algorithm::base64Decode(importFile["salt"].get<std::string>());
        std::vector<unsigned char> iv = 
            algorithm::base64Decode(importFile["iv"].get<std::string>());
        std::vector<unsigned char> tag = 
            algorithm::base64Decode(importFile["tag"].get<std::string>());
        std::vector<unsigned char> encryptedData = 
            algorithm::base64Decode(importFile["data"].get<std::string>());
        int iterations = importFile["iterations"].get<int>();
        
        // 从导入密码派生密钥
        std::vector<unsigned char> importKey = deriveKey(password, salt, iterations);
        
        // 使用AES-GCM解密数据
        std::vector<unsigned char> decryptedData(encryptedData.size());
        int outLen = 0;
        
        SslCipherContext ctx;
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              importKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize decryption for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        // 设置预期的标签
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to set authentication tag for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        if (EVP_DecryptUpdate(ctx.get(), decryptedData.data(), &outLen,
                             encryptedData.data(), encryptedData.size()) != 1) {
            LOG_F(ERROR, "Failed to decrypt data for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx.get(), decryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to verify imported data: Incorrect password or corrupted data");
            secureWipe(importKey);
            return false;
        }
        
        outLen += finalLen;
        decryptedData.resize(outLen);
        
        // 安全擦除导入密钥
        secureWipe(importKey);
        
        // 解析解密后的JSON数据
        std::string decryptedJson(decryptedData.begin(), decryptedData.end());
        nlohmann::json importData = nlohmann::json::parse(decryptedJson);
        
        if (!importData.contains("entries") || !importData["entries"].is_array()) {
            LOG_F(ERROR, "Import file has invalid structure: missing entries array");
            return false;
        }
        
        // 导入每个密码条目
        int importedCount = 0;
        int skippedCount = 0;
        
        for (const auto& entryJson : importData["entries"]) {
            // 创建密码条目
            PasswordEntry entry;
            std::string platformKey = entryJson["platform_key"].get<std::string>();
            
            // 填充条目数据
            entry.username = entryJson["username"].get<std::string>();
            entry.password = entryJson["password"].get<std::string>();
            entry.url = entryJson["url"].get<std::string>();
            
            if (entryJson.contains("notes")) {
                entry.notes = entryJson["notes"].get<std::string>();
            }
            
            if (entryJson.contains("category")) {
                entry.category = static_cast<PasswordCategory>(entryJson["category"].get<int>());
            }
            
            // 转换时间戳
            entry.created = std::chrono::system_clock::now();
            entry.modified = std::chrono::system_clock::now();
            
            // 导入策略：跳过或覆盖已存在的条目
            bool skipExisting = false; // 可以通过参数控制是否跳过已存在的条目
            
            if (skipExisting && cachedPasswords.find(platformKey) != cachedPasswords.end()) {
                skippedCount++;
                continue;
            }
            
            // 存储密码条目
            if (storePassword(platformKey, entry)) {
                importedCount++;
            } else {
                LOG_F(WARNING, "Failed to import password entry: {}", platformKey);
                skippedCount++;
            }
        }
        
        LOG_F(INFO, "Import complete: {} entries imported, {} entries skipped",
              importedCount, skippedCount);
        
        return importedCount > 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Import passwords error: {}", e.what());
        return false;
    }
}

// 实现检查过期密码的方法
std::vector<std::string> PasswordManager::checkExpiredPasswords() {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot check expired passwords: PasswordManager is locked");
        return {};
    }

    if (!settings.notifyOnPasswordExpiry || settings.passwordExpiryDays <= 0) {
        LOG_F(INFO, "Password expiry checking is disabled");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for expiry check");
            return {};
        }
        
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        
        // 计算警告阈值（即将过期的天数）
        auto warningThreshold = std::chrono::hours(settings.passwordExpiryDays * 24);
        
        // 存储即将过期的密码键
        std::vector<std::string> expiredKeys;
        
        for (const auto& [key, entry] : cachedPasswords) {
            // 根据最后修改时间和过期策略检查
            auto lastModified = entry.modified;
            if (lastModified == std::chrono::system_clock::time_point{}) {
                // 如果没有修改时间，使用创建时间
                lastModified = entry.created;
            }
            
            // 如果密码年龄超过了阈值
            if (now - lastModified >= warningThreshold) {
                expiredKeys.push_back(key);
            }
        }
        
        LOG_F(INFO, "Found {} expired or soon-to-expire passwords", expiredKeys.size());
        return expiredKeys;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Check expired passwords error: {}", e.what());
        return {};
    }
}

// 实现Windows平台特定的存储方法
#if defined(_WIN32)
bool PasswordManager::storeToWindowsCredentialManager(
    std::string_view target, std::string_view encryptedData) const {
    // 无需锁定，这是一个const方法访问外部系统

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    cred.TargetName = const_cast<LPWSTR>(wideTarget.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(encryptedData.length());
    // CredentialBlob需要非const指针
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(encryptedData.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    // 使用固定的、非敏感的用户名或派生一个？为简单起见使用固定值。
    static const std::wstring pmUser = L"AtomPasswordManagerUser";
    cred.UserName = const_cast<LPWSTR>(pmUser.c_str());

    if (CredWriteW(&cred, 0)) {
        LOG_F(INFO, "Successfully stored data to Windows Credential Manager for target: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        LOG_F(ERROR, "Failed to store data to Windows Credential Manager: Error code {}", lastError);
        return false;
    }
}

std::string PasswordManager::retrieveFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    PCREDENTIALW pCred = nullptr;
    std::string result = "";

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for retrieval");
        return result;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        if (pCred) {
            if (pCred->CredentialBlobSize > 0 && pCred->CredentialBlob) {
                // 将凭据数据转换为std::string
                result = std::string(
                    reinterpret_cast<const char*>(pCred->CredentialBlob),
                    pCred->CredentialBlobSize);
            }
            CredFree(pCred);
        }
        return result;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            LOG_F(INFO, "No credential found in Windows Credential Manager for target: {}",
                  std::string(target).c_str());
        } else {
            LOG_F(ERROR, "Failed to retrieve data from Windows Credential Manager: Error code {}",
                  lastError);
        }
        return result;
    }
}

bool PasswordManager::deleteFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for deletion");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        LOG_F(INFO, "Successfully deleted credential from Windows Credential Manager: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            // 如果凭据不存在，也视为删除成功（幂等操作）
            LOG_F(INFO, "No credential found to delete in Windows Credential Manager: {}",
                  std::string(target).c_str());
            return true;
        } else {
            LOG_F(ERROR, "Failed to delete credential from Windows Credential Manager: Error code {}",
                  lastError);
            return false;
        }
    }
}

std::vector<std::string> PasswordManager::getAllWindowsCredentials() const {
    // 无需锁定，这是一个const方法访问外部系统

    std::vector<std::string> results;
    DWORD count = 0;
    PCREDENTIALW* pCredentials = nullptr;

    // 枚举匹配模式的凭据（例如，"AtomPasswordManager*"）
    // 使用通配符可能需要特定的权限或配置。
    // 更安全的方法可能是存储一个索引凭据。
    // 为简单起见，假设枚举有效或使用与文件后备相似的索引方法。
    // 使用固定前缀进行枚举：
    std::wstring filter =
        std::wstring(PM_SERVICE_NAME.begin(), PM_SERVICE_NAME.end()) +
        L"*";

    if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
        // 处理枚举结果
        for (DWORD i = 0; i < count; i++) {
            if (pCredentials[i]) {
                // 将宽字符目标名称转换为UTF-8字符串
                int targetLen = WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                                   nullptr, 0, nullptr, nullptr);
                if (targetLen > 0) {
                    std::string targetName(targetLen - 1, 0); // 减1是为了不包括结尾的null
                    WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                       &targetName[0], targetLen, nullptr, nullptr);
                    
                    // 根据需要从目标名称中提取实际键
                    // 例如，如果目标名称格式为"AtomPasswordManager_key"，则去除前缀
                    std::string prefix = std::string(PM_SERVICE_NAME) + "_";
                    if (targetName.find(prefix) == 0) {
                        results.push_back(targetName.substr(prefix.length()));
                    } else {
                        // 没有预期的前缀，使用完整目标名称
                        results.push_back(targetName);
                    }
                }
            }
        }
        CredFree(pCredentials);
    } else {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) { // 没找到不是错误
            LOG_F(ERROR, "Failed to enumerate Windows credentials: Error code {}", lastError);
        }
    }
    return results;
}
#endif // defined(_WIN32)

#if defined(__APPLE__)
// 实现macOS平台特定的存储方法

// 辅助函数用于macOS状态码
std::string GetMacOSStatusString(OSStatus status) {
    // 考虑使用SecCopyErrorMessageString来获得更具描述性的错误（如果可用）
    return "macOS Error: " + std::to_string(status);
}

bool PasswordManager::storeToMacKeychain(std::string_view service,
                                         std::string_view account,
                                         std::string_view encryptedData) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);
    CFDataRef cfData =
        CFDataCreate(kCFAllocatorDefault,
                     reinterpret_cast<const UInt8*>(encryptedData.data()),
                     encryptedData.length());

    if (!cfService || !cfAccount || !cfData) {
        LOG_F(ERROR, "Failed to create CF objects for keychain storage");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        if (cfData) CFRelease(cfData);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    // 先检查项目是否已存在
    OSStatus status = SecItemCopyMatching(query, nullptr);
    if (status == errSecSuccess) {
        // 项目已存在，更新它
        CFMutableDictionaryRef updateDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(updateDict, kSecValueData, cfData);
        status = SecItemUpdate(query, updateDict);
        CFRelease(updateDict);
    } else if (status == errSecItemNotFound) {
        // 项目不存在，添加它
        CFDictionarySetValue(query, kSecValueData, cfData);
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);
    CFRelease(cfData);

    if (status != errSecSuccess) {
        LOG_F(ERROR, "Failed to store data to macOS Keychain: {}", GetMacOSStatusString(status));
        return false;
    }

    LOG_F(INFO, "Successfully stored data to macOS Keychain for service:{}/account:{}",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::string PasswordManager::retrieveFromMacKeychain(
    std::string_view service, std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain retrieval");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return "";
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    
    CFDataRef cfData = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfData);

    std::string result = "";
    if (status == errSecSuccess && cfData) {
        // 从CFData转换为std::string
        CFIndex length = CFDataGetLength(cfData);
        if (length > 0) {
            const UInt8* bytes = CFDataGetBytePtr(cfData);
            result = std::string(reinterpret_cast<const char*>(bytes), length);
        }
        CFRelease(cfData);
    } else if (status != errSecItemNotFound) {
        // 未找到项目不是错误，只是返回空字符串
        LOG_F(ERROR, "Failed to retrieve data from macOS Keychain: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    return result;
}

bool PasswordManager::deleteFromMacKeychain(std::string_view service,
                                            std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain deletion");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    OSStatus status = SecItemDelete(query);

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to delete item from macOS Keychain: {}", 
              GetMacOSStatusString(status));
        return false;
    }

    // 返回true如果删除成功或未找到（幂等）
    LOG_F(INFO, "Successfully deleted or confirmed absence of keychain item (service:{}/account:{})",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::vector<std::string> PasswordManager::getAllMacKeychainItems(
    std::string_view service) const {
    // 无需锁定

    std::vector<std::string> results;
    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    
    if (!cfService) {
        LOG_F(ERROR, "Failed to create CF string for keychain enumeration");
        return results;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    
    CFArrayRef cfResults = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfResults);

    if (status == errSecSuccess && cfResults) {
        // 处理匹配项列表
        CFIndex count = CFArrayGetCount(cfResults);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(cfResults, i);
            CFStringRef account = (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            
            if (account) {
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(account), 
                                                                  kCFStringEncodingUTF8) + 1;
                char* buffer = (char*)malloc(maxSize);
                
                if (buffer && CFStringGetCString(account, buffer, maxSize, kCFStringEncodingUTF8)) {
                    results.push_back(std::string(buffer));
                }
                
                if (buffer) {
                    free(buffer);
                }
            }
        }
        CFRelease(cfResults);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to enumerate macOS Keychain items: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);

    return results;
}
#endif // defined(__APPLE__)

#if defined(__linux__) && defined(USE_LIBSECRET)
// 实现Linux平台下的libsecret存储方法

bool PasswordManager::storeToLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name,
    std::string_view encryptedData) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 使用标签和属性存储密码
    gboolean success = secret_password_store_sync(
        &schema,
        SECRET_COLLECTION_DEFAULT,
        attribute_name.data(),
        encryptedData.data(),
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success) {
        if (error) {
            LOG_F(ERROR, "Failed to store data to Linux keyring: %s", error->message);
            g_error_free(error);
        } else {
            LOG_F(ERROR, "Failed to store data to Linux keyring: Unknown error");
        }
        return false;
    }
    
    LOG_F(INFO, "Data stored successfully in Linux keyring (Schema: {}, Key: {})",
          std::string(schema_name).c_str(), std::string(attribute_name).c_str());
    return true;
}

std::string PasswordManager::retrieveFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 根据属性查找密码
    gchar* secret = secret_password_lookup_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    std::string result = "";
    if (secret) {
        result = std::string(secret);
        secret_password_free(secret);
    } else if (error) {
        LOG_F(ERROR, "Failed to retrieve data from Linux keyring: %s", error->message);
        g_error_free(error);
    } else {
        LOG_F(INFO, "No data found in Linux keyring for key: {}", 
              std::string(attribute_name).c_str());
    }
    
    // 如果未找到或出错则返回空字符串
    return result;
}

bool PasswordManager::deleteFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    gboolean success = secret_password_clear_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success && error) {
        LOG_F(ERROR, "Failed to delete data from Linux keyring: %s", error->message);
        g_error_free(error);
        return false;
    }
    
    LOG_F(INFO, "Successfully deleted data from Linux keyring for key: {}", 
          std::string(attribute_name).c_str());
    // 如果删除成功或未找到（幂等操作）则返回true
    return success || !error;
}

std::vector<std::string> PasswordManager::getAllLinuxKeyringItems(
    std::string_view schema_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    std::vector<std::string> results;
    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    // libsecret不提供直接枚举所有项目的方法。
    // 使用已知的索引键来存储键列表。
    std::string indexData = retrieveFromLinuxKeyring(schema_name, "ATOM_PM_INDEX");
    if (!indexData.empty()) {
        try {
            nlohmann::json indexJson = nlohmann::json::parse(indexData);
            if (indexJson.is_array()) {
                for (const auto& key : indexJson) {
                    if (key.is_string()) {
                        results.push_back(key.get<std::string>());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse Linux keyring index: {}", e.what());
        }
    }
    
    return results;
}
#endif // defined(__linux__) && defined(USE_LIBSECRET)

// 文件后备存储实现
#if !defined(_WIN32) && !defined(__APPLE__) && (!defined(__linux__) || !defined(USE_LIBSECRET))
// 如果没有特定平台的实现，使用文件后备

bool PasswordManager::storeToEncryptedFile(
    std::string_view identifier, std::string_view encryptedData) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 确保目录存在
        std::error_code ec;
        if (!std::filesystem::exists(storageDir, ec)) {
            if (!std::filesystem::create_directories(storageDir, ec) && ec) {
                LOG_F(ERROR, "Failed to create storage directory: {}", ec.message());
                return false;
            }
            
            // 在Unix-like系统上设置权限为仅当前用户可访问
            #if !defined(_WIN32)
            chmod(storageDir.c_str(), 0700);
            #endif
        }
        
        // 写入文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filePath.string());
            return false;
        }
        
        outFile.write(encryptedData.data(), encryptedData.size());
        outFile.close();
        
        // 更新索引文件
        updateEncryptedFileIndex(identifier, true);
        
        LOG_F(INFO, "Data stored successfully to file: {}", filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to store data to file: {}", e.what());
        return false;
    }
}

std::string PasswordManager::retrieveFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return "";
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            LOG_F(INFO, "File not found: {}", filePath.string());
            return "";
        }
        
        // 读取文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open file for reading: {}", filePath.string());
            return "";
        }
        
        // 使用迭代器读取整个文件内容
        std::string fileContents(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>());
        inFile.close();
        
        LOG_F(INFO, "Data retrieved successfully from file: {}", filePath.string());
        return fileContents;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to retrieve data from file: {}", e.what());
        return "";
    }
}

bool PasswordManager::deleteFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 删除文件
        std::error_code ec;
        bool removed = std::filesystem::remove(filePath, ec);
        
        if (ec) {
            LOG_F(ERROR, "Failed to delete file: {}", ec.message());
            return false;
        }
        
        // 更新索引文件
        if (removed) {
            updateEncryptedFileIndex(identifier, false);
            LOG_F(INFO, "File deleted successfully: {}", filePath.string());
        } else {
            LOG_F(INFO, "File not found for deletion: {}", filePath.string());
        }
        
        // 文件存在并被删除或文件不存在都视为成功（幂等操作）
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to delete file: {}", e.what());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllEncryptedFileItems() const {
    // 无需互斥锁，这是一个const方法

    std::vector<std::string> keys;
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return keys;
    }

    std::filesystem::path indexPath = storageDir / "index.json";

    try {
        // 如果索引文件存在，从中读取键列表
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        } else {
            // 索引文件不存在，扫描目录
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(storageDir, ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error scanning directory: {}", ec.message());
                    break;
                }
                
                if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                    std::string filename = entry.path().stem().string();
                    // 此处可以实现反向sanitize逻辑（如果需要）
                    // 简单情况下，直接使用文件名（不带扩展名）
                    keys.push_back(filename);
                }
            }
            
            // 创建索引文件
            try {
                nlohmann::json indexJson = keys;
                std::ofstream outFile(indexPath);
                outFile << indexJson.dump(2);
                outFile.close();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to create index file: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error accessing encrypted files: {}", e.what());
    }

    return keys;
}

// 辅助方法：更新加密文件索引
void PasswordManager::updateEncryptedFileIndex(std::string_view identifier, bool add) const {
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        return;
    }
    
    std::filesystem::path indexPath = storageDir / "index.json";
    std::vector<std::string> keys;
    
    // 读取现有索引（如果存在）
    try {
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        }
        
        // 添加或删除标识符
        std::string idStr(identifier);
        if (add) {
            // 如果不存在，则添加
            if (std::find(keys.begin(), keys.end(), idStr) == keys.end()) {
                keys.push_back(idStr);
            }
        } else {
            // 删除（如果存在）
            keys.erase(std::remove(keys.begin(), keys.end(), idStr), keys.end());
        }
        
        // 写回索引文件
        nlohmann::json indexJson = keys;
        std::ofstream outFile(indexPath);
        outFile << indexJson.dump(2);
        outFile.close();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to update index file: {}", e.what());
    }
}
#endif // 文件后备存储实现

// 实现安全擦除方法的通用模板
template <typename T>
void PasswordManager::secureWipe(T& data) noexcept {
    // 对不同类型数据的安全擦除实现
    if constexpr (std::is_same_v<T, std::vector<unsigned char>> ||
                 std::is_same_v<T, std::vector<char>>) {
        // 标准向量类型（字节数组）
// filepath: d:\msys64\home\qwdma\Atom\atom\secret\password_manager.cpp
#include "password_manager.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <regex>
#include <stdexcept>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include "atom/algorithm/base.hpp"  // 用于base64编码/解码
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// 常量定义
namespace {
constexpr std::string_view PM_VERSION = "1.0.0";
constexpr std::string_view PM_SERVICE_NAME = "AtomPasswordManager";
constexpr size_t PM_SALT_SIZE = 16;
constexpr size_t PM_IV_SIZE = 12;   // AES-GCM标准IV大小
constexpr size_t PM_TAG_SIZE = 16;  // AES-GCM标准标签大小
constexpr int DEFAULT_PBKDF2_ITERATIONS = 100000;
constexpr std::string_view VERIFICATION_PREFIX = "ATOM_PM_VERIFICATION_";
}  // namespace

namespace atom::secret {

// 补充searchPasswords中的循环实现
std::vector<std::string> PasswordManager::searchPasswords(std::string_view query) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot search passwords: PasswordManager is locked");
        return {};
    }
    
    if (query.empty()) {
        LOG_F(WARNING, "Empty search query, returning all keys.");
        std::vector<std::string> allKeys;
        allKeys.reserve(cachedPasswords.size());
        for (const auto& pair : cachedPasswords) {
            allKeys.push_back(pair.first);
        }
        return allKeys;
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for search");
            return {};
        }
        
        std::vector<std::string> results;
        std::string lowerQuery(query);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        for (const auto& [key, entry] : cachedPasswords) {
            // 转换为小写进行不区分大小写的搜索
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            // 搜索标题、用户名、网址和标签
            std::string lowerUsername = entry.username;
            std::string lowerUrl = entry.url;
            
            std::transform(lowerUsername.begin(), lowerUsername.end(), lowerUsername.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            if (lowerKey.find(lowerQuery) != std::string::npos ||
                lowerUsername.find(lowerQuery) != std::string::npos ||
                lowerUrl.find(lowerQuery) != std::string::npos) {
                results.push_back(key);
                continue;
            }
        }

        LOG_F(INFO, "Search for '{}' returned {} results", query.data(), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Search passwords error: {}", e.what());
        return {};
    }
}

// 补充filterByCategory中的循环实现
std::vector<std::string> PasswordManager::filterByCategory(PasswordCategory category) {
    std::unique_lock lock(mutex);

    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot filter passwords: PasswordManager is locked");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for category filtering");
            return {};
        }

        std::vector<std::string> results;
        for (const auto& [key, entry] : cachedPasswords) {
            if (entry.category == category) {
                results.push_back(key);
            }
        }

        LOG_F(INFO, "Filter by category {} returned {} results",
              static_cast<int>(category), results.size());
        return results;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Filter by category error: {}", e.what());
        return {};
    }
}

// 实现生成密码的方法
std::string PasswordManager::generatePassword(int length, bool includeSpecial,
                                             bool includeNumbers,
                                             bool includeMixedCase) {
    // 无需锁定用于生成，但updateActivity需要锁定
    // 先调用updateActivity
    {
        std::unique_lock lock(mutex);
        if (!isUnlocked.load(std::memory_order_relaxed)) {
            LOG_F(ERROR, "Cannot generate password: PasswordManager is locked");
            return "";
        }
        updateActivity();
    }

    if (length < settings.minPasswordLength) {
        LOG_F(WARNING, "Requested password length {} is less than minimum {}, using minimum",
              length, settings.minPasswordLength);
        length = settings.minPasswordLength;
    }
    if (length <= 0) {
        LOG_F(ERROR, "Invalid password length: {}", length);
        return "";
    }

    // 字符集
    const std::string lower = "abcdefghijklmnopqrstuvwxyz";
    const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string digits = "0123456789";
    const std::string special = "!@#$%^&*()-_=+[]{}\\|;:'\",.<>/?`~";
    
    std::string charPool;
    std::vector<char> requiredChars;

    charPool += lower;
    requiredChars.push_back(lower[0]);  // 临时占位，稍后会替换为随机字符
    
    if (includeMixedCase || settings.requireMixedCase) {
        charPool += upper;
        requiredChars.push_back(upper[0]);  // 临时占位
    }
    
    if (includeNumbers || settings.requireNumbers) {
        charPool += digits;
        requiredChars.push_back(digits[0]);  // 临时占位
    }
    
    if (includeSpecial || settings.requireSpecialChars) {
        charPool += special;
        requiredChars.push_back(special[0]);  // 临时占位
    }

    if (charPool.empty()) {
        LOG_F(ERROR, "No character set selected for password generation");
        return "";
    }

    // 使用C++随机引擎来获得更好的可移植性和控制
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> pool_dist(0, charPool.length() - 1);
    std::uniform_int_distribution<size_t> lower_dist(0, lower.length() - 1);
    std::uniform_int_distribution<size_t> upper_dist(0, upper.length() - 1);
    std::uniform_int_distribution<size_t> digit_dist(0, digits.length() - 1);
    std::uniform_int_distribution<size_t> special_dist(0, special.length() - 1);

    std::string password(length, ' ');
    size_t requiredCount = requiredChars.size();

    // 首先填充必需的字符
    requiredChars[0] = lower[lower_dist(generator)];
    size_t reqIdx = 1;
    
    if (includeMixedCase || settings.requireMixedCase) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = upper[upper_dist(generator)];
        }
    }

    if (includeNumbers || settings.requireNumbers) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = digits[digit_dist(generator)];
        }
    }

    if (includeSpecial || settings.requireSpecialChars) {
        if (reqIdx < requiredChars.size()) {
            requiredChars[reqIdx++] = special[special_dist(generator)];
        }
    }

    // 填充密码的剩余长度
    for (int i = 0; i < length; ++i) {
        password[i] = charPool[pool_dist(generator)];
    }

    // 将必需的字符随机放入密码中
    std::vector<size_t> positions(length);
    std::iota(positions.begin(), positions.end(), 0);
    std::shuffle(positions.begin(), positions.end(), generator);

    for (size_t i = 0; i < requiredCount && i < static_cast<size_t>(length); ++i) {
        password[positions[i]] = requiredChars[i];
    }

    // 最后再次打乱整个密码
    std::shuffle(password.begin(), password.end(), generator);

    LOG_F(INFO, "Generated password of length {}", length);
    return password;
}

// 实现密码强度评估
PasswordStrength PasswordManager::evaluatePasswordStrength(std::string_view password) const {
    // 无需锁定进行评估，这是一个const方法
    // updateActivity(); // 读取强度可能不算作活动

    const size_t len = password.length();
    if (len == 0) {
        return PasswordStrength::VeryWeak;
    }

    int score = 0;
    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // 熵近似评分（非常粗略）
    if (len >= 8) {
        score += 1;
    }

    if (len >= 12) {
        score += 1;
    }

    if (len >= 16) {
        score += 1;
    }

    // 检查字符类型
    for (char c : password) {
        if (!hasLower && std::islower(static_cast<unsigned char>(c))) {
            hasLower = true;
        } else if (!hasUpper && std::isupper(static_cast<unsigned char>(c))) {
            hasUpper = true;
        } else if (!hasDigit && std::isdigit(static_cast<unsigned char>(c))) {
            hasDigit = true;
        } else if (!hasSpecial && !std::isalnum(static_cast<unsigned char>(c))) {
            hasSpecial = true;
        }
        
        // 如果已找到所有类型，可以提前结束循环
        if (hasLower && hasUpper && hasDigit && hasSpecial) {
            break;
        }
    }

    int charTypes = 0;
    if (hasLower) {
        charTypes++;
    }

    if (hasUpper) {
        charTypes++;
    }

    if (hasDigit) {
        charTypes++;
    }

    if (hasSpecial) {
        charTypes++;
    }

    // 根据字符类型加分
    if (charTypes >= 2) {
        score += 1;
    }

    if (charTypes >= 3) {
        score += 1;
    }

    if (charTypes >= 4) {
        score += 1;
    }

    // 对常见模式的惩罚（简单检查）
    try {
        // 检查是否全是数字
        if (std::regex_match(std::string(password), std::regex("^\\d+$"))) {
            score -= 1;
        }
        
        // 检查是否全是字母
        if (std::regex_match(std::string(password), std::regex("^[a-zA-Z]+$"))) {
            score -= 1;
        }
        
        // 检查重复字符（如果超过25%的字符是相同的）
        std::map<char, int> charCount;
        for (char c : password) {
            charCount[c]++;
        }
        
        for (const auto& [_, count] : charCount) {
            if (static_cast<double>(count) / len > 0.25) {
                score -= 1;
                break;
            }
        }
        
        // 检查键盘顺序（简单版本）
        const std::string qwertyRows[] = {
            "qwertyuiop", "asdfghjkl", "zxcvbnm"
        };
        
        std::string lowerPass = std::string(password);
        std::transform(lowerPass.begin(), lowerPass.end(), lowerPass.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        for (const auto& row : qwertyRows) {
            for (size_t i = 0; i <= row.length() - 3; ++i) {
                std::string pattern = row.substr(i, 3);
                if (lowerPass.find(pattern) != std::string::npos) {
                    score -= 1;
                    break;
                }
            }
        }
    } catch (const std::regex_error& e) {
        LOG_F(ERROR, "Regex error in password strength evaluation: {}", e.what());
        // 不要因为正则表达式错误而使整个评估失败
    }

    // 将分数映射到强度等级
    if (score <= 1) {
        return PasswordStrength::VeryWeak;
    }

    if (score == 2) {
        return PasswordStrength::Weak;
    }

    if (score == 3) {
        return PasswordStrength::Medium;
    }

    if (score == 4) {
        return PasswordStrength::Strong;
    }

    // score >= 5
    return PasswordStrength::VeryStrong;
}

// 实现密码导出功能
bool PasswordManager::exportPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot export passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Export password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 确保缓存已完全加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for export");
            return false;
        }
        
        // 准备导出数据
        nlohmann::json exportData;
        exportData["version"] = PM_VERSION;
        exportData["entries"] = nlohmann::json::array();
        
        // 添加所有密码条目
        for (const auto& [key, entry] : cachedPasswords) {
            nlohmann::json entryJson;
            entryJson["platform_key"] = key;
            entryJson["username"] = entry.username;
            entryJson["password"] = entry.password; // 未加密状态下的密码
            entryJson["url"] = entry.url;
            entryJson["notes"] = entry.notes;
            entryJson["category"] = static_cast<int>(entry.category);
            
            // 添加到导出数据中
            exportData["entries"].push_back(entryJson);
        }
        
        // 序列化导出数据
        std::string serializedData = exportData.dump(2); // 使用2空格缩进

        // 生成盐和IV
        std::vector<unsigned char> salt(PM_SALT_SIZE);
        std::vector<unsigned char> iv(PM_IV_SIZE);
        
        if (RAND_bytes(salt.data(), salt.size()) != 1 ||
            RAND_bytes(iv.data(), iv.size()) != 1) {
            LOG_F(ERROR, "Failed to generate random data for export encryption");
            return false;
        }
        
        // 从导出密码派生密钥
        std::vector<unsigned char> exportKey = deriveKey(password, salt, DEFAULT_PBKDF2_ITERATIONS);
        
        // 使用AES-GCM加密序列化数据
        std::vector<unsigned char> encryptedData;
        std::vector<unsigned char> tag(PM_TAG_SIZE);
        
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              exportKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        encryptedData.resize(serializedData.size() + EVP_MAX_BLOCK_LENGTH);
        int outLen = 0;
        
        if (EVP_EncryptUpdate(ctx.get(), encryptedData.data(), &outLen,
                             reinterpret_cast<const unsigned char*>(serializedData.data()),
                             serializedData.size()) != 1) {
            LOG_F(ERROR, "Failed to encrypt data for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to finalize encryption for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        outLen += finalLen;
        encryptedData.resize(outLen);
        
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to get authentication tag for export: OpenSSL error");
            secureWipe(exportKey);
            return false;
        }
        
        // 构建最终的导出文件结构
        nlohmann::json exportFile;
        exportFile["format"] = "ATOM_PASSWORD_EXPORT";
        exportFile["version"] = PM_VERSION;
        
        // 修复base64编码部分，将vector<unsigned char>显式转换为std::string
        std::string saltBase64 = algorithm::base64Encode(salt);
        std::string ivBase64 = algorithm::base64Encode(iv);
        std::string tagBase64 = algorithm::base64Encode(tag);
        std::string dataBase64 = algorithm::base64Encode(encryptedData);
        
        exportFile["salt"] = saltBase64;
        exportFile["iv"] = ivBase64;
        exportFile["tag"] = tagBase64;
        exportFile["iterations"] = DEFAULT_PBKDF2_ITERATIONS;
        exportFile["data"] = dataBase64;
        
        // 写入导出文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open export file for writing: {}", filePath.string());
            secureWipe(exportKey);
            return false;
        }
        
        outFile << exportFile.dump(2);
        outFile.close();
        
        // 安全擦除导出密钥
        secureWipe(exportKey);
        
        LOG_F(INFO, "Successfully exported {} password entries to {}", 
              cachedPasswords.size(), filePath.string());
        
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Export passwords error: {}", e.what());
        return false;
    }
}

// 实现密码导入功能
bool PasswordManager::importPasswords(const std::filesystem::path& filePath,
                                     std::string_view password) {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot import passwords: PasswordManager is locked");
        return false;
    }
    if (password.empty()) {
        LOG_F(ERROR, "Import password cannot be empty");
        return false;
    }

    updateActivity();

    try {
        // 读取导入文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open import file for reading: {}", filePath.string());
            return false;
        }
        
        std::string fileContents((std::istreambuf_iterator<char>(inFile)), 
                                std::istreambuf_iterator<char>());
        inFile.close();
        
        if (fileContents.empty()) {
            LOG_F(ERROR, "Import file is empty: {}", filePath.string());
            return false;
        }
        
        // 解析导入文件JSON
        nlohmann::json importFile = nlohmann::json::parse(fileContents);
        
        // 验证文件格式
        if (!importFile.contains("format") || 
            importFile["format"] != "ATOM_PASSWORD_EXPORT") {
            LOG_F(ERROR, "Invalid import file format");
            return false;
        }
        
        // 提取加密参数
        std::vector<unsigned char> salt = 
            algorithm::base64Decode(importFile["salt"].get<std::string>());
        std::vector<unsigned char> iv = 
            algorithm::base64Decode(importFile["iv"].get<std::string>());
        std::vector<unsigned char> tag = 
            algorithm::base64Decode(importFile["tag"].get<std::string>());
        std::vector<unsigned char> encryptedData = 
            algorithm::base64Decode(importFile["data"].get<std::string>());
        int iterations = importFile["iterations"].get<int>();
        
        // 从导入密码派生密钥
        std::vector<unsigned char> importKey = deriveKey(password, salt, iterations);
        
        // 使用AES-GCM解密数据
        std::vector<unsigned char> decryptedData(encryptedData.size());
        int outLen = 0;
        
        SslCipherContext ctx;
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              importKey.data(), iv.data()) != 1) {
            LOG_F(ERROR, "Failed to initialize decryption for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        // 设置预期的标签
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            LOG_F(ERROR, "Failed to set authentication tag for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        if (EVP_DecryptUpdate(ctx.get(), decryptedData.data(), &outLen,
                             encryptedData.data(), encryptedData.size()) != 1) {
            LOG_F(ERROR, "Failed to decrypt data for import: OpenSSL error");
            secureWipe(importKey);
            return false;
        }
        
        int finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx.get(), decryptedData.data() + outLen, &finalLen) != 1) {
            LOG_F(ERROR, "Failed to verify imported data: Incorrect password or corrupted data");
            secureWipe(importKey);
            return false;
        }
        
        outLen += finalLen;
        decryptedData.resize(outLen);
        
        // 安全擦除导入密钥
        secureWipe(importKey);
        
        // 解析解密后的JSON数据
        std::string decryptedJson(decryptedData.begin(), decryptedData.end());
        nlohmann::json importData = nlohmann::json::parse(decryptedJson);
        
        if (!importData.contains("entries") || !importData["entries"].is_array()) {
            LOG_F(ERROR, "Import file has invalid structure: missing entries array");
            return false;
        }
        
        // 导入每个密码条目
        int importedCount = 0;
        int skippedCount = 0;
        
        for (const auto& entryJson : importData["entries"]) {
            // 创建密码条目
            PasswordEntry entry;
            std::string platformKey = entryJson["platform_key"].get<std::string>();
            
            // 填充条目数据
            entry.username = entryJson["username"].get<std::string>();
            entry.password = entryJson["password"].get<std::string>();
            entry.url = entryJson["url"].get<std::string>();
            
            if (entryJson.contains("notes")) {
                entry.notes = entryJson["notes"].get<std::string>();
            }
            
            if (entryJson.contains("category")) {
                entry.category = static_cast<PasswordCategory>(entryJson["category"].get<int>());
            }
            
            // 转换时间戳
            entry.created = std::chrono::system_clock::now();
            entry.modified = std::chrono::system_clock::now();
            
            // 导入策略：跳过或覆盖已存在的条目
            bool skipExisting = false; // 可以通过参数控制是否跳过已存在的条目
            
            if (skipExisting && cachedPasswords.find(platformKey) != cachedPasswords.end()) {
                skippedCount++;
                continue;
            }
            
            // 存储密码条目
            if (storePassword(platformKey, entry)) {
                importedCount++;
            } else {
                LOG_F(WARNING, "Failed to import password entry: {}", platformKey);
                skippedCount++;
            }
        }
        
        LOG_F(INFO, "Import complete: {} entries imported, {} entries skipped",
              importedCount, skippedCount);
        
        return importedCount > 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Import passwords error: {}", e.what());
        return false;
    }
}

// 实现检查过期密码的方法
std::vector<std::string> PasswordManager::checkExpiredPasswords() {
    std::unique_lock lock(mutex);
    if (!isUnlocked.load(std::memory_order_relaxed)) {
        LOG_F(ERROR, "Cannot check expired passwords: PasswordManager is locked");
        return {};
    }

    if (!settings.notifyOnPasswordExpiry || settings.passwordExpiryDays <= 0) {
        LOG_F(INFO, "Password expiry checking is disabled");
        return {};
    }

    updateActivity();

    try {
        // 确保缓存已加载
        bool loadResult = loadAllPasswords(); // 处理返回值
        if (!loadResult) {
            LOG_F(ERROR, "Failed to load passwords for expiry check");
            return {};
        }
        
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        
        // 计算警告阈值（即将过期的天数）
        auto warningThreshold = std::chrono::hours(settings.passwordExpiryDays * 24);
        
        // 存储即将过期的密码键
        std::vector<std::string> expiredKeys;
        
        for (const auto& [key, entry] : cachedPasswords) {
            // 根据最后修改时间和过期策略检查
            auto lastModified = entry.modified;
            if (lastModified == std::chrono::system_clock::time_point{}) {
                // 如果没有修改时间，使用创建时间
                lastModified = entry.created;
            }
            
            // 如果密码年龄超过了阈值
            if (now - lastModified >= warningThreshold) {
                expiredKeys.push_back(key);
            }
        }
        
        LOG_F(INFO, "Found {} expired or soon-to-expire passwords", expiredKeys.size());
        return expiredKeys;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Check expired passwords error: {}", e.what());
        return {};
    }
}

// 实现Windows平台特定的存储方法
#if defined(_WIN32)
bool PasswordManager::storeToWindowsCredentialManager(
    std::string_view target, std::string_view encryptedData) const {
    // 无需锁定，这是一个const方法访问外部系统

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    cred.TargetName = const_cast<LPWSTR>(wideTarget.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(encryptedData.length());
    // CredentialBlob需要非const指针
    cred.CredentialBlob =
        reinterpret_cast<LPBYTE>(const_cast<char*>(encryptedData.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    // 使用固定的、非敏感的用户名或派生一个？为简单起见使用固定值。
    static const std::wstring pmUser = L"AtomPasswordManagerUser";
    cred.UserName = const_cast<LPWSTR>(pmUser.c_str());

    if (CredWriteW(&cred, 0)) {
        LOG_F(INFO, "Successfully stored data to Windows Credential Manager for target: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        LOG_F(ERROR, "Failed to store data to Windows Credential Manager: Error code {}", lastError);
        return false;
    }
}

std::string PasswordManager::retrieveFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    PCREDENTIALW pCred = nullptr;
    std::string result = "";

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for retrieval");
        return result;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredReadW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        if (pCred) {
            if (pCred->CredentialBlobSize > 0 && pCred->CredentialBlob) {
                // 将凭据数据转换为std::string
                result = std::string(
                    reinterpret_cast<const char*>(pCred->CredentialBlob),
                    pCred->CredentialBlobSize);
            }
            CredFree(pCred);
        }
        return result;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            LOG_F(INFO, "No credential found in Windows Credential Manager for target: {}",
                  std::string(target).c_str());
        } else {
            LOG_F(ERROR, "Failed to retrieve data from Windows Credential Manager: Error code {}",
                  lastError);
        }
        return result;
    }
}

bool PasswordManager::deleteFromWindowsCredentialManager(
    std::string_view target) const {
    // 无需锁定，这是一个const方法访问外部系统

    // 将target转换为宽字符串
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, target.data(),
                                      static_cast<int>(target.length()), nullptr, 0);
    if (wideLen <= 0) {
        LOG_F(ERROR, "Failed to convert target to wide string for deletion");
        return false;
    }
    std::wstring wideTarget(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, target.data(), static_cast<int>(target.length()),
                       &wideTarget[0], wideLen);

    if (CredDeleteW(wideTarget.c_str(), CRED_TYPE_GENERIC, 0)) {
        LOG_F(INFO, "Successfully deleted credential from Windows Credential Manager: {}",
              std::string(target).c_str());
        return true;
    } else {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_FOUND) {
            // 如果凭据不存在，也视为删除成功（幂等操作）
            LOG_F(INFO, "No credential found to delete in Windows Credential Manager: {}",
                  std::string(target).c_str());
            return true;
        } else {
            LOG_F(ERROR, "Failed to delete credential from Windows Credential Manager: Error code {}",
                  lastError);
            return false;
        }
    }
}

std::vector<std::string> PasswordManager::getAllWindowsCredentials() const {
    // 无需锁定，这是一个const方法访问外部系统

    std::vector<std::string> results;
    DWORD count = 0;
    PCREDENTIALW* pCredentials = nullptr;

    // 枚举匹配模式的凭据（例如，"AtomPasswordManager*"）
    // 使用通配符可能需要特定的权限或配置。
    // 更安全的方法可能是存储一个索引凭据。
    // 为简单起见，假设枚举有效或使用与文件后备相似的索引方法。
    // 使用固定前缀进行枚举：
    std::wstring filter =
        std::wstring(PM_SERVICE_NAME.begin(), PM_SERVICE_NAME.end()) +
        L"*";

    if (CredEnumerateW(filter.c_str(), 0, &count, &pCredentials)) {
        // 处理枚举结果
        for (DWORD i = 0; i < count; i++) {
            if (pCredentials[i]) {
                // 将宽字符目标名称转换为UTF-8字符串
                int targetLen = WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                                   nullptr, 0, nullptr, nullptr);
                if (targetLen > 0) {
                    std::string targetName(targetLen - 1, 0); // 减1是为了不包括结尾的null
                    WideCharToMultiByte(CP_UTF8, 0, pCredentials[i]->TargetName, -1,
                                       &targetName[0], targetLen, nullptr, nullptr);
                    
                    // 根据需要从目标名称中提取实际键
                    // 例如，如果目标名称格式为"AtomPasswordManager_key"，则去除前缀
                    std::string prefix = std::string(PM_SERVICE_NAME) + "_";
                    if (targetName.find(prefix) == 0) {
                        results.push_back(targetName.substr(prefix.length()));
                    } else {
                        // 没有预期的前缀，使用完整目标名称
                        results.push_back(targetName);
                    }
                }
            }
        }
        CredFree(pCredentials);
    } else {
        DWORD lastError = GetLastError();
        if (lastError != ERROR_NOT_FOUND) { // 没找到不是错误
            LOG_F(ERROR, "Failed to enumerate Windows credentials: Error code {}", lastError);
        }
    }
    return results;
}
#endif // defined(_WIN32)

#if defined(__APPLE__)
// 实现macOS平台特定的存储方法

// 辅助函数用于macOS状态码
std::string GetMacOSStatusString(OSStatus status) {
    // 考虑使用SecCopyErrorMessageString来获得更具描述性的错误（如果可用）
    return "macOS Error: " + std::to_string(status);
}

bool PasswordManager::storeToMacKeychain(std::string_view service,
                                         std::string_view account,
                                         std::string_view encryptedData) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);
    CFDataRef cfData =
        CFDataCreate(kCFAllocatorDefault,
                     reinterpret_cast<const UInt8*>(encryptedData.data()),
                     encryptedData.length());

    if (!cfService || !cfAccount || !cfData) {
        LOG_F(ERROR, "Failed to create CF objects for keychain storage");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        if (cfData) CFRelease(cfData);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    // 先检查项目是否已存在
    OSStatus status = SecItemCopyMatching(query, nullptr);
    if (status == errSecSuccess) {
        // 项目已存在，更新它
        CFMutableDictionaryRef updateDict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(updateDict, kSecValueData, cfData);
        status = SecItemUpdate(query, updateDict);
        CFRelease(updateDict);
    } else if (status == errSecItemNotFound) {
        // 项目不存在，添加它
        CFDictionarySetValue(query, kSecValueData, cfData);
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);
    CFRelease(cfData);

    if (status != errSecSuccess) {
        LOG_F(ERROR, "Failed to store data to macOS Keychain: {}", GetMacOSStatusString(status));
        return false;
    }

    LOG_F(INFO, "Successfully stored data to macOS Keychain for service:{}/account:{}",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::string PasswordManager::retrieveFromMacKeychain(
    std::string_view service, std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain retrieval");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return "";
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    
    CFDataRef cfData = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfData);

    std::string result = "";
    if (status == errSecSuccess && cfData) {
        // 从CFData转换为std::string
        CFIndex length = CFDataGetLength(cfData);
        if (length > 0) {
            const UInt8* bytes = CFDataGetBytePtr(cfData);
            result = std::string(reinterpret_cast<const char*>(bytes), length);
        }
        CFRelease(cfData);
    } else if (status != errSecItemNotFound) {
        // 未找到项目不是错误，只是返回空字符串
        LOG_F(ERROR, "Failed to retrieve data from macOS Keychain: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    return result;
}

bool PasswordManager::deleteFromMacKeychain(std::string_view service,
                                            std::string_view account) const {
    // 无需锁定

    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    CFStringRef cfAccount = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(account.data()),
        account.length(), kCFStringEncodingUTF8, false);

    if (!cfService || !cfAccount) {
        LOG_F(ERROR, "Failed to create CF objects for keychain deletion");
        if (cfService) CFRelease(cfService);
        if (cfAccount) CFRelease(cfAccount);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecAttrAccount, cfAccount);

    OSStatus status = SecItemDelete(query);

    CFRelease(query);
    CFRelease(cfService);
    CFRelease(cfAccount);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to delete item from macOS Keychain: {}", 
              GetMacOSStatusString(status));
        return false;
    }

    // 返回true如果删除成功或未找到（幂等）
    LOG_F(INFO, "Successfully deleted or confirmed absence of keychain item (service:{}/account:{})",
          std::string(service).c_str(), std::string(account).c_str());
    return true;
}

std::vector<std::string> PasswordManager::getAllMacKeychainItems(
    std::string_view service) const {
    // 无需锁定

    std::vector<std::string> results;
    CFStringRef cfService = CFStringCreateWithBytes(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(service.data()),
        service.length(), kCFStringEncodingUTF8, false);
    
    if (!cfService) {
        LOG_F(ERROR, "Failed to create CF string for keychain enumeration");
        return results;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, cfService);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    
    CFArrayRef cfResults = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&cfResults);

    if (status == errSecSuccess && cfResults) {
        // 处理匹配项列表
        CFIndex count = CFArrayGetCount(cfResults);
        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(cfResults, i);
            CFStringRef account = (CFStringRef)CFDictionaryGetValue(item, kSecAttrAccount);
            
            if (account) {
                CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(account), 
                                                                  kCFStringEncodingUTF8) + 1;
                char* buffer = (char*)malloc(maxSize);
                
                if (buffer && CFStringGetCString(account, buffer, maxSize, kCFStringEncodingUTF8)) {
                    results.push_back(std::string(buffer));
                }
                
                if (buffer) {
                    free(buffer);
                }
            }
        }
        CFRelease(cfResults);
    } else if (status != errSecItemNotFound) {
        LOG_F(ERROR, "Failed to enumerate macOS Keychain items: {}", 
              GetMacOSStatusString(status));
    }

    CFRelease(query);
    CFRelease(cfService);

    return results;
}
#endif // defined(__APPLE__)

#if defined(__linux__) && defined(USE_LIBSECRET)
// 实现Linux平台下的libsecret存储方法

bool PasswordManager::storeToLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name,
    std::string_view encryptedData) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 使用标签和属性存储密码
    gboolean success = secret_password_store_sync(
        &schema,
        SECRET_COLLECTION_DEFAULT,
        attribute_name.data(),
        encryptedData.data(),
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success) {
        if (error) {
            LOG_F(ERROR, "Failed to store data to Linux keyring: %s", error->message);
            g_error_free(error);
        } else {
            LOG_F(ERROR, "Failed to store data to Linux keyring: Unknown error");
        }
        return false;
    }
    
    LOG_F(INFO, "Data stored successfully in Linux keyring (Schema: {}, Key: {})",
          std::string(schema_name).c_str(), std::string(attribute_name).c_str());
    return true;
}

std::string PasswordManager::retrieveFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    // 根据属性查找密码
    gchar* secret = secret_password_lookup_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    std::string result = "";
    if (secret) {
        result = std::string(secret);
        secret_password_free(secret);
    } else if (error) {
        LOG_F(ERROR, "Failed to retrieve data from Linux keyring: %s", error->message);
        g_error_free(error);
    } else {
        LOG_F(INFO, "No data found in Linux keyring for key: {}", 
              std::string(attribute_name).c_str());
    }
    
    // 如果未找到或出错则返回空字符串
    return result;
}

bool PasswordManager::deleteFromLinuxKeyring(
    std::string_view schema_name, std::string_view attribute_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    GError* error = nullptr;
    gboolean success = secret_password_clear_sync(
        &schema,
        nullptr,
        &error,
        "atom_pm_key", attribute_name.data(),
        nullptr);
    
    if (!success && error) {
        LOG_F(ERROR, "Failed to delete data from Linux keyring: %s", error->message);
        g_error_free(error);
        return false;
    }
    
    LOG_F(INFO, "Successfully deleted data from Linux keyring for key: {}", 
          std::string(attribute_name).c_str());
    // 如果删除成功或未找到（幂等操作）则返回true
    return success || !error;
}

std::vector<std::string> PasswordManager::getAllLinuxKeyringItems(
    std::string_view schema_name) const {
    // 无需互斥锁，这是一个访问外部系统的const方法

    std::vector<std::string> results;
    const SecretSchema schema = {
        schema_name.data(),
        SECRET_SCHEMA_NONE,
        {
            {"atom_pm_key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)}
        }
    };

    // libsecret不提供直接枚举所有项目的方法。
    // 使用已知的索引键来存储键列表。
    std::string indexData = retrieveFromLinuxKeyring(schema_name, "ATOM_PM_INDEX");
    if (!indexData.empty()) {
        try {
            nlohmann::json indexJson = nlohmann::json::parse(indexData);
            if (indexJson.is_array()) {
                for (const auto& key : indexJson) {
                    if (key.is_string()) {
                        results.push_back(key.get<std::string>());
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to parse Linux keyring index: {}", e.what());
        }
    }
    
    return results;
}
#endif // defined(__linux__) && defined(USE_LIBSECRET)

// 文件后备存储实现
#if !defined(_WIN32) && !defined(__APPLE__) && (!defined(__linux__) || !defined(USE_LIBSECRET))
// 如果没有特定平台的实现，使用文件后备

bool PasswordManager::storeToEncryptedFile(
    std::string_view identifier, std::string_view encryptedData) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 确保目录存在
        std::error_code ec;
        if (!std::filesystem::exists(storageDir, ec)) {
            if (!std::filesystem::create_directories(storageDir, ec) && ec) {
                LOG_F(ERROR, "Failed to create storage directory: {}", ec.message());
                return false;
            }
            
            // 在Unix-like系统上设置权限为仅当前用户可访问
            #if !defined(_WIN32)
            chmod(storageDir.c_str(), 0700);
            #endif
        }
        
        // 写入文件
        std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
        if (!outFile) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filePath.string());
            return false;
        }
        
        outFile.write(encryptedData.data(), encryptedData.size());
        outFile.close();
        
        // 更新索引文件
        updateEncryptedFileIndex(identifier, true);
        
        LOG_F(INFO, "Data stored successfully to file: {}", filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to store data to file: {}", e.what());
        return false;
    }
}

std::string PasswordManager::retrieveFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return "";
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 检查文件是否存在
        if (!std::filesystem::exists(filePath)) {
            LOG_F(INFO, "File not found: {}", filePath.string());
            return "";
        }
        
        // 读取文件
        std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
        if (!inFile) {
            LOG_F(ERROR, "Failed to open file for reading: {}", filePath.string());
            return "";
        }
        
        // 使用迭代器读取整个文件内容
        std::string fileContents(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>());
        inFile.close();
        
        LOG_F(INFO, "Data retrieved successfully from file: {}", filePath.string());
        return fileContents;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to retrieve data from file: {}", e.what());
        return "";
    }
}

bool PasswordManager::deleteFromEncryptedFile(
    std::string_view identifier) const {
    // 无需互斥锁，这是一个const方法

    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return false;
    }

    std::string sanitizedIdentifier = sanitizeIdentifier(identifier);
    std::filesystem::path filePath = storageDir / (sanitizedIdentifier + ".dat");

    try {
        // 删除文件
        std::error_code ec;
        bool removed = std::filesystem::remove(filePath, ec);
        
        if (ec) {
            LOG_F(ERROR, "Failed to delete file: {}", ec.message());
            return false;
        }
        
        // 更新索引文件
        if (removed) {
            updateEncryptedFileIndex(identifier, false);
            LOG_F(INFO, "File deleted successfully: {}", filePath.string());
        } else {
            LOG_F(INFO, "File not found for deletion: {}", filePath.string());
        }
        
        // 文件存在并被删除或文件不存在都视为成功（幂等操作）
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to delete file: {}", e.what());
        return false;
    }
}

std::vector<std::string> PasswordManager::getAllEncryptedFileItems() const {
    // 无需互斥锁，这是一个const方法

    std::vector<std::string> keys;
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        LOG_F(ERROR, "Failed to get secure storage directory");
        return keys;
    }

    std::filesystem::path indexPath = storageDir / "index.json";

    try {
        // 如果索引文件存在，从中读取键列表
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        } else {
            // 索引文件不存在，扫描目录
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(storageDir, ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error scanning directory: {}", ec.message());
                    break;
                }
                
                if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                    std::string filename = entry.path().stem().string();
                    // 此处可以实现反向sanitize逻辑（如果需要）
                    // 简单情况下，直接使用文件名（不带扩展名）
                    keys.push_back(filename);
                }
            }
            
            // 创建索引文件
            try {
                nlohmann::json indexJson = keys;
                std::ofstream outFile(indexPath);
                outFile << indexJson.dump(2);
                outFile.close();
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Failed to create index file: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error accessing encrypted files: {}", e.what());
    }

    return keys;
}

// 辅助方法：更新加密文件索引
void PasswordManager::updateEncryptedFileIndex(std::string_view identifier, bool add) const {
    std::filesystem::path storageDir = getSecureStorageDirectory();
    if (storageDir.empty()) {
        return;
    }
    
    std::filesystem::path indexPath = storageDir / "index.json";
    std::vector<std::string> keys;
    
    // 读取现有索引（如果存在）
    try {
        if (std::filesystem::exists(indexPath)) {
            std::ifstream inFile(indexPath);
            if (inFile) {
                try {
                    nlohmann::json indexJson;
                    inFile >> indexJson;
                    inFile.close();
                    
                    if (indexJson.is_array()) {
                        for (const auto& key : indexJson) {
                            if (key.is_string()) {
                                keys.push_back(key.get<std::string>());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Failed to parse index file: {}", e.what());
                }
            }
        }
        
        // 添加或删除标识符
        std::string idStr(identifier);
        if (add) {
            // 如果不存在，则添加
            if (std::find(keys.begin(), keys.end(), idStr) == keys.end()) {
                keys.push_back(idStr);
            }
        } else {
            // 删除（如果存在）
            keys.erase(std::remove(keys.begin(), keys.end(), idStr), keys.end());
        }
        
        // 写回索引文件
        nlohmann::json indexJson = keys;
        std::ofstream outFile(indexPath);
        outFile << indexJson.dump(2);
        outFile.close();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to update index file: {}", e.what());
    }
}
#endif // 文件后备存储实现

// 实现安全擦除方法的通用模板
template <typename T>
void PasswordManager::secureWipe(T& data) noexcept {
    // 对不同类型数据的安全擦除实现
    if constexpr (std::is_same_v<T, std::vector<unsigned char>> ||
                 std::is_same_v<T, std::vector<char>>) {
        // 标准向量类型（字节数组）
        if (!data.empty()) {
            // 使用随机数据覆盖
            std::memset(data.data(), 0, data.size());
            
            // 可选：多次覆盖使用不同的模式
            #if defined(_WIN32)
            // Windows可能使用SecureZeroMemory，但标准memset应该足够，编译器通常不会优化掉
            #elif defined(__linux__) || defined(__APPLE__)
            // 对于Linux和macOS，volatile指针确保不会被优化掉
            volatile unsigned char* vptr = data.data();
            std::size_t size = data.size();
            for (std::size_t i = 0; i < size; ++i) {
                vptr[i] = 0;
            }
            #endif
            
            // 清空容器
            data.clear();
        }
    } else if constexpr (std::is_pointer_v<T>) {
        // 原始指针处理（需要额外的长度参数，不在此实现）
    } else {
        // 其他类型 - 默认实现
        static_assert(std::is_trivially_destructible_v<T>, 
                     "Only trivially destructible types can be securely wiped with the default implementation");
        std::memset(&data, 0, sizeof(T));
    }
}

// 字符串的特殊处理
template <>
void PasswordManager::secureWipe<std::string>(std::string& data) noexcept {
    if (!data.empty()) {
        // 用零覆盖
        std::memset(&data[0], 0, data.size());
        
        #if defined(_WIN32)
        // Windows使用标准memset
        #elif defined(__linux__) || defined(__APPLE__)
        // 对于Linux和macOS，volatile指针确保不会被优化掉
        volatile char* vptr = &data[0];
        std::size_t size = data.size();
        for (std::size_t i = 0; i < size; ++i) {
            vptr[i] = 0;
        }
        #endif
        
        // 清空字符串
        data.clear();
    }
}

// 实现派生密钥的方法
std::vector<unsigned char> PasswordManager::deriveKey(
    std::string_view masterPassword, std::span<const unsigned char> salt,
    int iterations) const {
    // 无需锁定，这是一个基于输入的const计算

    if (iterations <= 0) {
        LOG_F(WARNING, "Invalid iteration count {}, using default {}", 
              iterations, DEFAULT_PBKDF2_ITERATIONS);
        iterations = DEFAULT_PBKDF2_ITERATIONS;
    }

    std::vector<unsigned char> derivedKey(32); // AES-256需要32字节密钥
    // 使用EVP_KDF函数进行PBKDF2（比PKCS5_PBKDF2_HMAC更推荐）
    std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)> kdf(
        EVP_KDF_fetch(nullptr, "PBKDF2", nullptr), EVP_KDF_free);
    if (!kdf) {
        throw std::runtime_error("Failed to fetch PBKDF2 KDF implementation from OpenSSL");
    }

    std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)> kctx(
        EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free);
    if (!kctx) {
        throw std::runtime_error("Failed to create KDF context");
    }

    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD,
        const_cast<char*>(masterPassword.data()), 
        masterPassword.length());
    params[1] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SALT,
        const_cast<unsigned char*>(salt.data()), 
        salt.size());
    params[2] = OSSL_PARAM_construct_int(OSSL_KDF_PARAM_ITER, &iterations);
    params[3] = OSSL_PARAM_construct_utf8_string(
        OSSL_KDF_PARAM_DIGEST, const_cast<char*>(SN_sha256), 0);
    params[4] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(kctx.get(), derivedKey.data(), derivedKey.size(), params) != 1) {
        throw std::runtime_error("Failed to derive key: OpenSSL error");
    }

    return derivedKey;
}

// 实现加密条目的方法
std::string PasswordManager::encryptEntry(
    const PasswordEntry& entry, std::span<const unsigned char> key) const {
    // 无需锁定，这是一个使用提供的密钥的const方法

    // 序列化条目为JSON
    nlohmann::json entryJson;
    try {
        entryJson["title"] = entry.title;
        entryJson["username"] = entry.username;
        entryJson["password"] = entry.password;
        entryJson["url"] = entry.url;
        entryJson["notes"] = entry.notes;
        entryJson["category"] = static_cast<int>(entry.category);
        entryJson["tags"] = entry.tags;
        
        // 将时间点转换为Unix时间戳（秒）
        entryJson["created"] = std::chrono::system_clock::to_time_t(entry.created);
        entryJson["modified"] = std::chrono::system_clock::to_time_t(entry.modified);
        entryJson["expires"] = std::chrono::system_clock::to_time_t(entry.expires);
        
        // 序列化历史密码
        nlohmann::json previousArray = nlohmann::json::array();
        for (const auto& prevPwd : entry.previousPasswords) {
            nlohmann::json prevJson;
            prevJson["password"] = prevPwd.password;
            prevJson["changed"] = std::chrono::system_clock::to_time_t(prevPwd.changed);
            previousArray.push_back(prevJson);
        }
        entryJson["previous_passwords"] = previousArray;
        
        // 序列化自定义字段
        nlohmann::json customFieldsArray = nlohmann::json::array();
        for (const auto& field : entry.customFields) {
            nlohmann::json fieldJson;
            fieldJson["name"] = field.name;
            fieldJson["value"] = field.value;
            fieldJson["is_protected"] = field.isProtected;
            customFieldsArray.push_back(fieldJson);
        }
        entryJson["custom_fields"] = customFieldsArray;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to serialize password entry: {}", e.what());
        throw std::runtime_error("Serialization error during encryption");
    }

    std::string serializedEntry = entryJson.dump();

    // 生成随机IV
    std::vector<unsigned char> iv(PM_IV_SIZE);
    if (RAND_bytes(iv.data(), iv.size()) != 1) {
        LOG_F(ERROR, "Failed to generate random IV for encryption");
        throw std::runtime_error("Random number generation error");
    }

    // 使用AES-GCM加密
    std::vector<unsigned char> encryptedData;
    std::vector<unsigned char> tag(PM_TAG_SIZE);
    try {
        // 初始化加密上下文
        SslCipherContext ctx;
        if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              key.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to initialize encryption: OpenSSL error");
        }
        
        // 加密数据
        encryptedData.resize(serializedEntry.size() + EVP_MAX_BLOCK_LENGTH);
        int outLen = 0;
        
        if (EVP_EncryptUpdate(ctx.get(), encryptedData.data(), &outLen,
                             reinterpret_cast<const unsigned char*>(serializedEntry.data()),
                             serializedEntry.size()) != 1) {
            throw std::runtime_error("Failed to encrypt data: OpenSSL error");
        }
        
        // 完成加密
        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx.get(), encryptedData.data() + outLen, &finalLen) != 1) {
            throw std::runtime_error("Failed to finalize encryption: OpenSSL error");
        }
        
        outLen += finalLen;
        encryptedData.resize(outLen);
        
        // 获取认证标签
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
            throw std::runtime_error("Failed to get authentication tag: OpenSSL error");
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Encryption error: {}", e.what());
        throw;
    }

    // 构建加密包JSON
    nlohmann::json encJson;
    auto ivBase64 = algorithm::base64Encode(iv);
    auto tagBase64 = algorithm::base64Encode(tag);
    auto dataBase64 = algorithm::base64Encode(encryptedData);

    if (!ivBase64 || !tagBase64 || !dataBase64) {
        throw std::runtime_error("Failed to base64 encode encryption components");
    }

    encJson["iv"] = ivBase64->to_string();
    encJson["tag"] = tagBase64->to_string();
    encJson["data"] = dataBase64->to_string();
    // 如果支持多种加密方法，可以选择添加加密方法标识符
    // encJson["method"] = static_cast<int>(settings.encryptionOptions.encryptionMethod);

    return encJson.dump();
}

// 实现解密条目的方法
PasswordEntry PasswordManager::decryptEntry(
    std::string_view encryptedData, std::span<const unsigned char> key) const {
    // 无需锁定，这是一个使用提供的密钥的const方法

    // 解析加密包JSON
    nlohmann::json encJson;
    std::vector<unsigned char> iv, tag, dataBytes;
    try {
        encJson = nlohmann::json::parse(encryptedData);
        
        auto ivResult = algorithm::base64Decode(encJson["iv"].get<std::string>());
        auto tagResult = algorithm::base64Decode(encJson["tag"].get<std::string>());
        auto dataResult = algorithm::base64Decode(encJson["data"].get<std::string>());
        
        if (!ivResult || !tagResult || !dataResult) {
            throw std::runtime_error("Failed to decode base64 encryption components");
        }
        
        iv = std::move(*ivResult);
        tag = std::move(*tagResult);
        dataBytes = std::move(*dataResult);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to parse encrypted data: {}", e.what());
        throw std::runtime_error("Parse error during decryption");
    }

    // 使用AES-GCM解密
    std::vector<unsigned char> decryptedDataBytes;
    bool decryptionSuccess = false;
    try {
        // 初始化解密上下文
        SslCipherContext ctx;
        if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, 
                              key.data(), iv.data()) != 1) {
            throw std::runtime_error("Failed to initialize decryption: OpenSSL error");
        }
        
        // 分配足够的空间
        decryptedDataBytes.resize(dataBytes.size());
        int outLen = 0;
        
        // 设置预期的认证标签
        if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
            throw std::runtime_error("Failed to set authentication tag: OpenSSL error");
        }
        
        // 解密数据
        if (EVP_DecryptUpdate(ctx.get(), decryptedDataBytes.data(), &outLen,
                             dataBytes.data(), dataBytes.size()) != 1) {
            throw std::runtime_error("Failed to decrypt data: OpenSSL error");
        }
        
        // 完成解密并验证
        int finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx.get(), decryptedDataBytes.data() + outLen, &finalLen) <= 0) {
            throw std::runtime_error("Failed to verify decrypted data: Authentication failed");
        }
        
        outLen += finalLen;
        decryptedDataBytes.resize(outLen);
        decryptionSuccess = true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Decryption error: {}", e.what());
        throw;
    }

    if (!decryptionSuccess) {
        throw std::runtime_error("Decryption failed without specific error");
    }

    // 解析解密后的JSON
    PasswordEntry entry;
    try {
        std::string decryptedJson(decryptedDataBytes.begin(), decryptedDataBytes.end());
        nlohmann::json entryJson = nlohmann::json::parse(decryptedJson);
        
        // 填充基本字段
        entry.title = entryJson["title"].get<std::string>();
        entry.username = entryJson["username"].get<std::string>();
        entry.password = entryJson["password"].get<std::string>();
        
        // 可选字段使用条件获取
        if (entryJson.contains("url")) {
            entry.url = entryJson["url"].get<std::string>();
        }
        
        if (entryJson.contains("notes")) {
            entry.notes = entryJson["notes"].get<std::string>();
        }
        
        if (entryJson.contains("category")) {
            entry.category = static_cast<PasswordCategory>(entryJson["category"].get<int>());
        }
        
        if (entryJson.contains("tags") && entryJson["tags"].is_array()) {
            entry.tags = entryJson["tags"].get<std::vector<std::string>>();
        }
        
        // 时间字段
        if (entryJson.contains("created")) {
            std::time_t created = entryJson["created"].get<std::time_t>();
            entry.created = std::chrono::system_clock::from_time_t(created);
        } else {
            entry.created = std::chrono::system_clock::now();
        }
        
        if (entryJson.contains("modified")) {
            std::time_t modified = entryJson["modified"].get<std::time_t>();
            entry.modified = std::chrono::system_clock::from_time_t(modified);
        } else {
            entry.modified = entry.created;
        }
        
        if (entryJson.contains("expires")) {
            std::time_t expires = entryJson["expires"].get<std::time_t>();
            entry.expires = std::chrono::system_clock::from_time_t(expires);
        }
        
        // 历史密码
        if (entryJson.contains("previous_passwords") && 
            entryJson["previous_passwords"].is_array()) {
            for (const auto& prevJson : entryJson["previous_passwords"]) {
                PreviousPassword prev;
                prev.password = prevJson["password"].get<std::string>();
                
                if (prevJson.contains("changed")) {
                    std::time_t changed = prevJson["changed"].get<std::time_t>();
                    prev.changed = std::chrono::system_clock::from_time_t(changed);
                } else {
                    prev.changed = std::chrono::system_clock::now();
                }
                
                entry.previousPasswords.push_back(prev);
            }
        }
        
        // 自定义字段
        if (entryJson.contains("custom_fields") && 
            entryJson["custom_fields"].is_array()) {
            for (const auto& fieldJson : entryJson["custom_fields"]) {
                CustomField field;
                field.name = fieldJson["name"].get<std::string>();
                field.value = fieldJson["value"].get<std::string>();
                
                if (fieldJson.contains("is_protected")) {
                    field.isProtected = fieldJson["is_protected"].get<bool>();
                }
                
                entry.customFields.push_back(field);
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to parse decrypted entry: {}", e.what());
        throw std::runtime_error("Error parsing decrypted data");
    }

    return entry;
}

// 实现自动锁定检查的方法
bool PasswordManager::isLocked() const noexcept {
    // 首先检查显式锁定状态
    if (!isUnlocked.load(std::memory_order_acquire)) {
        return true;
    }

    // 然后检查是否应该自动锁定
    if (settings.autoLockTimeout > 0) {
        std::shared_lock lock(mutex);
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity).count();
        
        // 如果已超过自动锁定超时时间
        if (elapsed >= settings.autoLockTimeout) {
            // 我们无法在const方法内修改isUnlocked，所以返回true表示应该锁定
            // 调用者应当检查此返回并调用lock()
            LOG_F(INFO, "Auto-lock timeout reached ({} seconds)", elapsed);
            return true;
        }
    }

    return false;
}

// 辅助方法：更新活动时间
void PasswordManager::updateActivity() {
    // 假设调用者持有一个unique_lock
    lastActivity = std::chrono::system_clock::now();

    // 如果设置了回调函数则触发
    if (activityCallback) {
        try {
            activityCallback();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Activity callback error: {}", e.what());
            // 不要因为回调错误而中断主流程
        } catch (...) {
            LOG_F(ERROR, "Unknown error in activity callback");
        }
    }

    // 自动锁定检查在isLocked()中惰性处理或由用户/计时器显式处理
    // 在每次活动时检查可能过于激进。如果自动锁定计时器需要主动检查，单独的线程可能更好。
}

} // namespace atom::secret