#pragma once

// 发音模型资产清单 + 下载/校验决策（M10，纯逻辑）——把「首次使用引导
// 下载 + 哈希校验 + 断点续传 + 失败干净回退」这条要求（发音计划
// 「分层架构」与「已知风险」两处都写了，迟迟没做）从一句愿望变成可测
// 的规则。
//
// 为什么要现在做：评分功能的所有价值都挂在这一个 635MB 的 fp16 模型
// 上，用户得自己去 HuggingFace 页面手动下、放到 ~/.cache 下的约定
// 目录、还得自己确认没下坏——三步里任何一步错了，onnxruntime 只会
// 在几分钟后报一句看不懂的加载失败。
//
// 分层同 M3-M9：这里只回答"下一步该干什么"（下/续传/校验/落地），
// 真正的 HTTP 传输在平台壳（cli-std 的 curl 桥、gui 的 QNetwork），
// 界面与文案在 gui/pronunciation_panel。core 不引任何网络依赖。
#include <string>
#include <vector>

namespace UnidictCoreStd {

// 一个模型资产：单个要下载、下载完要按哈希校验的文件
struct ModelAsset {
    std::string key;        // 清单内标识（"model" / "vocab"）
    std::string filename;   // 落在模型目录里的文件名
    std::string url;        // 直链（只走 https：600MB 资产不裸奔）
    std::string sha256;     // 64 位小写 hex（上游 LFS pointer 记的值）
    long long size_bytes;   // 期望字节数（先比尺寸再算哈希）
    std::string note;       // 一句说明（状态栏/tooltip 用）
};

// 清单：sadda-speech/wav2vec2-espeak-ctc（Apache-2.0，见 docs
// pronunciation-plan M3b 选型记录）。哈希取自 HuggingFace 的 LFS
// pointer，与本仓库实际下载到的字节一致。
const std::vector<ModelAsset>& pron_model_assets();

// 按 key 找资产；没有返回 nullptr
const ModelAsset* find_pron_model_asset(const std::string& key);

// 模型目录的**单一真源**：GUI 与 CLI 过去各硬编码一份
// (~/.cache/unidict-models/wav2vec2-espeak-ctc)，布局一改要改两处，
// 漏一处就是"命令行能评、面板不能评"。env UNIDICT_PRON_MODEL_DIR 可覆盖
// （CI 与多模型并存的场景）。
std::string pron_model_dir();

// 资产总字节数（面板上"下载发音模型（606 MB）"那个数字）
long long pron_model_total_bytes();

// 缺哪些资产（空 = 齐了）。判据是"存在且尺寸对"而不是"重算一遍
// 哈希"：600MB 全量哈希每次开面板都跑一遍没道理，而真正的校验发生在
// install_part（只有校验过的 part 才会被改名成正式文件——正式文件名
// 出现在盘上，就意味着它过过哈希）。
std::vector<std::string> missing_assets(const std::vector<ModelAsset>& assets,
                                        const std::string& dir);
std::vector<std::string> missing_pron_assets(const std::string& dir);

// 资产正式文件 / 断点文件（下载途中）的路径
std::string pron_asset_path(const ModelAsset& asset, const std::string& dir);
std::string pron_asset_part_path(const ModelAsset& asset,
                                 const std::string& dir);

// 下载决策：给定盘上现状，下一步干什么
enum class FetchPlan {
    kDoneVerified,  // 正式文件已在位且尺寸对：什么都不用做
    kVerifyPart,    // 断点文件已是完整尺寸：只差校验与落地
    kResume,        // 断点半途：带 Range 从 resume_from 续传
    kStartFresh,    // 无断点/断点比资产还大/正式文件尺寸不对：重下
};

// 盘上现状（壳负责查，core 不碰文件系统语义之外的判断）
struct AssetState {
    bool final_exists = false;
    long long final_size = 0;
    long long part_bytes = 0;  // 断点文件当前字节数（不存在 = 0）
};

struct FetchDecision {
    FetchPlan plan = FetchPlan::kStartFresh;
    long long resume_from = 0;  // kResume 时的起始偏移
    std::string part_path;
    std::string final_path;
    std::string reason;  // 一句人话，状态栏直接用
};

FetchDecision plan_fetch(const ModelAsset& asset, const std::string& dir,
                         const AssetState& state);

// 服务器对 Range 的回应策略：请求带了偏移、服务器却回 200 全量（无视
// Range）时必须清空重下——把全量接在半截断点后面，就是 M3b 下载教训里
// 那个"尺寸对但字节坏"的坏包。返回 false = 截断重来。
// 206（正确回应）永远追加；无偏移的 200（首次请求）也是追加。
bool range_response_appends(bool requested_offset, long long http_status);

// 校验一个文件是否就是某个资产
enum class AssetVerify { kOk, kSizeMismatch, kHashMismatch, kUnreadable };

struct VerifyReport {
    AssetVerify status = AssetVerify::kUnreadable;
    std::string detail;  // 人话：实际多大 / 期望多大 / 哈希对不上
};

VerifyReport verify_asset(const std::string& path, const ModelAsset& asset);

// 断点文件 → 正式文件的落地。落地前**必须**校验：省得一个坏包占着正式
// 文件名，下次还"看起来齐了"。
// 校验不过 = part 是垃圾，删掉（留着会让下一次续传永远接在烂字节上，
// 壳收到 kVerifyFailed 后应当重下一次）；改名失败 = part 本身是好的
// （磁盘满/权限），**留着** part 好让重试直接落地。
enum class InstallStatus { kOk, kVerifyFailed, kIoFailed };

InstallStatus install_part(const ModelAsset& asset, const std::string& dir,
                           std::string& err);

}  // namespace UnidictCoreStd
