// data_store_std 分支缺口补测（真实缺边 87 条，全库次大簇）。
//
// 缺边构成：现有测试从不在磁盘上重载 vocab/notes/pron_records 区段
// （add 之后直接走内存断言），整个持久化解析半边（find_section 字符串
// 感知深度计数、for_each_object 状态机、obj_val/obj_int/obj_num/
// obj_str_array 的容错守卫与数字解析臂）从未跑热；再加 API 守卫臂
// （上限 0、不存在词、空时间戳写省略、目录路径打开失败）。
// 本文件全部真实输入驱动：
//   R1 save→load 全区段往返（转义字符、小数词分、多记录逗号、空时间戳）
//   M1 手写畸形文件：非字符串元素、缺键/缺冒号/缺引号、负数与非数字、
//      未闭合对象（for_each_object 深度不归零跳过）
//   M2 区段非数组（对象/标量）与键后无冒号
//   A1 API 守卫：目录路径 load/save 失败、limit<=0、不存在词、
//      upsert 更新臂、CSV 引号转义与不可写路径
// 已知结构不可达（不硬凑，见 EXCL）：parse_json_string 的"未闭合串"
// 兜底——三处调用方都由字符串感知扫描器（find_section/for_each_object）
// 把关后才切入，传入串必然闭合。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#if !defined(_WIN32)
#include <unistd.h>  // ::geteuid；MSVC 不认这个头，走下方跳过分支
#endif

#include "std/data_store_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path store_path(const char* tag) {
    return fs::current_path() / "build-local" / ("dsbr_" + std::string(tag) + ".json");
}

static void write_file(const fs::path& p, const std::string& body) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
}

static std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// R1：全区段 save→load 往返。释义带引号/反斜杠/花括号/换行/制表符，
// 词分带小数，notes/pron 各两条（写逗号臂），两类空时间戳（写省略臂）
static void test_roundtrip_all_sections() {
    auto p = store_path("roundtrip");
    fs::remove(p);
    {
        DataStoreStd ds;
        ds.set_storage_path(p.string());

        VocabItemStd v1;
        v1.word = "alpha";
        v1.definition = "int main() { return \"x\"; } // \\done\ttab\nnext";
        v1.added_at = 1700000000;
        v1.tags = {"grep", "q\"t", "组"};
        ds.add_vocabulary_item(v1);
        VocabItemStd v2;
        v2.word = "beta";  // added_at=0：save 省略该字段，load 归 0
        v2.definition = "plain";
        ds.add_vocabulary_item(v2);

        ds.set_note("alpha", "first note");
        ds.set_note("beta", "second \"quoted\" note");

        PronRecordStd r1;
        r1.word = "hello";
        r1.last_score = 0.85;
        r1.best_score = 0.9;
        r1.attempts = 3;
        r1.last_at = 1700000100;
        ds.set_pron_record(r1);
        PronRecordStd r2;
        r2.word = "world";  // last_at=0：省略写
        r2.last_score = 0.25;
        r2.best_score = 0.5;
        r2.attempts = 1;
        ds.set_pron_record(r2);
        PronRecordStd r3;
        r3.word = "qu\"ote";  // 词内转义引号：pron 副本的状态机转义臂
        r3.last_score = 0.6;
        r3.best_score = 0.6;
        r3.attempts = 1;
        ds.set_pron_record(r3);

        ds.add_search_history("hello");
        ds.add_search_history("wor\nld");
    }
    {
        DataStoreStd ds;  // 全新实例：从磁盘重载
        ds.set_storage_path(p.string());
        assert(ds.load());

        auto h = ds.get_search_history(10);
        assert(h.size() == 2);
        assert(h[0] == "hello");
        assert(h[1] == "wor\nld");  // \n 转义往返（历史状态机曾解成字母 n）

        auto v = ds.get_vocabulary();
        assert(v.size() == 2);
        for (const auto& it : v) {
            if (it.word == "alpha") {
                assert(it.definition ==
                       "int main() { return \"x\"; } // \\done\ttab\nnext");
                assert(it.added_at == 1700000000);
                assert(it.tags.size() == 3);
                assert(it.tags[0] == "grep" && it.tags[1] == "q\"t" &&
                       it.tags[2] == "组");
            } else if (it.word == "beta") {
                assert(it.definition == "plain");
                assert(it.added_at > 0);  // add 时补的当前时间戳往返
                assert(it.tags.empty());  // 无 tags 字段读回空
            } else {
                assert(false);
            }
        }

        auto n = ds.get_notes();
        assert(n.size() == 2);

        auto rs = ds.get_pron_records();
        assert(rs.size() == 3);
        for (const auto& r : rs) {
            if (r.word == "hello") {
                assert(r.last_score > 0.849 && r.last_score < 0.851);
                assert(r.best_score > 0.899 && r.best_score < 0.901);
                assert(r.attempts == 3);
                assert(r.last_at == 1700000100);
            } else if (r.word == "world") {
                assert(r.last_score > 0.249 && r.last_score < 0.251);
                assert(r.last_at == 0);
            } else if (r.word == "qu\"ote") {
                assert(r.last_score > 0.599 && r.last_score < 0.601);
            } else {
                assert(false);
            }
        }
    }
    fs::remove(p);
}

// M1：手写畸形文件——容错解析器的各守卫臂与降级面
static void test_malformed_objects() {
    auto p = store_path("malformed");
    write_file(p, R"({
  "history": [a"b"],
  "vocab": [
    {"word":"w1","definition":"d1","added_at":-5},
    {"word":""},
    {"bad"},
    {"word" "x"},
    {"word": },
    {"word":"y","added_at":"abc"},
    {"word":"z","added_at":	9,"tags":[1,"t2",3]},
    {"word":"t9","tags":5},
    {]
  ],
  "notes": [
    {"word":"n1","text":"nt","updated_at":0},
    {"word":""},
    {]
  ],
  "pron_records": [
    {"word":"p1","last_score":0.25,"best_score":-1.5,"attempts":2,"last_at":0},
    {"word":"","last_score":	0.5},
    {"word":"p2","last_score":"bad"},
    {]
  ]
}
)");
    DataStoreStd ds;
    ds.set_storage_path(p.string());
    assert(ds.load());

    // history：非字符串元素 a 跳过，只收 "b"
    auto h = ds.get_search_history(10);
    assert(h.size() == 1 && h[0] == "b");

    // vocab：空词/缺键/缺冒号/缺引号/未闭合对象全部跳过
    auto v = ds.get_vocabulary();
    assert(v.size() == 4);
    for (const auto& it : v) {
        if (it.word == "w1") {
            assert(it.added_at == -5);  // 负数臂
            assert(it.tags.empty());
        } else if (it.word == "y") {
            assert(it.added_at == 0);  // 非数字值归 0
        } else if (it.word == "z") {
            assert(it.tags.size() == 1 && it.tags[0] == "t2");  // 数字元素跳过
        } else if (it.word == "t9") {
            assert(it.tags.empty());  // tags 有键无数组 → 空
        } else {
            assert(false);
        }
    }

    // notes：n1 入库，未闭合的 {] 对象（深度不归零）被跳过
    auto n = ds.get_notes();
    assert(n.size() == 1 && n[0].word == "n1" && n[0].updated_at == 0);

    // pron：p1 小数与负数词分、省略 last_at；p2 非数字词分归 0
    auto rs = ds.get_pron_records();
    assert(rs.size() == 2);
    for (const auto& r : rs) {
        if (r.word == "p1") {
            assert(r.last_score > 0.249 && r.last_score < 0.251);
            assert(r.best_score > -1.501 && r.best_score < -1.499);
            assert(r.attempts == 2 && r.last_at == 0);
        } else if (r.word == "p2") {
            assert(r.last_score == 0.0 && r.best_score == 0.0);
        } else {
            assert(false);
        }
    }

    // 带 0 时间戳的数据直接存盘：写省略臂（added_at/updated_at/last_at
    // == 0 的字段不落盘）
    assert(ds.save());
    fs::remove(p);
}

// M2：区段不是数组（对象/标量）与键后无冒号——各守卫直接跳过该区段
static void test_non_array_sections() {
    auto p = store_path("nonarray");
    write_file(p, R"({
  "history": {"k":1},
  "vocab": {},
  "notes": {},
  "pron_records": 5
}
)");
    DataStoreStd ds;
    ds.set_storage_path(p.string());
    assert(ds.load());
    assert(ds.get_search_history(10).empty());
    assert(ds.get_vocabulary().empty());   // 区段是对象不是数组 → 跳过
    assert(ds.get_notes().empty());
    assert(ds.get_pron_records().empty());
    fs::remove(p);

    // 键后无冒号
    auto p2 = store_path("nocolon");
    write_file(p2, "{\"history\" 5}\n");
    DataStoreStd ds2;
    ds2.set_storage_path(p2.string());
    assert(ds2.load());
    assert(ds2.get_search_history(10).empty());
    fs::remove(p2);
}

// M1b：花括号汤——字符串之外的花括号、对象型 pron_records 区段。
// 手改 store 文件是这类容错解析器的真实输入面（括号保持平衡：区段
// 深度计数对 [ ] 与 { } 一视同仁，未闭合方括号会吞掉区段边界——
// 那是留档的已知局限，不在本批断言面）
static void test_brace_soup() {
    auto p = store_path("bracesoup");
    write_file(p, R"({
  "history": ["h1"],
  "vocab": [
    {"word":"q","tags":["x"]},
    {"word":{x},"definition":"d"},
    {"word":"m","added_at":7}
  ],
  "notes": [
    {"word":"n2","text":{oops}}
  ],
  "pron_records": {
    "word":"p9"
  }
}
)");
    DataStoreStd ds;
    ds.set_storage_path(p.string());
    assert(ds.load());

    auto h = ds.get_search_history(10);
    assert(h.size() == 1 && h[0] == "h1");

    // vocab：裸 {x} 词位把 "definition" 的键名解析成了词——容错不崩溃
    auto v = ds.get_vocabulary();
    assert(v.size() == 3);
    bool saw_q = false, saw_def = false, saw_m = false;
    for (const auto& it : v) {
        if (it.word == "q") { saw_q = true; assert(it.tags.size() == 1); }
        else if (it.word == "definition") { saw_def = true; assert(it.definition == "d"); }
        else if (it.word == "m") { saw_m = true; assert(it.added_at == 7); }
    }
    assert(saw_q && saw_def && saw_m);

    // notes：n2 收下，text 位的裸花括号取不到引号 → 空 text
    auto n = ds.get_notes();
    assert(n.size() == 1 && n[0].word == "n2" && n[0].text.empty());

    // pron_records 是对象不是数组：区段守卫跳过
    assert(ds.get_pron_records().empty());
    fs::remove(p);
}

// A1：API 守卫臂
static void test_api_guards() {
    // 目录路径：save 的 ofstream 打不开目录返回假
    // （load 对目录在 Linux 上能拿到句柄但读不出内容，按空文件算成功，
    // 不在此断言；真正的"存在但打不开"见下方权限用例）
    auto dir = fs::current_path() / "build-local" / "dsbr_dir";
    fs::create_directories(dir);
    DataStoreStd ds;
    ds.set_storage_path(dir.string());
    assert(!ds.save());

#if !defined(_WIN32)
    // 文件存在但不可读：load 打开失败返回假。Windows 只读位不拦读，
    // 分支无法成立，跳过；root 下权限不生效，同样跳过。
    if (geteuid() != 0) {
        auto locked = store_path("locked");
        write_file(locked, "{}");
        fs::permissions(locked, fs::perms::owner_read | fs::perms::group_read |
                                    fs::perms::others_read,
                        fs::perm_options::remove);
        DataStoreStd dsl;
        dsl.set_storage_path(locked.string());
        assert(!dsl.load());
        fs::permissions(locked, fs::perms::owner_read | fs::perms::group_read |
                                    fs::perms::others_read,
                        fs::perm_options::add);
        fs::remove(locked);
    }
#endif

    auto p = store_path("api");
    fs::remove(p);
    DataStoreStd ds2;
    ds2.set_storage_path(p.string());

    // limit<=0 直接空表
    assert(ds2.get_search_history(0).empty());
    assert(ds2.get_search_history(-1).empty());

    // 历史去重大小写不敏感
    ds2.add_search_history("Hello");
    ds2.add_search_history("HELLO");
    assert(ds2.get_search_history(10).size() == 1);

    // 词表 upsert：更新臂保留原 added_at；新增臂 0 时间戳补当前时间
    VocabItemStd v1;
    v1.word = "Alpha";
    v1.definition = "one";
    v1.added_at = 77;
    ds2.add_vocabulary_item(v1);
    VocabItemStd v2;
    v2.word = "ALPHA";
    v2.definition = "two";
    ds2.add_vocabulary_item(v2);  // 命中已有词：只换释义
    auto v = ds2.get_vocabulary();
    assert(v.size() == 1 && v[0].definition == "two" && v[0].added_at == 77);
    VocabItemStd v3;
    v3.word = "beta";
    v3.definition = "b";
    ds2.add_vocabulary_item(v3);  // added_at==0 → 补时间戳
    v = ds2.get_vocabulary();
    assert(v.size() == 2);
    for (const auto& it : v) {
        if (it.word == "beta") assert(it.added_at > 0);
    }

    // 标签：命中换标签返回真，未命中返回假不动数据
    assert(ds2.set_vocabulary_item_tags("alpha", {"t1", "t2"}));
    assert(ds2.set_vocabulary_item_tags("nope", {"x"}) == false);
    v = ds2.get_vocabulary();
    for (const auto& it : v) {
        if (it.word == "alpha") assert(it.tags.size() == 2);
    }

    // 删除：不存在词全走尺寸不等早退，存在词真删
    ds2.remove_vocabulary_item("ghost");
    assert(ds2.get_vocabulary().size() == 2);
    ds2.remove_vocabulary_item("BETA");
    assert(ds2.get_vocabulary().size() == 1);

    // CSV：释义含引号时逐字符加倍；目标为目录时打不开返回假
    ds2.set_note("alpha", "csv note");
    auto csv = store_path("export.csv");
    assert(ds2.export_vocabulary_csv(csv.string()));
    {
        VocabItemStd q;
        q.word = "quo";
        q.definition = "say \"hi\"";
        ds2.add_vocabulary_item(q);
    }
    assert(ds2.export_vocabulary_csv(csv.string()));
    assert(read_file(csv).find("\"say \"\"hi\"\"\"") != std::string::npos);
    assert(!ds2.export_vocabulary_csv(dir.string()));

    // 笔记：新增/更新/空文本移除；查询命中/同长不同字符/长度不等
    ds2.set_note("alpha", "n1");
    ds2.set_note("alpha", "n2");   // 更新臂
    assert(ds2.get_note("ALPHA") == "n2");
    ds2.set_note("beta2", "m");    // 新增臂
    assert(ds2.get_note("beta2") == "m");
    assert(ds2.get_note("alpxz") == "");  // 同长不同字符 → same=false
    assert(ds2.get_note("zz") == "");     // 长度不等早退
    ds2.set_note("alpha", "");     // 空文本=移除
    assert(ds2.get_note("alpha") == "");

    // 发音记录：空词忽略；更新臂；查询三态；清空
    PronRecordStd r1;
    r1.word = "hi";
    r1.last_score = 0.5;
    ds2.set_pron_record(r1);
    PronRecordStd r1b;
    r1b.word = "HI";
    r1b.last_score = 0.7;
    r1b.attempts = 2;
    ds2.set_pron_record(r1b);  // upsert 更新
    auto rs = ds2.get_pron_records();
    assert(rs.size() == 1 && rs[0].attempts == 2 && rs[0].last_score > 0.69 &&
           rs[0].last_score < 0.71);
    PronRecordStd empty;
    ds2.set_pron_record(empty);  // 空词直接忽略
    assert(ds2.get_pron_records().size() == 1);
    PronRecordStd r2;
    r2.word = "other";
    ds2.set_pron_record(r2);
    assert(ds2.get_pron_record("OTHER").has_value());
    assert(!ds2.get_pron_record("hX").has_value());   // 同长不同字符
    assert(!ds2.get_pron_record("xyz").has_value());  // 长度不等
    ds2.clear_pron_records();
    assert(ds2.get_pron_records().empty());

    // 清空历史/词表
    ds2.clear_history();
    assert(ds2.get_search_history(10).empty());
    ds2.clear_vocabulary();
    assert(ds2.get_vocabulary().empty());

    fs::remove(p);
    fs::remove(csv);
    fs::remove_all(dir);
}

int main() {
    test_roundtrip_all_sections();
    test_malformed_objects();
    test_non_array_sections();
    test_brace_soup();
    test_api_guards();
    std::cout << "OK\n";
    return 0;
}
