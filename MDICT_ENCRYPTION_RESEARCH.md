# MDict加密支持研究文档

## 🔐 当前状态

根据代码分析，Unidict目前对MDict加密的支持状况：

### 已实现部分
- **加密检测**: 能够识别加密的MDict文件（通过header中的`encrypted`属性）
- **基本信息读取**: 可以读取加密词典的基本信息（名称、描述等）
- **解密框架**: 已加入 `core/std/mdict_decryptor_std.*`，提供加密类型检测与解密入口
- **SimpleXOR原型**: 提供基于密码的SimpleXOR解密原型（仍需更多真实词典回归验证）

### 未实现部分
- **密码输入/管理**: UI/CLI尚未提供稳定的密码输入与持久化机制
- **强加密格式**: DES/Blowfish/AES等仍未实现（需要引入加密库并做兼容性回归）
- **兼容性回归**: 需要更多真实世界的加密MDX样本验证与边界用例覆盖

## 📋 MDict加密格式分析

### MDict加密类型
1. **SimpleXOR加密**: 简单XOR加密
2. **DES加密**: 数据加密标准
3. **Blowfish加密**: 对称加密算法
4. **AES加密**: 高级加密标准
5. **自定义加密**: 第三方开发者实现的加密

### 加密检测机制
```cpp
std::string enc = extract_attr(head, "encrypted");
if (!enc.empty()) {
    encrypted_ = (enc != "0" && enc != "no" && enc != "false");
}
```

### 当前实现
```cpp
// If encrypted, attempt decryption (best-effort). If decryption fails,
// keep dictionary metadata available but skip content parsing.
if (encrypted_) {
    // MdictDecryptorStd::detect_encryption_type(...)
    // MdictDecryptorStd::decrypt(...)
}
```

## 🎯 实施计划

### 阶段1: 加密检测和元数据提取 (已完成)
- [x] 检测加密标记
- [x] 提取基本信息（名称、描述、版本等）
- [x] 安全处理加密内容

### 阶段2: SimpleXOR加密支持 (优先级：高)
- [ ] 实现SimpleXOR解密算法
- [ ] 密钥生成和管理
- [ ] 解密内容验证

### 阶段3: DES/Blowfish加密支持 (优先级：中)
- [ ] 集成加密库（OpenSSL/Botan）
- [ ] 实现DES解密
- [ ] 实现Blowfish解密
- [ ] 密码接口设计

### 阶段4: AES加密支持 (优先级：低)
- [ ] AES算法实现
- [ ] 高级加密特性支持
- [ ] 性能优化

### 阶段5: 用户界面集成
- [ ] 密码输入对话框
- [ ] 密码缓存机制
- [ ] 错误处理和用户提示

## 🔧 技术实现建议

### 1. 加密库选择
**推荐**: Botan
- 现代C++加密库
- MIT许可证
- 全面的算法支持
- 良好的API设计

**备选**: OpenSSL
- 系统广泛支持
- 性能优秀
- 丰富的算法支持

### 2. 密码管理策略
```cpp
class MdictDecryptionContext {
public:
    bool set_password(const std::string& password);
    bool try_decrypt(const std::string& encrypted_data, std::string& output);
    void clear_password(); // 安全清除内存
private:
    std::string password_;
    bool has_password_ = false;
};
```

### 3. 接口设计
```cpp
// 在DictionaryParserStd中添加
virtual bool set_decryption_password(const std::string& password) = 0;
virtual bool is_encrypted() const = 0;
virtual bool requires_password() const = 0;
```

### 4. 性能考虑
- **延迟解密**: 只在需要时解密特定条目
- **缓存机制**: 缓存已解密的内容
- **并行处理**: 多线程解密支持
- **内存管理**: 及时清除敏感数据

## 📊 兼容性研究

### 现有MDict工具对比
| 工具 | 加密支持 | 支持格式 | 密码处理 |
|------|----------|----------|----------|
| GoldenDict | ✓ (有限) | DES, Blowfish | 弹窗输入 |
| MDict | ✓ | 全格式 | 密码文件 |
| ZDic | ✓ (部分) | SimpleXOR | 手动输入 |
| Unidict | ✗ | - | - |

### 用户需求调研
1. **学习词典**: 大多数用户使用公开词典，无需加密
2. **专业词典**: 医学、法律等专业词典常有加密
3. **个人词典**: 用户自制的加密词典
4. **商业词典**: 付费词典的版权保护

## 🧪 测试策略

### 测试文件准备
1. **SimpleXOR测试文件**: 创建已知密钥的测试文件
2. **标准加密样本**: 使用公开的MDict加密样本
3. **边界测试**: 空密码、错误密码等边界情况
4. **性能测试**: 大文件解密性能

### 测试用例设计
```cpp
// 解密功能测试
TEST(MdictParserStd, SimpleXORDecryption) {
    // 创建已知内容的加密文件
    // 测试解密功能
}

// 密码处理测试
TEST(MdictParserStd, PasswordManagement) {
    // 测试密码设置和清除
    // 测试错误密码处理
}
```

## 📈 实施优先级

### 高优先级 (立即实施)
1. **SimpleXOR支持**: 最简单且使用较多
2. **密码输入机制**: 基础的用户交互
3. **错误处理**: 完善的异常处理和用户提示

### 中优先级 (后续版本)
1. **DES/Blowfish支持**: 覆盖更多加密格式
2. **性能优化**: 解密缓存和并行处理
3. **UI集成**: 与现有界面的无缝集成

### 低优先级 (长期规划)
1. **AES支持**: 现代加密标准
2. **高级特性**: 密码管理、批量处理等
3. **插件架构**: 第三方加密算法支持

## 🚀 开发建议

### 1. 渐进式实施
- 从最简单的SimpleXOR开始
- 逐步增加复杂算法
- 每个阶段都进行充分测试

### 2. 向后兼容
- 保持现有API不变
- 新功能通过可选参数提供
- 优雅处理加密/非加密文件

### 3. 安全考虑
- 密码内存安全（及时清除）
- 防止时序攻击
- 安全的密码存储机制

### 4. 用户体验
- 清晰的错误提示
- 密码输入限制
- 解密进度显示

## 📚 参考资料

### MDict规范文档
- MDict 2.0格式规范
- 加密算法详细说明
- 第三方工具兼容性说明

### 加密算法资料
- DES算法标准和实现
- Blowfish算法细节
- AES加密规范
- XOR加密变体

### 开源项目参考
- GoldenDict的加密实现
- MDict客户端源码
- 相关加密库文档

---

**注意**: 实施MDict加密支持需要仔细考虑法律和版权问题。建议：
1. 只支持合法的加密格式
2. 遵守相关软件许可证
3. 提供充分的免责声明
4. 鼓励用户使用开源词典
