// 模型资产清单 + 下载/校验/落地决策的纯 std 测试：清单形状（https +
// 64 位哈希 + 尺寸）、模型目录单一真源与 env 覆盖、五条下载计划、
// Range 回包策略、四种校验结果、三种落地结果。资产本身是合成的几十
// 字节（真资产 635MB，不该进测试）。
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>  // ::setenv；MSVC 走上方 _putenv_s 分支，不认这个头
#endif

#include "std/pron_model_source_std.h"
#include "std/sha256_std.h"

using namespace UnidictCoreStd;

namespace {

namespace fs = std::filesystem;

// env 读写（Windows/MSVC 与 POSIX 两套，CI 三平台都跑）
void set_env(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

void unset_env(const char* key) { set_env(key, std::string()); }

// 合成资产：内容短小但规则与真资产同构（尺寸 + 哈希都真算）
ModelAsset make_asset(const std::string& key, const std::string& body) {
    ModelAsset a;
    a.key = key;
    a.filename = key + ".bin";
    a.url = "https://example.invalid/assets/" + key + ".bin";
    a.sha256 = sha256_hex(body);
    a.size_bytes = static_cast<long long>(body.size());
    a.note = "synthetic asset for tests";
    return a;
}

std::string temp_dir(const std::string& tag) {
    const fs::path dir = fs::temp_directory_path() / ("pron_model_source_std_" + tag);
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir.string();
}

void write_file(const std::string& path, const std::string& body) {
    std::ofstream out(path, std::ios::binary);
    out << body;
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// 清单本身：手抄错哈希/漏 https 是这类表最常见的事故，全部钉死
void test_manifest() {
    const std::vector<ModelAsset>& assets = pron_model_assets();
    assert(assets.size() == 2);
    long long total = 0;
    for (const ModelAsset& a : assets) {
        assert(!a.key.empty() && !a.filename.empty() && !a.note.empty());
        assert(a.url.rfind("https://", 0) == 0);  // 600MB 资产不走明文
        assert(is_sha256_hex(a.sha256));
        assert(a.size_bytes > 0);
        total += a.size_bytes;
    }
    assert(pron_model_total_bytes() == total);
    // 真资产的形状钉死：改了清单就是有意改动，得在 PR 里说清楚
    const ModelAsset* model = find_pron_model_asset("model");
    const ModelAsset* vocab = find_pron_model_asset("vocab");
    assert(model != nullptr && vocab != nullptr);
    assert(model->filename == "model.onnx" && model->size_bytes == 635089013);
    assert(model->sha256 ==
           "35c2f8484b74737c7816e8762c095d6f7dc40ded7c36d249c8134d8f30d717b4");
    assert(vocab->filename == "vocab.json" && vocab->size_bytes == 4637);
    assert(vocab->sha256 ==
           "d732ab2456c0c017930001dc9af0b41b3b93d25b2eb9740bf9d925508d7d87d0");
    assert(find_pron_model_asset("nope") == nullptr);
    assert(find_pron_model_asset("") == nullptr);
}

// 模型目录：单一真源（GUI/CLI 过去各硬编码一份）。env 覆盖 + 尾斜杠
// 拼接 + 连 home 都拿不到时的相对回退
void test_model_dir() {
    const char* keys[] = {"UNIDICT_PRON_MODEL_DIR", "HOME", "USERPROFILE"};
    std::vector<std::string> saved;
    for (const char* k : keys) {
        const char* v = std::getenv(k);
        saved.push_back(v ? std::string(v) : std::string());
    }
    const auto restore = [&]() {
        for (size_t i = 0; i < 3; ++i) {
            if (saved[i].empty()) {
                unset_env(keys[i]);
            } else {
                set_env(keys[i], saved[i]);
            }
        }
    };

    setenv("UNIDICT_PRON_MODEL_DIR", "/tmp/custom-model", 1);
    assert(pron_model_dir() == "/tmp/custom-model");
    // 空串视作未设（与仓库既有 env 约定一致）
    setenv("UNIDICT_PRON_MODEL_DIR", "", 1);
    unset_env("UNIDICT_PRON_MODEL_DIR");
    set_env("HOME", "/home/tester");
    assert(pron_model_dir() == "/home/tester/.cache/unidict-models/wav2vec2-espeak-ctc");
    // Windows 上 HOME 通常不设，USERPROFILE 顶上
    unset_env("HOME");
    set_env("USERPROFILE", "C:/Users/tester");
    assert(pron_model_dir() == "C:/Users/tester/.cache/unidict-models/wav2vec2-espeak-ctc");
    // 两个都没有：退到当前目录下的相对路径（受限环境），仍是可用路径
    unset_env("USERPROFILE");
    assert(pron_model_dir() == ".cache/unidict-models/wav2vec2-espeak-ctc");
    restore();

    // 路径拼接：尾斜杠不重复加，空目录名退化为裸文件名
    const ModelAsset a = make_asset("k", "x");
    assert(pron_asset_path(a, "/tmp/d") == "/tmp/d/k.bin");
    assert(pron_asset_path(a, "/tmp/d/") == "/tmp/d/k.bin");
    assert(pron_asset_part_path(a, "/tmp/d") == "/tmp/d/k.bin.part");
    assert(pron_asset_path(a, "") == "k.bin");
}

// 下载计划：齐了/下完只差校验/续传/重来/断点过大/正式文件尺寸不对
void test_plan_fetch() {
    const ModelAsset a = make_asset("model", std::string(1000, 'z'));
    const std::string dir = temp_dir("plan");
    const long long full = a.size_bytes;

    AssetState s;
    FetchDecision d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kStartFresh);
    assert(d.resume_from == 0);
    assert(d.part_path == dir + "/model.bin.part");
    assert(d.final_path == dir + "/model.bin");
    assert(!d.reason.empty());

    s.part_bytes = 400;  // 下到一半
    d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kResume);
    assert(d.resume_from == 400);

    s.part_bytes = full;  // 下完了，只差校验
    d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kVerifyPart);

    s.part_bytes = full + 1;  // 比资产还大：烂断点，丢弃重来
    s.final_exists = false;
    d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kStartFresh);
    assert(d.reason.find("丢弃") != std::string::npos);

    s.part_bytes = 0;
    s.final_exists = true;
    s.final_size = full - 1;  // 正式文件在但尺寸不对
    d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kStartFresh);
    assert(d.reason.find("尺寸") != std::string::npos);

    s.final_size = full;  // 正式文件齐了（只由 install_part 产生）
    d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kDoneVerified);
    assert(d.resume_from == 0);

    // 半截断点 + 正式文件齐了：齐了就是齐了，不该再去续传
    s.part_bytes = 300;
    d = plan_fetch(a, dir, s);
    assert(d.plan == FetchPlan::kDoneVerified);

    fs::remove_all(dir);
}

// Range 回包：服务器无视 Range（回 200 全量）时必须截断重下——把全量
// 接在半截断点后面，就是"尺寸对但字节坏"的坏包（M3b 下载教训）
void test_range_response() {
    assert(range_response_appends(false, 200));  // 首次请求
    assert(range_response_appends(false, 206));
    assert(range_response_appends(true, 206));   // 正确回应
    assert(!range_response_appends(true, 200));  // 无视了 Range
    assert(!range_response_appends(true, 416));  // 越界（断点比文件还长）
    assert(!range_response_appends(true, 500));
}

// 校验：先比尺寸（省一次 600MB 读盘）再比哈希
void test_verify_asset() {
    const std::string body(4096, 'q');
    const ModelAsset a = make_asset("model", body);
    const std::string dir = temp_dir("verify");
    const std::string path = dir + "/model.bin";

    VerifyReport rep = verify_asset(path, a);
    assert(rep.status == AssetVerify::kUnreadable);  // 文件不在
    assert(!rep.detail.empty());

    write_file(path, body);
    rep = verify_asset(path, a);
    assert(rep.status == AssetVerify::kOk);
    assert(rep.detail.find("一致") != std::string::npos);

    // 尺寸对、字节错：哈希拦下（这正是坏包的形态）
    write_file(path, body.substr(1) + "Q");
    rep = verify_asset(path, a);
    assert(rep.status == AssetVerify::kHashMismatch);
    assert(rep.detail.find("哈希不符") != std::string::npos);

    // 尺寸就不对：短读报告，不浪费算哈希
    write_file(path, body.substr(0, 100));
    rep = verify_asset(path, a);
    assert(rep.status == AssetVerify::kSizeMismatch);
    assert(rep.detail.find("尺寸不符") != std::string::npos);

    // 同尺寸但不可读文件：file_size 通过，sha256_file_hex 失败
    // （覆盖 verify_asset 里的 sha256_file_hex 失败分支：kUnreadable，
    // detail 是"cannot open"而非"打不开"）。需要非 root 用户运行才会真正不可读；
    // root 下会读成功，这里只在非 root 时断言该分支，root 时跳过。
    bool is_root = (geteuid() == 0);
    if (!is_root) {
        write_file(path, body);  // 正确尺寸
        const fs::path p = path;
        fs::permissions(p, fs::perms::owner_read | fs::perms::group_read |
                                    fs::perms::others_read,
                        fs::perm_options::remove);  // 去掉所有读权限
        VerifyReport rep3 = verify_asset(path, a);
        assert(rep3.status == AssetVerify::kUnreadable);
        assert(rep3.detail.find("cannot open") != std::string::npos);
        // 恢复权限以便清理
        fs::permissions(p, fs::perms::owner_read | fs::perms::group_read |
                                    fs::perms::others_read,
                        fs::perm_options::add);
    }

    fs::remove_all(dir);
}

// 落地：校验过的 part 才改名成正式文件；坏 part 删掉（留着会让下次
// 续传永远接在烂字节上），改名失败则保留 part（它本身是好的）
void test_install_part() {
    const std::string body(2048, 'm');
    const ModelAsset a = make_asset("model", body);
    const std::string dir = temp_dir("install");
    const std::string part = dir + "/model.bin.part";
    const std::string final_path = dir + "/model.bin";

    std::string err = "x";
    // 没有 part：校验读不到 → kVerifyFailed（err 覆盖旧的）
    assert(install_part(a, dir, err) == InstallStatus::kVerifyFailed);
    assert(!err.empty());
    assert(!file_exists(final_path));

    // 坏 part：落地被拒 + part 被清掉
    write_file(part, std::string(2048, 'X'));
    assert(install_part(a, dir, err) == InstallStatus::kVerifyFailed);
    assert(!file_exists(part));
    assert(!file_exists(final_path));

    // 好 part：改名落地，part 消失
    write_file(part, body);
    assert(install_part(a, dir, err) == InstallStatus::kOk);
    assert(err.empty());
    assert(file_exists(final_path));
    assert(!file_exists(part));
    assert(verify_asset(final_path, a).status == AssetVerify::kOk);

    // 改名失败（正式位置是个目录）：part 保留，好让重试能落地
    const fs::path dir2 = fs::path(temp_dir("install2"));
    fs::create_directories(dir2 / "model.bin");
    const std::string part2 = (dir2 / "model.bin.part").string();
    write_file(part2, body);
    assert(install_part(a, dir2.string(), err) == InstallStatus::kIoFailed);
    assert(!err.empty());
    assert(file_exists(part2));

    fs::remove_all(dir);
    fs::remove_all(dir2);
}

// 缺哪些资产：面板据此决定要不要给"下载模型"按钮。判据只看存在+尺寸
void test_missing_assets() {
    const std::vector<ModelAsset> assets = {make_asset("a", "aaa"),
                                            make_asset("b", "bbbb")};
    const std::string dir = temp_dir("missing");
    assert(missing_assets(assets, dir).size() == 2);

    write_file(dir + "/a.bin", "aaa");
    std::vector<std::string> missing = missing_assets(assets, dir);
    assert(missing.size() == 1 && missing[0] == "b");

    write_file(dir + "/b.bin", "bbb");   // 尺寸不对（4 期望 5 字节）
    missing = missing_assets(assets, dir);
    assert(missing.size() == 1 && missing[0] == "b");
    write_file(dir + "/b.bin", "bbbb");
    assert(missing_assets(assets, dir).empty());

    // 真清单在空目录上：两个都缺（600MB 资产不进测试）
    assert(missing_pron_assets(dir).size() == pron_model_assets().size());
    assert(missing_pron_assets(temp_dir("missing2")).size() == 2);

    fs::remove_all(dir);
}

}  // namespace

int main() {
    test_manifest();
    test_model_dir();
    test_plan_fetch();
    test_range_response();
    test_verify_asset();
    test_install_part();
    test_missing_assets();
    std::cout << "pron_model_source_std_test: all assertions passed\n";
    return 0;
}
