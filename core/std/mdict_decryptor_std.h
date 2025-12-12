#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace UnidictCoreStd {

/**
 * @brief MDict加密类型枚举
 */
enum class MdictEncryptionType {
    NONE,           // 无加密
    SIMPLE_XOR,     // Simple XOR加密
    DES_ECB,        // DES ECB模式
    DES_CBC,        // DES CBC模式
    BLOWFISH_ECB,  // Blowfish ECB模式
    BLOWFISH_CBC,  // Blowfish CBC模式
    AES_ECB,        // AES ECB模式
    AES_CBC,        // AES CBC模式
    CUSTOM          // 自定义加密
};

/**
 * @brief 解密结果结构
 */
struct DecryptResult {
    bool success = false;                    // 解密是否成功
    std::string data;                        // 解密后的数据
    std::string error;                        // 错误信息
    MdictEncryptionType detected_type = MdictEncryptionType::NONE; // 检测到的加密类型

    DecryptResult(bool s, std::string d = {}, MdictEncryptionType type = MdictEncryptionType::NONE)
        : success(s), data(std::move(d)), detected_type(type) {}

    DecryptResult(std::string err, MdictEncryptionType type = MdictEncryptionType::NONE)
        : success(false), error(std::move(err)), detected_type(type) {}
};

/**
 * @brief MDict解密器类
 *
 * 提供对MDict文件中常见加密格式的支持
 * 支持基于密码的解密和基本的XOR解密
 */
class MdictDecryptorStd {
public:
    /**
     * @brief 构造函数
     */
    MdictDecryptorStd();

    /**
     * @brief 析构函数 - 声明为虚函数以避免ABI问题
     */
    virtual ~MdictDecryptorStd();

    /**
     * @brief 安全清除密码
     */
    // 禁止拷贝
    MdictDecryptorStd(const MdictDecryptorStd&) = delete;
    MdictDecryptorStd& operator=(const MdictDecryptorStd&) = delete;

    /**
     * @brief 设置解密密码
     * @param password 密码字符串
     * @return 是否成功设置密码
     */
    bool set_password(const std::string& password);

    /**
     * @brief 清除密码（安全操作）
     */
    void clear_password();

    /**
     * @brief 检查是否设置了密码
     */
    bool has_password() const;

    /**
     * @brief 检测加密类型
     * @param encrypted_header 加密的头部数据
     * @return 检测到的加密类型和解密结果
     */
    DecryptResult detect_encryption_type(const std::vector<uint8_t>& encrypted_header) const;

    /**
     * @brief 解密数据
     * @param encrypted_data 加密的数据
     * @param type 加密类型（如果已知）
     * @return 解密结果
     */
    DecryptResult decrypt(const std::vector<uint8_t>& encrypted_data,
                         MdictEncryptionType type = MdictEncryptionType::NONE) const;

    /**
     * @brief 解密数据（字符串版本）
     * @param encrypted_data 加密的数据字符串
     * @param type 加密类型（如果已知）
     * @return 解密结果
     */
    DecryptResult decrypt(const std::string& encrypted_data,
                         MdictEncryptionType type = MdictEncryptionType::NONE) const;

    /**
     * @brief 尝试自动解密（无密码）
     * @param encrypted_data 加密的数据
     * @return 解密结果
     */
    DecryptResult try_auto_decrypt(const std::vector<uint8_t>& encrypted_data) const;

    /**
     * @brief 获取支持的加密类型列表
     * @return 加密类型描述列表
     */
    std::vector<std::string> get_supported_types() const;

    /**
     * @brief 验证解密结果的有效性
     * @param data 解密后的数据
     * @return 数据是否可能是有效的MDict内容
     */
    bool validate_decrypted_data(const std::string& data) const;

    /**
     * @brief 启用/禁用调试模式
     * @param enable 是否启用调试
     */
    void set_debug_mode(bool enable);

    /**
     * @brief 获取最后的错误信息
     * @return 错误信息
     */
    std::string get_last_error() const;

private:
    /**
     * @brief 密码存储结构
     */
    struct PasswordData {
        std::string password;
        bool is_set = false;

        void clear() {
            // 安全清除密码内存
            password.assign(password.size(), '\0');
            password.clear();
            is_set = false;
        }
    };

    /**
     * @brief XOR解密实现
     */
    DecryptResult decrypt_xor(const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& key) const;

    /**
     * @brief SimpleXOR解密（常见于简单加密的MDict）
     */
    DecryptResult decrypt_simple_xor(const std::vector<uint8_t>& data,
                                const std::string& password) const;

    /**
     * @brief DES解密（占位符实现）
     */
    DecryptResult decrypt_des(const std::vector<uint8_t>& data,
                         const std::vector<uint8_t>& key) const;

    /**
     * @brief Blowfish解密（占位符实现）
     */
    DecryptResult decrypt_blowfish(const std::vector<uint8_t>& data,
                              const std::vector<uint8_t>& key) const;

    /**
     * @brief AES解密（占位符实现）
     */
    DecryptResult decrypt_aes(const std::vector<uint8_t>& data,
                         const std::vector<uint8_t>& key) const;

    /**
     * @brief 生成简单密钥流
     */
    std::vector<uint8_t> generate_key_stream(const std::string& password, size_t length) const;

    /**
     * @brief 分析数据特征
     */
    struct DataFeatures {
        double entropy = 0.0;        // 熵值
        size_t repetition_score = 0;  // 重复分数
        bool has_header_structure = false;  // 是否有头部结构
        bool looks_like_text = false;     // 是否像文本
        std::vector<uint8_t> patterns; // 常见模式
    };

    DataFeatures analyze_data_features(const std::vector<uint8_t>& data) const;

    /**
     * @brief 计算数据熵
     */
    double calculate_entropy(const std::vector<uint8_t>& data) const;

    /**
     * @brief 检测常见模式
     */
    std::vector<uint8_t> detect_patterns(const std::vector<uint8_t>& data) const;

    /**
     * @brief 检测头部结构
     */
    bool detect_header_structure(const std::vector<uint8_t>& data) const;

    // 成员变量
    PasswordData password_data_;
    bool debug_mode_ = false;
    mutable std::string last_error_;

    // 常量定义
    static constexpr size_t MAX_PASSWORD_LENGTH = 1024;
    static constexpr size_t MIN_ANALYSIS_SIZE = 256;
    static constexpr double ENTROPY_THRESHOLD_ENCRYPTED = 7.0;
    static constexpr double ENTROPY_THRESHOLD_TEXT = 4.5;
};

} // namespace UnidictCoreStd
