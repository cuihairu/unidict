// B7 自救口纯 std 测试：口令加密备份导出/导入全链——全字段状态往返
// （含 dicts 与转义内容）、空态、空口令双向拒绝、错口令、密文/头部
// 篡改（AAD 绑定）、截断、坏 magic、坏轮数字段、随机 salt/nonce 异态
// 不同文、导出函数直验（serialize/parse 出匿名命名空间后的格式锚）。
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "crypto_std.h"
#include "sync_backup_std.h"

using namespace UnidictCoreStd;

namespace {

// 全字段样本态：words/notes/tags/history/prefs/dicts 全非空，含中文、
// emoji、引号反斜杠（转义链路全踩）
SyncVocabStateStd sample_state() {
    SyncVocabStateStd s;
    s.words = {"apple", "banana", "牛津", "😀moji"};
    s.notes["apple"] = "line1\nline2 \"q\" \\slash\\";
    s.notes["牛津"] = "Oxford";
    s.tags["apple"] = {"hard", "sat"};
    s.tags["banana"] = {"easy"};
    s.history = {{"apple", 42}, {"牛津", 7}};
    s.prefs["theme"] = "dark";
    s.prefs["k\"y"] = "v:a,l";
    s.dicts["lib-oxadv"] = "牛津高阶第9版";
    s.dicts["lib-jmdict"] = "JMdict \"多会话\" \\escaped\\ 😀";
    return s;
}

const uint32_t kFastIters = 1000;  // 测试加速；缺省轮数另有单测覆盖

void test_roundtrip_full_state() {
    const SyncVocabStateStd s = sample_state();
    const std::string blob =
        sync_backup_export(s, "口令 pass 🔑", kFastIters);
    assert(blob.size() > 56 + 16);

    SyncVocabStateStd back;
    std::string err;
    assert(sync_backup_import(blob, "口令 pass 🔑", back, &err));
    assert(err.empty());  // 成功不写 err（仓库口径）
    assert(back == s);
}

void test_roundtrip_empty_state() {
    const SyncVocabStateStd s;  // 全空态也合法（新装机自救兜底形态）
    const std::string blob = sync_backup_export(s, "p", kFastIters);
    SyncVocabStateStd back;
    std::string err;
    assert(sync_backup_import(blob, "p", back, &err));
    assert(back == s);
    assert(back.words.empty() && back.dicts.empty());
}

void test_default_iterations_shape() {
    // 缺省轮数：文件内 iters 字段如实落写（LE），可读回
    const std::string blob =
        sync_backup_export(sample_state(), "p", kBackupDefaultIterations);
    assert(blob.compare(0, 12, "UNIDICT-BK1\n") == 0);
    const uint32_t iters =
        static_cast<uint32_t>(static_cast<unsigned char>(blob[28])) |
        (static_cast<uint32_t>(static_cast<unsigned char>(blob[29])) << 8) |
        (static_cast<uint32_t>(static_cast<unsigned char>(blob[30])) << 16) |
        (static_cast<uint32_t>(static_cast<unsigned char>(blob[31])) << 24);
    assert(iters == kBackupDefaultIterations);
}

void test_export_rejects_empty_passphrase() {
    bool threw = false;
    try {
        sync_backup_export(sample_state(), "", kFastIters);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void test_import_failures() {
    const std::string blob = sync_backup_export(sample_state(), "right", kFastIters);
    SyncVocabStateStd back;
    std::string err;

    // 空口令双向拒绝
    assert(!sync_backup_import(blob, "", back, &err));
    assert(err == "passphrase required");

    // 错口令：AEAD 校验失败，与密文篡改同口径（不区分、少探测面）
    assert(!sync_backup_import(blob, "wrong", back, &err));
    assert(err == "wrong passphrase or corrupted backup");
    assert(back.words.empty());  // 失败不落地半态

    // 密文篡改
    std::string tam = blob;
    tam[tam.size() - 1] ^= 0x01;
    assert(!sync_backup_import(tam, "right", back, &err));
    assert(err == "wrong passphrase or corrupted backup");

    // 头部篡改（salt/nonce/iters 任一字节）——AAD 绑定收口
    for (const std::size_t off : {std::size_t{12}, std::size_t{28}, std::size_t{40}}) {
        std::string h = blob;
        h[off] ^= 0x01;
        assert(!sync_backup_import(h, "right", back, &err));
        assert(err == "wrong passphrase or corrupted backup");
    }

    // iters=0：非法轮数在解密前拒收（导出器永不写 0）
    std::string z = blob;
    z[28] = z[29] = z[30] = z[31] = 0;
    assert(!sync_backup_import(z, "right", back, &err));
    assert(err == "bad iterations field");

    // 截断 / 坏 magic
    assert(!sync_backup_import(blob.substr(0, 40), "right", back, &err));
    assert(err == "backup file too short");
    assert(!sync_backup_import("", "right", back, &err));
    assert(err == "backup file too short");
    std::string m = blob;
    m[0] = 'X';
    assert(!sync_backup_import(m, "right", back, &err));
    assert(err == "not a unidict backup file");
}

void test_same_input_different_bytes() {
    // salt/nonce 随机：同态两次导出字节不同，均能还原
    const SyncVocabStateStd s = sample_state();
    const std::string b1 = sync_backup_export(s, "p", kFastIters);
    const std::string b2 = sync_backup_export(s, "p", kFastIters);
    assert(b1 != b2);
    SyncVocabStateStd r1, r2;
    std::string err;
    assert(sync_backup_import(b1, "p", r1, &err));
    assert(sync_backup_import(b2, "p", r2, &err));
    assert(r1 == s && r2 == s);
}

void test_serialize_parse_exposed() {
    // 出匿名命名空间后的格式锚：函数可直接用，往返无损（快照/持久化
    // /备份三面同一格式源）
    const SyncVocabStateStd s = sample_state();
    const std::string json = serialize_state(s);
    assert(json.find("\"dicts\":{") != std::string::npos);
    assert(parse_state(json) == s);
    // 缺 dicts 区段（旧格式）→ 空清单
    const SyncVocabStateStd legacy =
        parse_state("{\"words\":[\"w\"],\"notes\":{},\"tags\":{},"
                    "\"history\":[],\"prefs\":{}}");
    assert(legacy.words.size() == 1 && legacy.dicts.empty());
}

}  // namespace

int main() {
    test_roundtrip_full_state();
    test_roundtrip_empty_state();
    test_default_iterations_shape();
    test_export_rejects_empty_passphrase();
    test_import_failures();
    test_same_input_different_bytes();
    test_serialize_parse_exposed();
    std::cout << "sync_backup_std_test: all assertions passed" << std::endl;
    return 0;
}
