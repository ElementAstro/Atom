#ifndef ATOM_SECRET_PASSWORD_HPP
#define ATOM_SECRET_PASSWORD_HPP

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace atom::secret {

/**
 * @brief 密码强度等级
 */
enum class PasswordStrength { VeryWeak, Weak, Medium, Strong, VeryStrong };

/**
 * @brief 密码类别
 */
enum class PasswordCategory {
    General,
    Finance,
    Work,
    Personal,
    Social,
    Entertainment,
    Other
};

/**
 * @brief 密码项结构
 */
struct PasswordEntry {
    std::string password;                            ///< 存储的密码
    std::string username;                            ///< 关联的用户名
    std::string url;                                 ///< 关联的URL
    std::string notes;                               ///< 附加说明
    PasswordCategory category;                       ///< 密码类别
    std::chrono::system_clock::time_point created;   ///< 创建时间
    std::chrono::system_clock::time_point modified;  ///< 最后修改时间
    std::vector<std::string> previousPasswords;      ///< 历史密码
};

/**
 * @brief 加密选项结构
 */
struct EncryptionOptions {
    bool useHardwareAcceleration = true;  ///< 是否使用硬件加速
    int keyIterations = 10000;            ///< PBKDF2迭代次数
    int encryptionMethod =
        0;  ///< 加密方法(0=AES-GCM, 1=AES-CBC, 2=ChaCha20-Poly1305)
};

/**
 * @brief 密码管理器设置
 */
struct PasswordManagerSettings {
    int autoLockTimeoutSeconds = 300;     ///< 自动锁定超时(秒)
    bool notifyOnPasswordExpiry = true;   ///< 密码过期提醒
    int passwordExpiryDays = 90;          ///< 密码有效期(天)
    int minPasswordLength = 12;           ///< 最小密码长度
    bool requireSpecialChars = true;      ///< 是否要求特殊字符
    bool requireNumbers = true;           ///< 是否要求数字
    bool requireMixedCase = true;         ///< 是否要求大小写混合
    EncryptionOptions encryptionOptions;  ///< 加密选项
};

/**
 * @brief 用于安全管理密码的类。
 *
 * PasswordManager类提供了使用平台特定的凭据存储机制
 * 安全地存储、检索和删除密码的方法。
 */
class PasswordManager {
private:
    std::vector<unsigned char> masterKey;  ///< 主密钥，用于派生AES加密密钥
    bool isInitialized = false;            ///< 是否已初始化
    bool isUnlocked = false;               ///< 是否已解锁
    std::chrono::system_clock::time_point lastActivity;  ///< 最后活动时间
    PasswordManagerSettings settings;                    ///< 管理器设置

    // 缓存的密码数据，解锁状态下可用
    std::map<std::string, PasswordEntry> cachedPasswords;

public:
    /**
     * @brief 构造一个PasswordManager对象。
     */
    PasswordManager();

    /**
     * @brief 析构函数，确保安全清理内存
     */
    ~PasswordManager();

    /**
     * @brief 使用主密码初始化密码管理器
     *
     * @param masterPassword 用于派生加密密钥的主密码
     * @param settings 可选的密码管理器设置
     * @return 初始化是否成功
     */
    bool initialize(
        const std::string& masterPassword,
        const PasswordManagerSettings& settings = PasswordManagerSettings());

    /**
     * @brief 使用主密码解锁密码管理器
     *
     * @param masterPassword 主密码
     * @return 解锁是否成功
     */
    bool unlock(const std::string& masterPassword);

    /**
     * @brief 锁定密码管理器，清除内存中的敏感数据
     */
    void lock();

    /**
     * @brief 更改主密码
     *
     * @param currentPassword 当前主密码
     * @param newPassword 新主密码
     * @return 更改是否成功
     */
    bool changeMasterPassword(const std::string& currentPassword,
                              const std::string& newPassword);

    void loadAllPasswords();

    /**
     * @brief 存储密码条目
     *
     * @param platformKey 与平台相关的键
     * @param entry 密码条目信息
     * @return 存储是否成功
     */
    bool storePassword(const std::string& platformKey,
                       const PasswordEntry& entry);

    /**
     * @brief 检索密码条目
     *
     * @param platformKey 与平台相关的键
     * @return 检索到的密码条目
     */
    PasswordEntry retrievePassword(const std::string& platformKey);

    /**
     * @brief 删除密码
     *
     * @param platformKey 与平台相关的键
     * @return 删除是否成功
     */
    bool deletePassword(const std::string& platformKey);

    /**
     * @brief 获取所有平台键的列表
     *
     * @return 所有平台键的列表
     */
    std::vector<std::string> getAllPlatformKeys();

    /**
     * @brief 搜索密码条目
     *
     * @param query 搜索关键字
     * @return 匹配的平台键列表
     */
    std::vector<std::string> searchPasswords(const std::string& query);

    /**
     * @brief 按类别过滤密码
     *
     * @param category 密码类别
     * @return 属于该类别的平台键列表
     */
    std::vector<std::string> filterByCategory(PasswordCategory category);

    /**
     * @brief 生成强密码
     *
     * @param length 密码长度
     * @param includeSpecial 是否包含特殊字符
     * @param includeNumbers 是否包含数字
     * @param includeMixedCase 是否包含大小写字母
     * @return 生成的密码
     */
    std::string generatePassword(int length = 16, bool includeSpecial = true,
                                 bool includeNumbers = true,
                                 bool includeMixedCase = true);

    /**
     * @brief 评估密码强度
     *
     * @param password 需评估的密码
     * @return 密码强度级别
     */
    PasswordStrength evaluatePasswordStrength(const std::string& password);

    /**
     * @brief 导出所有密码数据（加密状态）
     *
     * @param filePath 导出文件路径
     * @param password 额外的加密密码
     * @return 导出是否成功
     */
    bool exportPasswords(const std::string& filePath,
                         const std::string& password);

    /**
     * @brief 从备份文件导入密码数据
     *
     * @param filePath 备份文件路径
     * @param password 解密密码
     * @return 导入是否成功
     */
    bool importPasswords(const std::string& filePath,
                         const std::string& password);

    /**
     * @brief 更新密码管理器设置
     *
     * @param newSettings 新设置
     */
    void updateSettings(const PasswordManagerSettings& newSettings);

    /**
     * @brief 获取当前设置
     *
     * @return 当前设置
     */
    PasswordManagerSettings getSettings() const;

    /**
     * @brief 检查是否有过期密码
     *
     * @return 过期密码的平台键列表
     */
    std::vector<std::string> checkExpiredPasswords();

    /**
     * @brief 设置活动更新回调
     *
     * @param callback 当活动发生时调用的函数
     */
    void setActivityCallback(std::function<void()> callback);

private:
    std::function<void()> activityCallback;  ///< 活动回调函数

    /**
     * @brief 更新最后活动时间
     */
    void updateActivity();

    /**
     * @brief 从主密码派生加密密钥
     *
     * @param masterPassword 主密码
     * @param salt 盐值
     * @param iterations 迭代次数
     * @return 派生的密钥
     */
    std::vector<unsigned char> deriveKey(const std::string& masterPassword,
                                         const std::vector<unsigned char>& salt,
                                         int iterations = 10000);

    /**
     * @brief 安全清除内存中的敏感数据
     *
     * @param data 待清除的数据
     */
    template <typename T>
    void secureWipe(T& data);

    /**
     * @brief 加密密码条目
     *
     * @param entry 密码条目
     * @param key 加密密钥
     * @return 加密后的数据
     */
    std::string encryptEntry(const PasswordEntry& entry,
                             const std::vector<unsigned char>& key);

    /**
     * @brief 解密密码条目
     *
     * @param encryptedData 加密数据
     * @param key 解密密钥
     * @return 解密后的密码条目
     */
    PasswordEntry decryptEntry(const std::string& encryptedData,
                               const std::vector<unsigned char>& key);

#if defined(_WIN32)
    /**
     * @brief 将加密的密码存储到Windows凭据管理器中。
     *
     * @param target 凭据的目标名称
     * @param encryptedData 要存储的加密数据
     * @return 存储是否成功
     */
    bool storeToWindowsCredentialManager(const std::string& target,
                                         const std::string& encryptedData);

    /**
     * @brief 从Windows凭据管理器中检索加密的密码。
     *
     * @param target 凭据的目标名称
     * @return 检索到的加密密码
     */
    std::string retrieveFromWindowsCredentialManager(const std::string& target);

    /**
     * @brief 从Windows凭据管理器中删除密码。
     *
     * @param target 凭据的目标名称
     * @return 删除是否成功
     */
    bool deleteFromWindowsCredentialManager(const std::string& target);

    /**
     * @brief 获取Windows凭据管理器中所有条目
     *
     * @return 所有条目的目标名称列表
     */
    std::vector<std::string> getAllWindowsCredentials();

#elif defined(__APPLE__)
    /**
     * @brief 将加密的密码存储到macOS钥匙串中。
     *
     * @param service 钥匙串项的服务名称
     * @param account 钥匙串项的账户名称
     * @param encryptedData 要存储的加密数据
     * @return 存储是否成功
     */
    bool storeToMacKeychain(const std::string& service,
                            const std::string& account,
                            const std::string& encryptedData);

    /**
     * @brief 从macOS钥匙串中检索加密的密码。
     *
     * @param service 钥匙串项的服务名称
     * @param account 钥匙串项的账户名称
     * @return 检索到的加密密码
     */
    std::string retrieveFromMacKeychain(const std::string& service,
                                        const std::string& account);

    /**
     * @brief 从macOS钥匙串中删除密码。
     *
     * @param service 钥匙串项的服务名称
     * @param account 钥匙串项的账户名称
     * @return 删除是否成功
     */
    bool deleteFromMacKeychain(const std::string& service,
                               const std::string& account);

    /**
     * @brief 获取macOS钥匙串中所有条目
     *
     * @param service 服务名称
     * @return 所有条目的账户名称列表
     */
    std::vector<std::string> getAllMacKeychainItems(const std::string& service);

#elif defined(__linux__)
    /**
     * @brief 将加密的密码存储到Linux密钥环中。
     *
     * @param schema_name 密钥环项的模式名称
     * @param attribute_name 密钥环项的属性名称
     * @param encryptedData 要存储的加密数据
     * @return 存储是否成功
     */
    bool storeToLinuxKeyring(const std::string& schema_name,
                             const std::string& attribute_name,
                             const std::string& encryptedData);

    /**
     * @brief 从Linux密钥环中检索加密的密码。
     *
     * @param schema_name 密钥环项的模式名称
     * @param attribute_name 密钥环项的属性名称
     * @return 检索到的加密密码
     */
    std::string retrieveFromLinuxKeyring(const std::string& schema_name,
                                         const std::string& attribute_name);

    /**
     * @brief 从Linux密钥环中删除密码。
     *
     * @param schema_name 密钥环项的模式名称
     * @param attribute_name 密钥环项的属性名称
     * @return 删除是否成功
     */
    bool deleteFromLinuxKeyring(const std::string& schema_name,
                                const std::string& attribute_name);

    /**
     * @brief 获取Linux密钥环中所有条目
     *
     * @param schema_name 模式名称
     * @return 所有条目的属性名称列表
     */
    std::vector<std::string> getAllLinuxKeyringItems(
        const std::string& schema_name);
    /**
     * @brief 文件存储后备方案的实现
     */
    bool storeToEncryptedFile(const std::string& identifier,
                              const std::string& encryptedData);

    std::string retrieveFromEncryptedFile(const std::string& identifier);

    bool deleteFromEncryptedFile(const std::string& identifier);

    std::vector<std::string> getAllEncryptedFileItems();
#endif
};
}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_HPP