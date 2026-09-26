# Unidict 插件开发指南

欢迎Unidict插件开发者社区！本指南将帮助您为Unidict创建自定义词典格式解析器和其他扩展功能。

## 快速开始

### 开发环境要求

- **编译器**: C++20或更高版本
- **构建系统**: CMake 3.20或更高版本
- **平台支持**: Windows (MSVC 2022+), macOS (Clang 12+), Linux (GCC 10+)
- **依赖项**:
  - C++20标准库
  - zlib (对于压缩格式）
  - （可选）Qt6 (用于GUI插件）

### 插件架构概览

```
Unidict Plugin Architecture

┌─────────────────────────────────────────────────────┐
│                    Core Layer (C++20)                  │
├─────────────────────────────────────────────────────┤
│                                                    │
│  ┌─────────────────┐    ┌─────────────────┐  │
│  │   DictionaryParser │    │  SearchEngine  │  │
│  │    (Interface)   │    │  (Interface)  │  │
│  └─────────────────┘    └─────────────────┘  │
│                                                    │
├─────────────────────────────────────────────────────┤
│                  Plugin Adapters (Qt6)                 │
├─────────────────────────────────────────────────────┤
│              ┌─────────────────────────────────────┐   │
│              │     PluginManager     │   │
│              │     (Registration & Discovery)   │   │
│              └─────────────────────────────────────┘   │
├─────────────────────────────────────────────────────┤
│                   UI Layer (Qt/QML)                    │
├─────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌─────────────────┐    │
│  │   LookupService │    │  DictionaryManager │  │
│  │    (API)         │    │    (API)         │  │
│  └─────────────────┘    └─────────────────┘    │
├─────────────────────────────────────────────────────┤
│                  Data Layer (Storage & Sync)             │
├─────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌─────────────────┐    │
│  │    DataStore      │    │  FullTextIndex  │  │
│  │    (JSON/SQLite)  │    │  (Inverted Index)│  │
│  └─────────────────┘    └─────────────────┘    │
└─────────────────────────────────────────────────────┘
```

## 核心接口

### DictionaryParser 接口

所有词典解析器都必须实现`DictionaryParser`接口：

```cpp
// core/dictionary_parser.h
class DictionaryParser {
public:
    virtual ~DictionaryParser() = default;

    // 基本接口
    virtual bool load_dictionary(const std::string& path) = 0;
    virtual bool is_loaded() const = 0;
    virtual std::string dictionary_name() const = 0;
    virtual std::string dictionary_description() const = 0;
    virtual int word_count() const = 0;

    // 搜索接口
    virtual std::string lookup(const std::string& word) const = 0;
    virtual std::vector<std::string> find_similar(const std::string& word, int max_results) const = 0;
    virtual std::vector<std::string> all_words() const = 0;

    // 高级接口（可选）
    virtual bool supports_fast_lookup() const { return false; }
    virtual bool supports_prefix_search() const { return false; }
    virtual bool supports_regex_search() const { return false; }
    virtual bool supports_fulltext_search() const { return false; }
};
```

### 搜索引擎接口

```cpp
// core/search_engine.h
class SearchEngine {
public:
    virtual ~SearchEngine() = default;

    // 搜索方法
    virtual std::vector<SearchResult> exact_search(const std::string& query) = 0;
    virtual std::vector<SearchResult> prefix_search(const std::string& prefix, int max_results) = 0;
    virtual std::vector<SearchResult> fuzzy_search(const std::string& word, int max_results) = 0;
    virtual std::vector<SearchResult> wildcard_search(const std::string& pattern, int max_results) = 0;
    virtual std::vector<SearchResult> regex_search(const std::string& pattern, int max_results) = 0;
    virtual std::vector<SearchResult> fulltext_search(const std::string& query, int max_results) = 0;

    // 配置方法
    virtual void set_max_results(int max_results) = 0;
    virtual void set_case_sensitive(bool sensitive) = 0;
    virtual void enable_ranking(bool enable) = 0;
};
```

### 搜索结果结构

```cpp
// core/types.h
struct SearchResult {
    std::string word;
    std::string definition;
    std::string dictionary_name;
    float relevance_score;
    std::string matched_pattern;
};
```

## 插件类型

### 1. 词典格式插件

创建新词典格式支持：

#### 示例：EPUB词典解析器

```cpp
// plugins/epub_parser.h
#pragma once
#include "core/dictionary_parser.h"

class EPUBParser : public DictionaryParser {
private:
    std::string book_title_;
    std::unordered_map<std::string, std::string> entries_;
    std::vector<std::string> words_;

public:
    EPUBParser() = default;
    ~EPUBParser() override;

    bool load_dictionary(const std::string& epub_path) override;
    bool is_loaded() const override;
    std::string dictionary_name() const override;
    std::string dictionary_description() const override;
    int word_count() const override;
    std::string lookup(const std::string& word) const override;
    std::vector<std::string> find_similar(const std::string& word, int max_results) const override;
    std::vector<std::string> all_words() const override;

    // EPUB特有方法
    std::string get_chapter(int chapter_number) const;
    std::vector<std::string> get_toc() const;
    std::vector<std::string> get_metadata() const;
};
```

#### 注册插件

```cpp
// plugins/epub_plugin.cpp
#include "epub_parser.h"
#include "plugins/plugin_manager.h"

class EPUBPlugin {
public:
    static const char* NAME = "EPUB Dictionary";
    static const char* VERSION = "1.0.0";
    static const char* DESCRIPTION = "EPUB格式电子书词典解析器";
    static const char* AUTHOR = "Your Name";

    static DictionaryParser* create_parser() {
        return new EPUBParser();
    }

    // 插件元信息
    static PluginMetadata get_metadata() {
        return PluginMetadata{
            .name = NAME,
            .version = VERSION,
            .description = DESCRIPTION,
            .author = AUTHOR,
            .supported_formats = {".epub"},
            .min_unidict_version = "1.0.0"
        };
    }
};
```

### 2. 功能扩展插件

创建功能增强插件：

#### 示例：AI翻译插件

```cpp
// plugins/ai_translator.h
#pragma once
#include "core/types.h"
#include <functional>

class AITranslator {
private:
    std::string api_key_;
    std::string service_endpoint_;

public:
    AITranslator(const std::string& api_key, const std::string& endpoint);
    ~AITranslator() = default;

    // 翻译接口
    std::string translate(const std::string& text, const std::string& target_lang) const;
    std::string detect_language(const std::string& text) const;

    // 异步翻译
    using TranslationCallback = std::function<void(const std::string&)>;
    void translate_async(const std::string& text, const std::string& target_lang, TranslationCallback callback) const;

    // 流式翻译
    using StreamCallback = std::function<void(const std::string&)>;
    void translate_stream(const std::string& text, const std::string& target_lang, StreamCallback callback) const;
};
```

### 3. UI插件

创建Qt/QML插件：

#### 示例：自定义搜索结果显示

```qml
// plugins/custom_search_display.qml
import QtQuick 2.15

Rectangle {
    width: parent.width
    height: parent.height
    color: "white"

    CustomSearchView {
        id: searchView
        anchors.fill: parent
        model: searchResults

        delegate: CustomResultDelegate {
            width: parent.width

            contentItem: CustomResultItem {
                word: model.word
                definition: model.definition
                dictionaryName: model.dictionaryName
                relevanceScore: model.relevanceScore
            }
        }
    }

    Component {
        id: searchResults

        property var searchQuery: ""
        property var searchResults: []

        function performSearch(query) {
            searchQuery = query
            // 调用核心搜索API
            dictionaryManager.search(query, function(results) {
                searchResults = results
            })
        }
    }
}
```

## 插件注册系统

### 动态库插件

使用动态库实现插件：

```cpp
// plugins/dll_loader.h
#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
    PLUGIN_EXPORT DictionaryParser* create_plugin();
    PLUGIN_EXPORT const char* get_plugin_name();
    PLUGIN_EXPORT const char* get_plugin_version();
    PLUGIN_EXPORT PluginMetadata get_plugin_metadata();
}
```

### 静态库插件

```cmake
# CMakeLists.txt for a plugin
cmake_minimum_required(3.20)
project(MyUnidictPlugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件类型
add_library(my_plugin SHARED
    src/plugin.cpp
    src/parser.cpp
)

# 导出符号
target_compile_definitions(my_plugin PRIVATE PLUGIN_EXPORT)

# 链接到Unidict核心（如果需要）
target_link_libraries(my_plugin PRIVATE unidict_core)

# 安装到Unidict插件目录
install(TARGETS my_plugin
    LIBRARY DESTINATION ${UNIDICT_PLUGIN_DIR}
)
```

## 开发工具链

### 开发环境设置

```bash
# 克隆Unidict仓库
git clone https://github.com/unidict/unidict.git
cd unidict

# 创建插件目录
mkdir -p my_plugin

# 创建基本的插件项目结构
mkdir -p my_plugin/src
mkdir -p my_plugin/include
mkdir -p my_plugin/tests
```

### 项目模板

#### CMakeLists.txt模板

```cmake
cmake_minimum_required(3.20)
project(MyUnidictPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Unidict
find_package(Unidict 1.0.0 REQUIRED)

if(Unidict_FOUND)
    message(STATUS "Found Unidict: ${Unidict_VERSION}")
else()
    message(FATAL_ERROR "Unidict not found")
endif()

# 插件源文件
set(PLUGIN_SOURCES
    src/plugin.cpp
    src/parser.cpp
    src/metadata.cpp
)

# 创建插件库
add_library(my_plugin SHARED ${PLUGIN_SOURCES})

# 设置导出符号
target_compile_definitions(my_plugin PRIVATE
    UNIDICT_PLUGIN_EXPORT
    MY_PLUGIN_VERSION="${PROJECT_VERSION}"
)

# 生成元数据头
set(PLUGIN_METADATA_HEADER "${CMAKE_CURRENT_BINARY_DIR}/plugin_metadata.h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/plugin_metadata.h.in"
    "${PLUGIN_METADATA_HEADER}"
    @ONLY
)

# 生成导出宏文件
set(PLUGIN_EXPORTS_FILE "${CMAKE_CURRENT_BINARY_DIR}/plugin_exports.cpp")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/plugin_exports.cpp.in"
    "${PLUGIN_EXPORTS_FILE}"
    @ONLY
)

# 编译导出文件
target_sources(my_plugin PRIVATE ${PLUGIN_EXPORTS_FILE})

# 包含Unidict核心（如果需要）
target_include_directories(my_plugin PRIVATE ${Unidict_INCLUDE_DIRS})
target_link_libraries(my_plugin PRIVATE ${Unidict_LIBRARIES})

# 安装规则
install(TARGETS my_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
```

### 测试框架

```cpp
// tests/test_my_plugin.cpp
#include <gtest/gtest.h>
#include "plugins/my_plugin.h"

class MyPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin_ = std::make_unique<MyPlugin>();
    }

public:
    TEST_F(MyPluginTest, LoadDictionary) {
        // 测试词典加载
        EXPECT_TRUE(plugin_->load_dictionary("test.epub"));
        EXPECT_TRUE(plugin_->is_loaded());
        EXPECT_GT(plugin_->word_count(), 0);
    }

    TEST_F(MyPluginTest, SearchFunctionality) {
        plugin_->load_dictionary("test.epub");

        // 测试搜索功能
        auto result = plugin_->lookup("test");
        EXPECT_FALSE(result.empty());

        // 测试模糊搜索
        auto similar = plugin_->find_similar("tes", 5);
        EXPECT_GT(similar.size(), 0);
    }

    TEST_F(MyPluginTest, ErrorHandling) {
        // 测试错误处理
        EXPECT_FALSE(plugin_->load_dictionary("nonexistent.epub"));
        EXPECT_FALSE(plugin_->is_loaded());
    }

private:
    std::unique_ptr<MyPlugin> plugin_;
};
```

## 构建和分发

### 构建插件

```bash
# 构建插件
cd my_plugin
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 测试插件
ctest --output-on-failure
```

### 插件安装

#### 自动发现

Unidict会在以下目录自动发现插件：

- **Windows**: `%APPDATA%/Unidict/plugins`
- **macOS**: `~/Library/Application Support/Unidict/plugins`
- **Linux**: `~/.local/share/unidict/plugins`

#### 手动安装

```bash
# 复制插件到Unidict插件目录
cp build/libmy_plugin.so ~/.local/share/unidict/plugins/

# 复制插件元数据
cp my_plugin.json ~/.local/share/unidict/plugins/
```

### 插件元数据

```json
// my_plugin.json
{
    "name": "My EPUB Dictionary Plugin",
    "version": "1.0.0",
    "description": "EPUB格式电子书词典解析器",
    "author": "Your Name",
    "license": "MIT",
    "website": "https://github.com/yourname/my-unidict-plugin",
    "repository": "https://github.com/yourname/my-unidict-plugin",

    "plugin_type": "dictionary_parser",
    "supported_formats": [".epub"],
    "min_unidict_version": "1.0.0",
    "dependencies": [],

    "capabilities": {
        "dictionary_parsing": true,
        "fast_lookup": true,
        "prefix_search": true,
        "fuzzy_search": false,
        "regex_search": false,
        "fulltext_search": false
    },

    "entry_points": {
        "dictionary_parser": "create_plugin",
        "ui_components": ["custom_search_display"],
        "settings_page": "my_plugin_settings"
    }
}
```

## 调试和测试

### 日志系统

```cpp
// 在插件中使用Unidict日志系统
#include "core/logger.h"

void MyPlugin::load_dictionary(const std::string& path) {
    Logger::info("Loading dictionary: {}", path);

    try {
        // 解析逻辑...
        Logger::info("Dictionary loaded successfully: {} words", word_count());
    } catch (const std::exception& e) {
        Logger::error("Failed to load dictionary: {}", e.what());
    }
}
```

### 性能分析

```cpp
// 使用性能监控
#include "core/performance_monitor.h"

class PerformanceMonitoredParser : public DictionaryParser {
private:
    std::unique_ptr<PerformanceMonitor> perf_monitor_;

public:
    PerformanceMonitoredParser() : perf_monitor_(std::make_unique<PerformanceMonitor>()) {}

    std::string lookup(const std::string& word) const override {
        auto timer = perf_monitor_->start_timer("lookup");

        auto result = do_lookup(word);

        timer.stop();
        Logger::debug("Lookup completed in {}ms", timer.elapsed_ms());

        return result;
    }
};
```

## API最佳实践

### 内存管理

```cpp
class GoodPlugin : public DictionaryParser {
private:
    // 使用RAII和智能指针
    std::unique_ptr<DataStructure> data_;
    std::vector<std::unique_ptr<Resource>> resources_;

public:
    GoodPlugin() : data_(std::make_unique<DataStructure>()) {}

    // 自动清理资源
    ~GoodPlugin() {
        // resources_会自动清理
        // data_会自动清理
    }
};
```

### 错误处理

```cpp
class RobustPlugin : public DictionaryParser {
public:
    bool load_dictionary(const std::string& path) override {
        try {
            // 尝试加载
            return do_load_dictionary(path);
        } catch (const std::ios::failure& e) {
            Logger::error("IO error: {}", e.what());
            return false;
        } catch (const std::bad_alloc& e) {
            Logger::error("Memory allocation failed: {}", e.what());
            return false;
        } catch (const std::exception& e) {
            Logger::error("Unexpected error: {}", e.what());
            return false;
        }
    }
};
```

### 配置管理

```cpp
class ConfigurablePlugin : public DictionaryParser {
private:
    PluginConfig config_;

public:
    ConfigurablePlugin() = default;

    virtual void load_config(const std::string& config_path) {
        config_.load_from_file(config_path);
    }

    virtual void save_config(const std::string& config_path) {
        config_.save_to_file(config_path);
    }

protected:
    const PluginConfig& get_config() const { return config_; }
};
```

## 插件分发

### 打包

```bash
# 创建插件包
mkdir -p my_plugin_package
cp build/libmy_plugin.so my_plugin_package/
cp my_plugin.json my_plugin_package/
cp README.md my_plugin_package/
cp LICENSE my_plugin_package/

# 创建安装脚本
cat > my_plugin_package/install.sh << 'EOF'
#!/bin/bash
PLUGIN_DIR="$HOME/.local/share/unidict/plugins"
mkdir -p "$PLUGIN_DIR"

echo "Installing plugin..."
cp *.so "$PLUGIN_DIR/"
cp *.json "$PLUGIN_DIR/"

echo "Plugin installed successfully!"
echo "Restart Unidict to load the new plugin."
EOF

chmod +x my_plugin_package/install.sh
```

### 版本管理

```bash
# 版本标签
git tag -a v1.0.0 -m "Release version 1.0.0"
git push origin v1.0.0

# 生成发布包
tar -czvf my-plugin-v1.0.0.tar.gz \
    my_plugin/ \
    --transform 's/^my-plugin/my-plugin-v1.0.0\//' \
    --exclude-vcs
```

## 社区资源

### 官方插件仓库

- **Unidict官方插件**: https://github.com/unidict/plugins
- **社区插件**: https://github.com/topics/unidict-plugin
- **插件模板**: https://github.com/unidict/plugin-template

### 贡献指南

1. **Fork官方模板仓库**
   ```bash
   git clone https://github.com/unidict/plugin-template my-awesome-plugin
   cd my-awesome-plugin
   ```

2. **实现您的插件**
   - 按照本指南实现所需功能
   - 遵循C++20和Unidict核心API规范

3. **测试您的插件**
   - 编写全面的单元测试
   - 使用Unidict测试框架
   - 确保跨平台兼容性

4. **文档编写**
   - 提供详细的README.md
   - 包含API文档
   - 添加使用示例

5. **提交PR**
   - 创建Pull Request到官方仓库
   - 通过CI/CD检查
   - 等待社区审核

## 技术支持

### 常见问题

**Q: 如何调试插件？**
A: 使用Unidict日志系统，设置日志级别为DEBUG：
   ```cpp
   Logger::set_level(Logger::DEBUG);
   ```

**Q: 插件加载失败怎么办？**
A: 检查插件元数据文件格式、依赖库版本、API兼容性

**Q: 如何优化大词典性能？**
A: 实现索引缓存、延迟加载、内存映射等技术

**Q: 如何支持加密词典？**
A: 使用Unidict的解密框架，或集成第三方解密库

### 联系方式

- **GitHub Issues**: https://github.com/unidict/unidict/issues
- **讨论论坛**: https://github.com/unidict/unidict/discussions
- **开发者邮件**: dev@unidict.org

---

**开始开发您的插件，扩展Unidict的功能吧！**

*本指南会随着Unidict的更新而持续完善。*