// 在线发音源（纯逻辑层）：URL 拼装 + 响应解析 + 口音挑选。
// HTTP 拉取和音频播放都在平台壳（qmlui 的 QNetworkAccessManager +
// QMediaPlayer、Android 的 HttpURLConnection + MediaPlayer）——core
// 不引网络栈，本模块只有可单测的纯函数。
//
// 可扩展口径：新增在线源 = 实现 PronunciationSourceStd 三个纯函数，
// 在平台壳里按 name() 注册。core 不维护全局注册表（避免静态单例，
// 与 DataStore 以外的既有纪律一致）。
//
// 隐私口径：request_url 拼出的请求里唯一外发内容是查询词本身——
// 不带用户历史、生词本或任何设备信息。设置三态（本地/在线/自动）
// 与"默认关+明示"在平台壳落地，本模块只保证请求面最小。

#ifndef UNIDICT_ONLINE_PRON_STD_H
#define UNIDICT_ONLINE_PRON_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd {

// 口音标签。来自音频 URL 的文件名段（-us/-uk/-au/--_gb_ 等）或
// phonetics 条目的 text 标注；推断不出就是 Unknown（照样可播，
// 只是不参与口音偏好挑选的优先级）。
enum class PronAccent {
    Unknown,
    US,  // 美音
    UK,  // 英音（URL 里常见 _gb_/-gb 段）
    AU,  // 澳音
};

// 一条可播放的发音片段。
struct PronClip {
    std::string url;
    PronAccent accent = PronAccent::Unknown;
    std::string label;  // phonetics 里的 text 标注（如 "us"），可空
};

// 在线源接口。实现方只做两件事：把查询词拼成请求 URL、把响应体
// 解析成片段列表。解析必须容忍残缺输入（网络截断/字段缺失/结构
// 变体），坏数据返回空表，不抛异常。
class PronunciationSourceStd {
public:
    virtual ~PronunciationSourceStd() = default;

    // 源名（记录用哪家；平台壳状态行与日志直接展示）
    virtual const char* name() const = 0;
    // 查询词 → 请求 URL（查询词经 url_path_escape）
    virtual std::string request_url(const std::string& word) const = 0;
    // 响应体 → 片段列表（按 url 去重；空 audio 条目跳过）
    virtual std::vector<PronClip> parse_response(
        const std::string& body) const = 0;
};

// dictionaryapi.dev（Free Dictionary API）：免密钥、无配额、
// 响应就是公开词库的 JSON。当前默认在线源。
class FreeDictionarySource : public PronunciationSourceStd {
public:
    const char* name() const override;
    std::string request_url(const std::string& word) const override;
    std::vector<PronClip> parse_response(
        const std::string& body) const override;
};

// RFC 3986 unreserved 之外的字节全部百分号转义（空格→%20，UTF-8
// 逐字节保留后转义）。用于把查询词放进 URL path 段。
std::string url_path_escape(const std::string& raw);

// 容忍式解析：遍历 JSON 找带非空 "audio" 的对象，取同级 "text"
// 作标注，推断口音，按 url 去重。不要求文档整体合法——截断处
// 之前已完整读到的条目照常产出。malformed 输入不崩、最多返回空表。
std::vector<PronClip> parse_free_dictionary(const std::string& body);

// 从音频 URL 文件名段（-us/--_gb_/-uk/-au 等）推断口音，推断不出
// 返回 Unknown。
PronAccent accent_from_url(const std::string& url);

// 从 text 标注（"us"/"uk"/"au"/"american"/"british"/"australian"，
// 大小写不敏感）推断口音。
PronAccent accent_from_label(const std::string& text);

// 口音挑选：偏好口音命中优先，否则 US → UK → AU → Unknown → 首条。
// 空表返回 nullptr。
const PronClip* pick_clip(const std::vector<PronClip>& clips,
                          PronAccent prefer);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_ONLINE_PRON_STD_H
