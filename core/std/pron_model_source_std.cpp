#include "std/pron_model_source_std.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "std/sha256_std.h"

namespace UnidictCoreStd {
namespace {

namespace fs = std::filesystem;

// 与上游 LFS pointer 一致的两个资产（HF 仓库 sadda-speech/
// wav2vec2-espeak-ctc，Apache-2.0）。size/hash 互相独立地钉了两道：
// 尺寸先挡掉"下到一半"的情况，哈希挡掉"下全了但字节是坏的"。
const std::vector<ModelAsset> kAssets = {
    ModelAsset{"model", "model.onnx",
               "https://huggingface.co/sadda-speech/wav2vec2-espeak-ctc/"
               "resolve/main/model.onnx",
               "35c2f8484b74737c7816e8762c095d6f7dc40ded7c36d249c8134d8f30d717b4",
               635089013,
               "wav2vec2 的 CTC 声学模型（fp16 ONNX，50 帧/秒）"},
    ModelAsset{"vocab", "vocab.json",
               "https://huggingface.co/sadda-speech/wav2vec2-espeak-ctc/"
               "resolve/main/vocab.json",
               "d732ab2456c0c017930001dc9af0b41b3b93d25b2eb9740bf9d925508d7d87d0",
               4637, "espeak 音素 → 类 id 的词表"},
};

inline const char* env_or_null(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : nullptr;
}

inline std::string join_path(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) return leaf;
    if (dir.back() == '/' || dir.back() == '\\') return dir + leaf;
    return dir + "/" + leaf;
}

}  // namespace

const std::vector<ModelAsset>& pron_model_assets() { return kAssets; }

const ModelAsset* find_pron_model_asset(const std::string& key) {
    for (const ModelAsset& a : kAssets) {
        if (a.key == key) {
            return &a;
        }
    }
    return nullptr;
}

std::string pron_model_dir() {
    if (const char* v = env_or_null("UNIDICT_PRON_MODEL_DIR")) {
        return std::string(v);
    }
    // Windows 上 HOME 通常不设，USERPROFILE 才是
    const char* home = env_or_null("HOME");
    if (!home) home = env_or_null("USERPROFILE");
    if (home) {
        return join_path(std::string(home),
                         ".cache/unidict-models/wav2vec2-espeak-ctc");
    }
    // 连 home 都拿不到（受限环境）：退到当前目录下，行为与旧 GUI 一致
    return ".cache/unidict-models/wav2vec2-espeak-ctc";
}

long long pron_model_total_bytes() {
    long long total = 0;
    for (const ModelAsset& a : kAssets) {
        total += a.size_bytes;
    }
    return total;
}

std::string pron_asset_path(const ModelAsset& asset, const std::string& dir) {
    return join_path(dir, asset.filename);
}

std::string pron_asset_part_path(const ModelAsset& asset,
                                 const std::string& dir) {
    return pron_asset_path(asset, dir) + ".part";
}

std::vector<std::string> missing_assets(const std::vector<ModelAsset>& assets,
                                        const std::string& dir) {
    std::vector<std::string> missing;
    for (const ModelAsset& a : assets) {
        std::error_code ec;
        const std::uintmax_t size = fs::file_size(pron_asset_path(a, dir), ec);
        if (ec || static_cast<long long>(size) != a.size_bytes) {
            missing.push_back(a.key);
        }
    }
    return missing;
}

std::vector<std::string> missing_pron_assets(const std::string& dir) {
    return missing_assets(pron_model_assets(), dir);
}

FetchDecision plan_fetch(const ModelAsset& asset, const std::string& dir,
                         const AssetState& state) {
    FetchDecision d;
    d.part_path = pron_asset_part_path(asset, dir);
    d.final_path = pron_asset_path(asset, dir);
    if (state.final_exists && state.final_size == asset.size_bytes) {
        // 正式文件名只由 install_part 产生，而它落地前必过哈希
        d.plan = FetchPlan::kDoneVerified;
        d.reason = "文件已就位";
        return d;
    }
    if (state.part_bytes == asset.size_bytes && asset.size_bytes > 0) {
        d.plan = FetchPlan::kVerifyPart;
        d.reason = "断点文件已下完，只差校验";
        return d;
    }
    if (state.part_bytes > 0 && state.part_bytes < asset.size_bytes) {
        d.plan = FetchPlan::kResume;
        d.resume_from = state.part_bytes;
        d.reason = "断点续传（已有 " + std::to_string(state.part_bytes) +
                   " / " + std::to_string(asset.size_bytes) + " 字节）";
        return d;
    }
    d.plan = FetchPlan::kStartFresh;
    if (state.part_bytes > asset.size_bytes) {
        d.reason = "断点文件比资产还大，丢弃重来";
    } else if (state.final_exists) {
        d.reason = "已存在的文件尺寸不对，重新下载";
    } else {
        d.reason = "开始下载";
    }
    return d;
}

bool range_response_appends(bool requested_offset, long long http_status) {
    if (!requested_offset) {
        return true;  // 首次请求，没有可追加的东西
    }
    // 请求了偏移却拿到 200（而不是 206）= 服务器无视了 Range
    return http_status == 206;
}

VerifyReport verify_asset(const std::string& path, const ModelAsset& asset) {
    VerifyReport rep;
    std::error_code ec;
    const std::uintmax_t size = fs::file_size(path, ec);
    if (ec) {
        rep.status = AssetVerify::kUnreadable;
        rep.detail = "读不到文件: " + path;
        return rep;
    }
    if (static_cast<long long>(size) != asset.size_bytes) {
        rep.status = AssetVerify::kSizeMismatch;
        rep.detail = "尺寸不符：实际 " + std::to_string(size) + " 字节，期望 " +
                     std::to_string(asset.size_bytes) + " 字节";
        return rep;
    }
    std::string hex;
    std::string err;
    if (!sha256_file_hex(path, hex, err)) {
        rep.status = AssetVerify::kUnreadable;
        rep.detail = err;
        return rep;
    }
    if (hex != asset.sha256) {
        rep.status = AssetVerify::kHashMismatch;
        rep.detail = "哈希不符：算出 " + hex + "，期望 " + asset.sha256;
        return rep;
    }
    rep.status = AssetVerify::kOk;
    rep.detail = std::to_string(size) + " 字节，哈希一致";
    return rep;
}

InstallStatus install_part(const ModelAsset& asset, const std::string& dir,
                           std::string& err) {
    const std::string part = pron_asset_part_path(asset, dir);
    const VerifyReport rep = verify_asset(part, asset);
    if (rep.status != AssetVerify::kOk) {
        err = rep.detail;
        std::error_code ec;
        fs::remove(part, ec);  // 坏 part 留着只会让下次续传接在烂字节上
        return InstallStatus::kVerifyFailed;
    }
    const std::string final_path = pron_asset_path(asset, dir);
    std::error_code ec;
    fs::rename(part, final_path, ec);
    if (ec) {
        // part 本身是好的（磁盘满/权限/正式位置是目录）——留着让重试能落地
        err = "改名为 " + final_path + " 失败: " + ec.message();
        return InstallStatus::kIoFailed;
    }
    err.clear();
    return InstallStatus::kOk;
}

}  // namespace UnidictCoreStd
