# Contributing to Unidict

感谢您对Unidict项目的兴趣！我们欢迎各种形式的贡献，包括但不限于代码、文档、测试、bug报告和功能建议。

## 🤝 贡献方式

### 报告问题
- 使用GitHub Issues报告bug或提出功能请求
- 提供详细的重现步骤和系统环境信息
- 搜索现有issues以避免重复报告

### 代码贡献
- Fork本仓库并创建功能分支
- 遵循现有的代码风格和架构模式
- 添加适当的测试覆盖
- 确保所有测试通过
- 提交Pull Request

### 文档改进
- 改进用户文档和API文档
- 添加使用示例和教程
- 翻译文档到其他语言

### 测试
- 为新功能编写测试
- 测试各种词典格式和边界情况
- 性能测试和优化

## 🏗️ 开发环境设置

### 前置要求
- CMake 3.20+
- C++20兼容编译器（GCC 10+, Clang 12+, MSVC 19.3+）
- Qt 6.3+ (可选，用于GUI版本)
- zlib开发库

### 构建项目
```bash
# 克隆仓库
git clone https://github.com/your-username/unidict.git
cd unidict

# 标准构建（包含Qt支持）
cmake -B build -S .
cmake --build build -j

# 仅构建std版本（无Qt依赖）
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF \
  -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF \
  -DUNIDICT_BUILD_QT_TESTS=OFF \
  -DUNIDICT_BUILD_STD_CLI=ON
cmake --build build-std -j

# 运行测试
ctest --test-dir build --output-on-failure
```

## 📁 项目架构

### 核心组件
```
unidict/
├── core/                    # C++核心库（无Qt依赖）
│   ├── dictionary_parser*    # 词典解析器接口
│   ├── index_engine*        # 搜索索引引擎
│   ├── data_store*          # 数据存储
│   └── search_engine*       # 搜索算法
├── adapters/qt/            # Qt适配器层
│   ├── *_qt.cpp            # Qt桥接实现
│   └── *_qt.h              # Qt接口定义
├── plugins/                # 词典格式插件
├── tools/cli/              # Qt版本CLI工具
├── cli-std/               # std版本CLI工具
├── qmlui/                 # QML用户界面
└── tests/                 # 测试套件
```

### 代码风格
- 使用C++20现代特性
- 遵循RAII和智能指针原则
- 函数和变量使用snake_case命名
- 类名使用PascalCase命名
- 保持核心层与Qt层的分离

## 🧪 测试指南

### 运行测试
```bash
# 运行所有测试
ctest --test-dir build

# 运行特定测试
ctest --test-dir build -R test_mdict_std
ctest --test-dir build -R test_stardict_std
```

### 添加新测试
1. 在相应的`tests/`子目录中创建测试文件
2. 使用Google Test框架
3. 包含正面和负面测试用例
4. 测试边界条件和错误处理

### 测试命名约定
- `test_<module>_std.cpp` - std版本的测试
- `test_<component>.cpp` - Qt组件的测试
- 测试用例使用`TEST(TestSuite, TestName)`格式

## 🔧 开发工作流

### 1. 功能开发
- 创建功能分支：`git checkout -b feature/new-feature`
- 实现功能并编写测试
- 确保代码通过所有检查
- 提交Pull Request

### 2. Bug修复
- 创建分支：`git checkout -b fix/bug-description`
- 编写重现测试（如果可能）
- 修复问题并验证测试通过
- 提交Pull Request

### 3. 代码审查
- 所有代码都需要经过审查
- 关注代码质量、性能和安全性
- 确保文档同步更新

## 📝 提交规范

### 提交消息格式
```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

### 类型说明
- `feat`: 新功能
- `fix`: Bug修复
- `docs`: 文档更新
- `style`: 代码格式（不影响功能）
- `refactor`: 代码重构
- `test`: 测试相关
- `chore`: 构建过程或辅助工具的变动

### 示例
```
feat(parser): add support for encrypted MDict files

- Implement decryption for MDict v2.0 format
- Add comprehensive error handling
- Include unit tests for encrypted files

Closes #123
```

## 🔍 词典格式支持

### 支持的格式
- **StarDict** (.ifo, .idx, .dict/.dict.dz)
- **MDict** (.mdx, .mdd) - 支持多种压缩和布局
- **DSL** (.dsl)
- **JSON** - 自定义格式
- **Plain Text** (.txt, .csv, .tsv)

### 添加新格式支持
1. 在`core/`中实现新的解析器
2. 继承`DictionaryParser`接口
3. 在`PluginManager`中注册格式
4. 添加全面的测试用例
5. 更新文档

## 🚀 性能考虑

### 性能目标
- 大型词典（>100万词条）的毫秒级搜索响应
- 内存高效的数据结构
- 快速索引构建和持久化
- 跨平台一致性

### 优化指南
- 使用内存映射文件访问
- 实现高效的二分搜索
- 考虑缓存策略
- 性能测试基准
- 内存使用分析

## 📚 文档

### API文档
- 使用Doxygen格式注释
- 为公共接口提供详细说明
- 包含使用示例

### 用户文档
- 更新README.md
- 添加教程和指南
- 维护变更日志

## 🏷️ 发布流程

### 版本号规范
遵循[语义化版本](https://semver.org/lang/zh-CN/)：
- `MAJOR.MINOR.PATCH`
- 主版本号：不兼容的API修改
- 次版本号：向下兼容的功能性新增
- 修订号：向下兼容的问题修正

### 发布检查清单
- [ ] 所有测试通过
- [ ] 文档已更新
- [ ] 性能基准测试通过
- [ ] 版本号已更新
- [ ] 变更日志已编写
- [ ] 安全审查完成

## 🤝 社区准则

### 行为准则
- 尊重所有参与者
- 保持专业和友善
- 接受建设性的反馈
- 专注于对社区最有利的事情

### 沟通渠道
- GitHub Issues - Bug报告和功能请求
- GitHub Discussions - 一般讨论和问答
- Pull Requests - 代码审查和讨论

## 📄 许可证

通过贡献代码，您同意您的贡献将在[MIT许可证](LICENSE)下发布。

## 🙏 致谢

感谢所有为Unidict做出贡献的开发者！您的贡献使这个项目变得更好。

---

如果您有任何问题或需要帮助，请随时在GitHub Issues中提问。我们期待您的贡献！