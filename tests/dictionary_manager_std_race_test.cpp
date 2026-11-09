// 并发首查竞态回归（深审 P2）：前缀 trie 兜底自愈（load_index 置脏后
// const_cast + build_index）与全文索引懒构建（const_cast 写 ft_index_）
// 都发生在 const 查询路径上——并发首查同时进同一懒构建就是数据竞争
// （trie 构建并发写、unique_ptr 并发赋值）。修法：各自互斥护构建、
// 脏标记原子化。TSAN 下未修必红；修复后每线程结果须与单线程基线一致。
#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

static std::filesystem::path write_json(const std::string& name, const std::string& dict_name,
                                        int words) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr_race";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream o(p, std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"race\",\"entries\":[";
    for (int i = 0; i < words; ++i) {
        if (i) o << ",";
        o << "{\"word\":\"word" << i << "\",\"definition\":\"def of word" << i << "\"}";
    }
    o << "]}";
    return p;
}

int main() {
    namespace fs = std::filesystem;
    fs::path idx = fs::current_path() / "build-local" / "dict_mgr_race" / "race.index";

    // 基线实例：建好索引落盘，供 load_index 整表替换（置脏前缀 trie）
    auto d = write_json("race.json", "RaceDict", 2000);
    {
        DictionaryManagerStd m0;
        assert(m0.add_dictionary(d.string()));
        m0.build_index();
        assert(m0.save_index(idx.string()));
    }

    // 同一 manager 两种懒构建并发首查：前缀（dirty 自愈）+ 全文（惰建）。
    // 词典本体必须装载（全文释义取自 dicts_），load_index 只换词表索引
    DictionaryManagerStd m;
    assert(m.add_dictionary(d.string()));
    assert(m.load_index(idx.string()));
    assert(!m.index_ready());  // 前缀 trie 置脏，首查必走自愈构建

    // 单线程基线（另开干净实例同口径，先各走一次懒构建预热）
    DictionaryManagerStd base;
    assert(base.add_dictionary(d.string()));
    assert(base.load_index(idx.string()));
    const auto warmed = base.prefix_search("zzz_no_such_word", 5);
    (void)warmed;  // 自愈构建入口
    base.warm_fulltext_index();
    const auto base_prefix = base.prefix_search("word1", 20);
    const auto base_ft = base.full_text_search("def of word2", 20, 0);
    assert(!base_prefix.empty());
    assert(!base_ft.empty());

    volatile bool bad = false;
    const int nthreads = 8;
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> pool;
    for (int t = 0; t < nthreads; ++t) {
        pool.emplace_back([&]() {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (!go.load(std::memory_order_relaxed))
                std::this_thread::yield();
            auto pref = m.prefix_search("word1", 20);
            if (pref != base_prefix) { bad = true; return; }
            auto ft = m.full_text_search("def of word2", 20, 0);
            if (ft.size() != base_ft.size()) { bad = true; return; }
            for (size_t i = 0; i < ft.size(); ++i)
                if (ft[i].word != base_ft[i].word) { bad = true; return; }
        });
    }
    while (ready.load() < nthreads) std::this_thread::yield();
    go.store(true, std::memory_order_relaxed);
    for (auto& th : pool) th.join();
    assert(!bad);
    std::cout << "OK\n";
    return 0;
}
