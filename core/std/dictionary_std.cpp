#include "dictionary_std.h"

#include <cctype>
#include <filesystem>
#include <utility>

#include "parser_registry_std.h"

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string lcase(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

bool DictionaryStd::load(const std::string& path) {
    const std::string ext = lcase(fs::path(path).extension().string());
    src_paths_.push_back(path);

    // 扩展名 → 解析器：注册表未命中即「扩展名不支持」（运行期档，
    // 不建隔离记录）；命中但加载失败才是「解析失败」（持久隔离档）
    auto parser = ParserRegistryStd::instance().create(ext);
    if (!parser) {
        load_error_ = std::string(kUnsupportedExtPrefix) + ": " + ext;
        return false;
    }
    if (!parser->load_dictionary(path)) {
        load_error_ = "failed to load dictionary: " + path;
        return false;
    }
    name_ = parser->dictionary_name();
    words_ = parser->all_words();

    if (ext == ".ifo") {
        // 伴生文件：.idx 与 .dict/.dict.dz 旁置 .ifo
        fs::path base = fs::path(path);
        base.replace_extension("");
        fs::path idx = base; idx += ".idx";
        fs::path dict = base; dict += ".dict";
        fs::path dz = base; dz += ".dict.dz";
        std::error_code ec;
        // GCOVR_EXCL_LINE：stardict 解析器 load_dictionary 成功的前提是
        // .idx 存在（stardict_parser_std.cpp：`if (!fs::exists(idx))
        // return false;`），走到伴生扫描时 idx 必在，假臂结构不可达。
        if (fs::exists(idx, ec)) src_paths_.push_back(idx.string());  // GCOVR_EXCL_LINE
        // GCOVR_EXCL_LINE：解析器成功还要求 .dict 与 .dict.dz 至少一个
        // 存在（否则 return false），故上一行假时 dz 必在，假臂不可达。
        if (fs::exists(dict, ec)) src_paths_.push_back(dict.string());
        else if (fs::exists(dz, ec)) src_paths_.push_back(dz.string());  // GCOVR_EXCL_LINE
    } else if (ext == ".mdx") {
        // 伴生 .mdd：路径进 src_paths（签名绑定）；资源解析失败只跳过
        // 资源表，不因资源损坏拒绝词典本体
        fs::path dir = fs::path(path).parent_path();
        std::string stem = fs::path(path).stem().string();
        std::error_code ec;
        for (auto& de : fs::directory_iterator(dir, ec)) {
            if (!de.is_regular_file()) continue;
            fs::path q = de.path();
            if (lcase(q.extension().string()) == ".mdd" && q.stem().string() == stem) {
                src_paths_.push_back(q.string());
                auto rp = std::make_unique<MddResourceParser>();
                if (rp->load(q.string())) mdd_parsers_.push_back(std::move(rp));
            }
        }
    }

    parser_ = std::move(parser);
    return true;
}

std::string DictionaryStd::lookup(const std::string& w) const {
    // 无解析器（默认构造或 load 失败的实例）：无释义。manager 只持
    // load 成功的实例，空实例臂由公开 API 面的测试直接覆盖。
    return parser_ ? parser_->lookup(w) : std::string{};
}

std::string DictionaryStd::description() const {
    return parser_ ? parser_->dictionary_description() : std::string{};
}

} // namespace UnidictCoreStd
