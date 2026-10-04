#include "dictionary_std.h"

#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string lcase(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

bool DictionaryStd::load(const std::string& path) {
    auto ext = lcase(fs::path(path).extension().string());
    src_paths_.push_back(path);
    if (ext == ".json") {
        auto p = std::make_unique<JsonParserStd>();
        if (!p->load_dictionary(path)) { load_error_ = "failed to load dictionary: " + path; return false; }
        json_ = std::move(p); name_ = json_->name(); words_ = json_->all_words();
    } else if (ext == ".ifo") {
        auto p = std::make_unique<StarDictParserStd>();
        if (!p->load_dictionary(path)) { load_error_ = "failed to load dictionary: " + path; return false; }
        stardict_ = std::move(p); name_ = stardict_->dictionary_name(); words_ = stardict_->all_words();
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
        auto p = std::make_unique<MdictParserStd>();
        if (!p->load_dictionary(path)) { load_error_ = "failed to load dictionary: " + path; return false; }
        mdict_ = std::move(p); name_ = mdict_->dictionary_name(); words_ = mdict_->all_words();
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
    } else if (ext == ".dsl") {
        auto p = std::make_unique<DslParserStd>();
        if (!p->load_dictionary(path)) { load_error_ = "failed to load dictionary: " + path; return false; }
        dsl_ = std::move(p); name_ = dsl_->dictionary_name(); words_ = dsl_->all_words();
    } else if (ext == ".csv" || ext == ".tsv" || ext == ".txt") {
        auto p = std::make_unique<CsvParserStd>();
        if (!p->load_dictionary(path)) { load_error_ = "failed to load dictionary: " + path; return false; }
        csv_ = std::move(p); name_ = csv_->dictionary_name(); words_ = csv_->all_words();
    } else if (ext == ".epub") {
        auto p = std::make_unique<EpubParserStd>();
        if (!p->load_dictionary(path)) { load_error_ = "failed to load dictionary: " + path; return false; }
        epub_ = std::move(p); name_ = epub_->dictionary_name(); words_ = epub_->all_words();
    } else {
        load_error_ = std::string(kUnsupportedExtPrefix) + ": " + ext;
        return false;
    }
    return true;
}

std::string DictionaryStd::lookup(const std::string& w) const {
    if (json_) return json_->lookup(w);
    if (stardict_) return stardict_->lookup(w);
    if (mdict_) return mdict_->lookup(w);
    if (dsl_) return dsl_->lookup(w);
    if (csv_) return csv_->lookup(w);
    if (epub_) return epub_->lookup(w);
    // 无解析器（默认构造或 load 失败的实例）：无释义。manager 只持
    // load 成功的实例，此臂由公开 API 面的空实例覆盖。
    return {};
}

std::string DictionaryStd::description() const {
    if (json_) return json_->description();
    if (stardict_) return stardict_->dictionary_description();
    if (mdict_) return mdict_->dictionary_description();
    if (dsl_) return dsl_->dictionary_description();
    if (csv_) return csv_->dictionary_description();
    if (epub_) return epub_->dictionary_description();
    return {};
}

} // namespace UnidictCoreStd