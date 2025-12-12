#include "mdict_decryptor_std.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <random>

namespace UnidictCoreStd {

MdictDecryptorStd::MdictDecryptorStd() = default;

MdictDecryptorStd::~MdictDecryptorStd() {
    clear_password();
}

bool MdictDecryptorStd::set_password(const std::string& password) {
    if (password.length() > MAX_PASSWORD_LENGTH) {
        last_error_ = "密码长度超过限制 (" +
                      std::to_string(MAX_PASSWORD_LENGTH) + " 字符)";
        return false;
    }

    // 安全清除旧密码
    password_data_.clear();

    // 存储新密码
    password_data_.password = password;
    password_data_.is_set = true;

    last_error_.clear();
    return true;
}

void MdictDecryptorStd::clear_password() {
    password_data_.clear();
    last_error_.clear();
}

bool MdictDecryptorStd::has_password() const {
    return password_data_.is_set;
}

DecryptResult MdictDecryptorStd::detect_encryption_type(const std::vector<uint8_t>& header) const {
    if (debug_mode_) {
        std::cout << "开始分析加密头部..." << std::endl;

        // 显示头部前32字节
        std::cout << "头部内容 (前32字节): ";
        for (size_t i = 0; i < std::min<size_t>(32, header.size()); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(header[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // 特征分析
    auto features = analyze_data_features(header);

    if (debug_mode_) {
        std::cout << "数据特征分析:" << std::endl;
        std::cout << "  熵值: " << features.entropy << std::endl;
        std::cout << "  重复分数: " << features.repetition_score << std::endl;
        std::cout << "  有头部结构: " << (features.has_header_structure ? "是" : "否") << std::endl;
        std::cout << "  像文本: " << (features.looks_like_text ? "是" : "否") << std::endl;
        std::cout << "  模式数量: " << features.patterns.size() << std::endl;
    }

    // 判断加密类型
    if (features.entropy < ENTROPY_THRESHOLD_TEXT) {
        // 熵值低，可能未加密或简单加密
        if (features.has_header_structure) {
            if (debug_mode_) {
                std::cout << "检测到结构化头部，可能已格式化" << std::endl;
            }
            return DecryptResult(true, std::string(header.begin(), header.end()), MdictEncryptionType::NONE);
        } else if (features.entropy < ENTROPY_THRESHOLD_ENCRYPTED) {
            // 低熵但有结构，可能是SimpleXOR
            if (debug_mode_) {
                std::cout << "检测到可能的SimpleXOR加密" << std::endl;
            }
            return DecryptResult("需要SimpleXOR解密", MdictEncryptionType::SIMPLE_XOR);
        }
    }

    // 检测模式特征
    if (!features.patterns.empty()) {
        if (debug_mode_) {
            std::cout << "检测到重复模式，可能是XOR加密" << std::endl;
        }
        return DecryptResult("检测到XOR模式，需要解密", MdictEncryptionType::SIMPLE_XOR);
    }

    // 高熵值，可能是强加密
    if (features.entropy > 6.0) {
        if (debug_mode_) {
            std::cout << "检测到高熵值，可能需要强加密解密" << std::endl;
        }
        return DecryptResult("检测到强加密，不支持", MdictEncryptionType::CUSTOM);
    }

    if (debug_mode_) {
        std::cout << "无法确定加密类型，假设为SimpleXOR" << std::endl;
    }
    return DecryptResult("检测到可能的加密，尝试SimpleXOR解密", MdictEncryptionType::SIMPLE_XOR);
}

DecryptResult MdictDecryptorStd::decrypt_xor(const std::vector<uint8_t>& data,
                                            const std::vector<uint8_t>& key) const {
    if (key.empty()) {
        return DecryptResult("XOR解密需要密钥", MdictEncryptionType::SIMPLE_XOR);
    }

    std::vector<uint8_t> decrypted;
    decrypted.reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i) {
        decrypted.push_back(data[i] ^ key[i % key.size()]);
    }

    if (debug_mode_) {
        std::cout << "XOR解密完成，数据大小: " << decrypted.size() << std::endl;
    }

    return DecryptResult(true, std::string(decrypted.begin(), decrypted.end()), MdictEncryptionType::SIMPLE_XOR);
}

DecryptResult MdictDecryptorStd::decrypt_simple_xor(const std::vector<uint8_t>& data,
                                                  const std::string& password) const {
    if (password.empty()) {
        return DecryptResult("SimpleXOR解密需要密码", MdictEncryptionType::SIMPLE_XOR);
    }

    // 生成密钥流
    auto key = generate_key_stream(password, std::min<size_t>(256, data.size()));

    if (debug_mode_) {
        std::cout << "SimpleXOR密钥生成完成，密钥长度: " << key.size() << std::endl;
    }

    return decrypt_xor(data, key);
}

DecryptResult MdictDecryptorStd::decrypt(const std::vector<uint8_t>& encrypted_data,
                                       MdictEncryptionType type) const {
    switch (type) {
        case MdictEncryptionType::NONE:
            return DecryptResult(true, std::string(encrypted_data.begin(), encrypted_data.end()), MdictEncryptionType::NONE);

        case MdictEncryptionType::SIMPLE_XOR:
            if (!has_password()) {
                return DecryptResult("SimpleXOR解密需要密码", MdictEncryptionType::SIMPLE_XOR);
            }
            return decrypt_simple_xor(encrypted_data, password_data_.password);

        case MdictEncryptionType::DES_ECB:
        case MdictEncryptionType::DES_CBC:
        case MdictEncryptionType::BLOWFISH_ECB:
        case MdictEncryptionType::BLOWFISH_CBC:
        case MdictEncryptionType::AES_ECB:
        case MdictEncryptionType::AES_CBC:
        {
            std::string error = "不支持的加密类型: ";
            switch (type) {
                case MdictEncryptionType::DES_ECB: error += "DES_ECB"; break;
                case MdictEncryptionType::DES_CBC: error += "DES_CBC"; break;
                case MdictEncryptionType::BLOWFISH_ECB: error += "BLOWFISH_ECB"; break;
                case MdictEncryptionType::BLOWFISH_CBC: error += "BLOWFISH_CBC"; break;
                case MdictEncryptionType::AES_ECB: error += "AES_ECB"; break;
                case MdictEncryptionType::AES_CBC: error += "AES_CBC"; break;
                default: error += "UNKNOWN"; break;
            }
            return DecryptResult(error, type);
        }

        case MdictEncryptionType::CUSTOM:
        default:
            return DecryptResult("自定义加密类型不支持", MdictEncryptionType::CUSTOM);
    }
}

DecryptResult MdictDecryptorStd::decrypt(const std::string& encrypted_data,
                                       MdictEncryptionType type) const {
    std::vector<uint8_t> data(encrypted_data.begin(), encrypted_data.end());
    return decrypt(data, type);
}

DecryptResult MdictDecryptorStd::try_auto_decrypt(const std::vector<uint8_t>& encrypted_data) const {
    if (debug_mode_) {
        std::cout << "尝试自动解密..." << std::endl;
    }

    // 首先尝试无加密
    auto result = decrypt(encrypted_data, MdictEncryptionType::NONE);
    if (result.success && validate_decrypted_data(result.data)) {
        if (debug_mode_) {
            std::cout << "自动检测：数据未加密" << std::endl;
        }
        return result;
    }

    // 如果有密码，尝试SimpleXOR
    if (has_password()) {
        result = decrypt_simple_xor(encrypted_data, password_data_.password);
        if (result.success && validate_decrypted_data(result.data)) {
            if (debug_mode_) {
                std::cout << "自动解密：SimpleXOR解密成功" << std::endl;
            }
            return result;
        }
    }

    // 尝试其他常见的XOR密钥（单字节）
    for (int key = 1; key <= 255; ++key) {
        std::vector<uint8_t> single_key = {static_cast<uint8_t>(key)};
        result = decrypt_xor(encrypted_data, single_key);
        if (result.success && validate_decrypted_data(result.data)) {
            if (debug_mode_) {
                std::cout << "自动解密：找到单字节XOR密钥: 0x"
                          << std::hex << key << std::endl;
            }
            return DecryptResult(true, result.data, MdictEncryptionType::SIMPLE_XOR);
        }
    }

    return DecryptResult("自动解密失败", MdictEncryptionType::SIMPLE_XOR);
}

std::vector<std::string> MdictDecryptorStd::get_supported_types() const {
    return {
        "NONE - 无加密",
        "SIMPLE_XOR - Simple XOR加密",
        "DES_ECB - DES ECB模式",
        "DES_CBC - DES CBC模式",
        "BLOWFISH_ECB - Blowfish ECB模式",
        "BLOWFISH_CBC - Blowfish CBC模式",
        "AES_ECB - AES ECB模式",
        "AES_CBC - AES CBC模式",
        "CUSTOM - 自定义加密"
    };
}

bool MdictDecryptorStd::validate_decrypted_data(const std::string& data) const {
    if (data.empty()) {
        return false;
    }

    // 检查是否包含常见的MDict标记
    const std::vector<std::string> mdict_markers = {
        "MDX", "MDD", "BookName", "Description",
        "Title", "Author", "Version", "StyleSheet",
        "encoding", "Format", "KeyBlock", "RecordBlock"
    };

    // 在数据前512字节中搜索标记
    size_t search_size = std::min<size_t>(512, data.size());
    std::string search_data = data.substr(0, search_size);
    std::string lower_search_data = search_data;
    std::transform(lower_search_data.begin(), lower_search_data.end(),
                   lower_search_data.begin(), ::tolower);

    int marker_count = 0;
    for (const auto& marker : mdict_markers) {
        std::string lower_marker = marker;
        std::transform(lower_marker.begin(), lower_marker.end(),
                       lower_marker.begin(), ::tolower);

        if (lower_search_data.find(lower_marker) != std::string::npos) {
            marker_count++;
            if (debug_mode_) {
                std::cout << "找到MDict标记: " << marker << std::endl;
            }
        }
    }

    // 如果找到3个或更多标记，认为是有效的MDict数据
    bool is_valid = marker_count >= 3;

    if (debug_mode_) {
        std::cout << "MDict标记数量: " << marker_count
                  << ", 有效性: " << (is_valid ? "是" : "否") << std::endl;
    }

    return is_valid;
}

void MdictDecryptorStd::set_debug_mode(bool enable) {
    debug_mode_ = enable;
    if (debug_mode_) {
        std::cout << "MDict解密器调试模式已启用" << std::endl;
    }
}

std::string MdictDecryptorStd::get_last_error() const {
    return last_error_;
}

std::vector<uint8_t> MdictDecryptorStd::generate_key_stream(const std::string& password, size_t length) const {
    std::vector<uint8_t> key;
    key.reserve(length);

    // 简单的密钥流生成算法
    std::mt19937 rng(std::hash<std::string>{}(password));
    for (size_t i = 0; i < length; ++i) {
        key.push_back(static_cast<uint8_t>(rng() % 256));
    }

    return key;
}

MdictDecryptorStd::DataFeatures MdictDecryptorStd::analyze_data_features(const std::vector<uint8_t>& data) const {
    DataFeatures features{};

    // 计算熵
    features.entropy = calculate_entropy(data);

    // 分析重复模式
    std::array<int, 256> freq{};
    int max_freq = 0;
    int unique_chars = 0;

    for (uint8_t byte : data) {
        freq[byte]++;
        if (freq[byte] == 1) {
            unique_chars++;
        }
        max_freq = std::max(max_freq, freq[byte]);
    }

    // 重复分数（如果某些字符过于频繁，可能是XOR加密的文本）
    if (unique_chars > 0) {
        features.repetition_score = static_cast<size_t>(max_freq * data.size() / unique_chars);
    }

    // 检测文本特征
    features.looks_like_text = true;
    for (uint8_t byte : data) {
        // 检查是否为可打印ASCII字符
        if (byte < 32 || byte > 126) {
            // 允许常见的控制字符
            if (byte != '\t' && byte != '\n' && byte != '\r') {
                features.looks_like_text = false;
                break;
            }
        }
    }

    // 检测头部结构
    features.has_header_structure = detect_header_structure(data);

    // 检测常见模式
    features.patterns = detect_patterns(data);

    return features;
}

double MdictDecryptorStd::calculate_entropy(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return 0.0;
    }

    std::array<int, 256> freq{};
    for (uint8_t byte : data) {
        freq[byte]++;
    }

    double entropy = 0.0;
    for (int count : freq) {
        if (count > 0) {
            double probability = static_cast<double>(count) / data.size();
            entropy -= probability * std::log2(probability);
        }
    }

    return entropy;
}

std::vector<uint8_t> MdictDecryptorStd::detect_patterns(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> patterns;

    if (data.size() < 4) {
        return patterns;
    }

    // 检测重复的字节模式
    for (size_t i = 0; i < data.size() - 3; ++i) {
        uint8_t pattern[4] = {
            data[i], data[i+1], data[i+2], data[i+3]
        };

        // 检查这个模式是否重复
        bool is_repeating = false;
        for (size_t j = i + 4; j < data.size() - 3; ++j) {
            if (data[j] == pattern[0] &&
                data[j+1] == pattern[1] &&
                data[j+2] == pattern[2] &&
                data[j+3] == pattern[3]) {
                is_repeating = true;
                break;
            }
        }

        if (is_repeating) {
            for (int k = 0; k < 4; ++k) {
                patterns.push_back(pattern[k]);
            }
        }
    }

    return patterns;
}

bool MdictDecryptorStd::detect_header_structure(const std::vector<uint8_t>& data) const {
    if (data.size() < 16) {
        return false;
    }

    // 简单的头部结构检测
    // 查找可能的长度字段、标记等
    size_t header_fields = 0;

    // 检查前8字节是否可能是长度字段
    for (size_t i = 0; i < 8 && i + 4 <= data.size(); ++i) {
        uint32_t possible_length = (static_cast<uint32_t>(data[i]) << 24) |
                                  (static_cast<uint32_t>(data[i+1]) << 16) |
                                  (static_cast<uint32_t>(data[i+2]) << 8) |
                                  static_cast<uint32_t>(data[i+3]);

        // 合理的长度字段（<1MB）
        if (possible_length > 0 && possible_length < 1024 * 1024) {
            header_fields++;
        }
    }

    // 检查是否包含常见标记
    std::string data_str(data.begin(), data.begin() + std::min<size_t>(64, data.size()));
    std::vector<std::string> common_tags = {
        "MDX", "MDD", "BookName", "Title", "KeyBlock", "RecordBlock"
    };

    for (const auto& tag : common_tags) {
        if (data_str.find(tag) != std::string::npos) {
            header_fields++;
        }
    }

    return header_fields >= 2;
}

} // namespace UnidictCoreStd
