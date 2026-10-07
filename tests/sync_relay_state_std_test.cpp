// 同步中转协议 v1 契约语义层测试（PROTOCOL.md；server_plan §7 B5）。
//
// 与 dev 参考实现的契约符合性套件（test_relay_protocol.py，真实 HTTP 往返）
// 同口径：这里在结构级把 16 用例语义钉死（定序/幂等/位点/快照/限额），
// 外加持久化边界（坏文件空起/原子替换失败）。传输壳另有端到端对拍。
#include <cassert>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "std/sync_relay_state_std.h"

using namespace UnidictCoreStd;

namespace {

namespace fs = std::filesystem;

const std::string kGid = "A0123456789abcdefghij";   // 21 字符，^[\w-]{16,64}$
const std::string kGid2 = "B0123456789abcdefghij";

// payload 对中转不透明：base64 形态即可，内容随意
std::string b64i(int i) { return "QUJDREVG" + std::to_string(i); }

template <typename Fn>
void must_err(int status, const std::string& code, Fn&& fn) {
    try {
        fn();
    } catch (const SyncRelayErrorStd& e) {
        assert(e.status() == status);
        assert(e.code() == code);
        return;
    }
    assert(false && "expected SyncRelayErrorStd");
}

// 临时数据目录：RAII 清理（每用例独立，避免串扰）
struct TempDir {
    fs::path path;
    explicit TempDir(const char* tag) {
        static int seq = 0;  // 同进程内防撞；跨进程靠时钟区分
        path = fs::temp_directory_path() /
               ("unidict_relay_test_" + std::string(tag) + "_" +
                std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) +
                "_" + std::to_string(seq++));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

}  // namespace

int main() {
    // ---- 探活字段常量 ----
    assert(std::string(kRelayServiceName) == "unidict-sync-relay");
    assert(kRelayProtocolVersion == 1);

    SyncRelayStateStd st;  // 纯内存形态

    // ---- 建组幂等 / 组不存在 404 ----
    {
        SyncRelayMeta m1 = st.create_group(kGid);
        assert(m1.gid == kGid && m1.latest_seq == 0 && m1.op_count == 0);
        assert(m1.snapshot_up_to_seq == 0);
        SyncRelayMeta m2 = st.create_group(kGid);  // 幂等：原样成功
        assert(m1.created_at == m2.created_at && m2.latest_seq == 0);

        for (const std::string& bad : {"short",                                        // 5 字符
                                       "0123456789abcde",                              // 15 字符（低于下界）
                                       "G0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",  // 65 字符（高于上界）
                                       "G0123456789abcdef!",                           // 非法字符
                                       "",                                             // 空
                                       "..%2F0123456789abcdef"}) {                     // 转义形态
            must_err(400, "invalid_group_id", [&] { (void)st.create_group(bad); });
        }
        must_err(404, "group_not_found", [&] { (void)st.group_meta(kGid2); });
        must_err(404, "group_not_found",
                 [&] { (void)st.pull_ops(kGid2, 0, 200); });
        must_err(404, "group_not_found",
                 [&] { (void)st.get_snapshot(kGid2); });
        must_err(404, "group_not_found",
                 [&] { (void)st.put_snapshot(kGid2, 1, b64i(1)); });
    }

    // ---- 追加：自动建组 + 服务端定序全序无重号 ----
    {
        SyncRelayAppendResult r = st.append_ops(kGid, {{"devA:1", "devA", b64i(1)},
                                                       {"devB:1", "devB", b64i(2)}});
        assert(r.assigned.size() == 2);
        assert(r.assigned[0].seq == 1 && r.assigned[0].op_id == "devA:1");
        assert(r.assigned[1].seq == 2 && r.assigned[1].op_id == "devB:1");
        assert(r.duplicate_op_ids.empty());
        SyncRelayMeta m = st.group_meta(kGid);
        assert(m.latest_seq == 2 && m.op_count == 2);

        // 首发新 op 分配下一号，再原样重发（网络重试）→ 不追加不计新 seq
        SyncRelayOpIn op2{"devA:2", "devA", b64i(3)};
        SyncRelayAppendResult first = st.append_ops(kGid, {op2});
        assert(first.assigned.size() == 1 && first.assigned[0].seq == 3);
        SyncRelayAppendResult d = st.append_ops(kGid, {op2});
        assert(d.assigned.empty());
        assert(d.duplicate_op_ids.size() == 1 && d.duplicate_op_ids[0] == "devA:2");
        // 新旧混合：只有新的分配 seq
        SyncRelayAppendResult mix = st.append_ops(
            kGid, {op2, {"devA:3", "devA", b64i(4)}});
        assert(mix.assigned.size() == 1 && mix.assigned[0].seq == 4);
        assert(mix.duplicate_op_ids.size() == 1);
        SyncRelayMeta m2 = st.group_meta(kGid);
        assert(m2.latest_seq == 4 && m2.op_count == 4);

        // 同请求内重复 op_id → 400（客户端 bug 面）
        must_err(400, "duplicate_op_id_in_request", [&] {
            (void)st.append_ops(kGid, {{"devX:1", "devX", b64i(1)},
                                       {"devX:1", "devX", b64i(1)}});
        });

        // 非法 op 矩阵（整批原子：校验在应用前）
        const std::vector<SyncRelayOpIn> bads = {
            {"d:1", "d", ""},            // 缺 payload
            {"d:2", "", b64i(1)},        // 缺 device_id
            {"", "d", b64i(1)},          // 缺 op_id
            {"d:3", "d", "!!"},          // 非 base64
            {"d:4", "d", "ab&cd"},       // 非 b64 字符
            {std::string(201, 'o'), "d", b64i(1)},  // op_id 超 200
            {"d:5", std::string(129, 'd'), b64i(1)}, // device_id 超 128
        };
        for (const SyncRelayOpIn& bad : bads)
            must_err(400, "invalid_op", [&] { (void)st.append_ops(kGid, {bad}); });
        must_err(400, "invalid_op", [&] { (void)st.append_ops(kGid, {}); });

        std::vector<SyncRelayOpIn> many;
        for (int i = 0; i < 257; ++i)
            many.push_back({"m" + std::to_string(i), "m", b64i(i)});
        must_err(400, "too_many_ops", [&] { (void)st.append_ops(kGid, many); });

        // payload 尺寸：≤256KiB 收，>256KiB 413
        must_err(413, "payload_too_large", [&] {
            (void)st.append_ops(kGid, {{"big:1", "big", std::string(256 * 1024 + 1, 'A')}});
        });
        SyncRelayAppendResult okbig = st.append_ops(
            kGid, {{"big:2", "big", std::string(256 * 1024, 'A')}});
        assert(okbig.assigned.size() == 1);
    }

    // ---- 拉取：位点语义 / 分页 / 钳制 / 空组 ----
    {
        const std::string gid = "C0123456789abcdefghij";
        std::vector<SyncRelayOpIn> ops;
        for (int i = 1; i <= 10; ++i)
            ops.push_back({"devP:" + std::to_string(i), "devP", b64i(i)});
        (void)st.append_ops(gid, ops);

        SyncRelayPull r = st.pull_ops(gid, 0, 200);
        assert(r.ops.size() == 10);
        for (std::size_t i = 0; i < 10; ++i) assert(r.ops[i].seq == (std::int64_t)i + 1);
        assert(r.cursor == 10 && !r.has_more);
        // since=N 严格返回 seq>N
        SyncRelayPull r7 = st.pull_ops(gid, 7, 200);
        assert(r7.ops.size() == 3 && r7.ops[0].seq == 8);
        assert(r7.ops[0].op_id == "devP:8" && r7.ops[2].op_id == "devP:10");
        // 分页：limit 截断 + has_more 续拉
        SyncRelayPull p1 = st.pull_ops(gid, 0, 4);
        assert(p1.ops.size() == 4 && p1.has_more);
        SyncRelayPull p2 = st.pull_ops(gid, 4, 4);
        assert(p2.ops.size() == 4 && p2.ops[0].seq == 5 && p2.has_more);
        SyncRelayPull p3 = st.pull_ops(gid, 8, 4);
        assert(p3.ops.size() == 2 && !p3.has_more);
        // limit 上限钳制、下限钳制、负位点容错、越过尾部空返回
        assert(st.pull_ops(gid, 0, 99999).ops.size() == 10);
        assert(st.pull_ops(gid, 0, 0).ops.size() == 1);    // limit<1 钳到 1
        assert(st.pull_ops(gid, -5, 200).ops.size() == 10);
        assert(st.pull_ops(gid, 99, 200).ops.empty());
        assert(st.pull_ops(gid, 99, 200).cursor == 10);
        // 空组：存在但空（区别于 404）
        const std::string empty_gid = "D0123456789abcdefghij";
        (void)st.create_group(empty_gid);
        SyncRelayPull e = st.pull_ops(empty_gid, 0, 200);
        assert(e.ops.empty() && e.cursor == 0 && !e.has_more);
    }

    // ---- 快照：校验 / 往返 / 最后写入者胜 / 404 ----
    {
        const std::string gid = "E0123456789abcdefghij";
        std::vector<SyncRelayOpIn> ops;
        for (int i = 1; i <= 5; ++i)
            ops.push_back({"devS:" + std::to_string(i), "devS", b64i(i)});
        (void)st.append_ops(gid, ops);
        // 不能快照未来
        must_err(400, "invalid_snapshot",
                 [&] { (void)st.put_snapshot(gid, 6, b64i(0)); });
        must_err(400, "invalid_snapshot",
                 [&] { (void)st.put_snapshot(gid, 0, b64i(0)); });
        assert(st.put_snapshot(gid, 3, b64i(9)) == 3);
        SyncRelayMeta m = st.group_meta(gid);
        assert(m.snapshot_up_to_seq == 3);
        SyncRelaySnapshot s = st.get_snapshot(gid);
        assert(s.up_to_seq == 3 && s.payload == b64i(9));
        // 最后写入者胜（整体替换）
        (void)st.put_snapshot(gid, 5, b64i(8));
        SyncRelaySnapshot s2 = st.get_snapshot(gid);
        assert(s2.up_to_seq == 5 && s2.payload == b64i(8));
        // 快照 payload 形态校验（按 invalid_op / 413）
        must_err(400, "invalid_op", [&] { (void)st.put_snapshot(gid, 4, "!!"); });
        must_err(413, "payload_too_large", [&] {
            (void)st.put_snapshot(gid, 4, std::string(256 * 1024 + 1, 'A'));
        });
        // 无快照组 → 404 snapshot_not_found
        const std::string no_snap = "K0123456789abcdefghij";
        (void)st.create_group(no_snap);
        must_err(404, "snapshot_not_found", [&] { (void)st.get_snapshot(no_snap); });
        // §2.8 空洞拉取数据面：位点 0 < 快照覆盖位 → 客户端先快照后增量
        SyncRelayPull after = st.pull_ops(gid, 3, 200);
        assert(after.ops.size() == 2 && after.ops[0].seq == 4);
    }

    // ---- 并发追加：8 线程 × 25 条，seq 严格 1..200 无重号 ----
    {
        const std::string gid = "G0123456789abcdefghij";
        std::vector<std::thread> threads;
        std::vector<std::vector<std::int64_t>> got(8);
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([&, t] {
                std::vector<SyncRelayOpIn> batch;
                for (int i = 0; i < 25; ++i)
                    batch.push_back({"dev" + std::to_string(t) + ":" + std::to_string(i),
                                     "dev" + std::to_string(t), b64i(i)});
                SyncRelayAppendResult r = st.append_ops(gid, batch);
                for (const SyncRelayAssign& a : r.assigned) got[t].push_back(a.seq);
            });
        }
        for (std::thread& th : threads) th.join();
        std::vector<std::int64_t> all;
        for (auto& v : got) all.insert(all.end(), v.begin(), v.end());
        assert(all.size() == 200);
        std::sort(all.begin(), all.end());
        for (std::size_t i = 0; i < all.size(); ++i) assert(all[i] == (std::int64_t)i + 1);
        SyncRelayPull r = st.pull_ops(gid, 0, 1000);
        assert(r.ops.size() == 200);
        std::map<std::string, bool> ids;
        for (const SyncRelayOp& op : r.ops) {
            assert(!ids.count(op.op_id));  // 无重复指令
            ids[op.op_id] = true;
            assert(op.seq > 0);
        }
        assert(r.ops.back().seq == 200);  // 升序全序
    }

    // ---- 持久化：跨实例续存 / 坏文件空起 / 原子替换失败 ----
    {
        TempDir td("state");
        const std::string dir = td.path.string();
        const std::string gid = "H0123456789abcdefghij";
        {
            SyncRelayStateStd s1(dir);
            (void)s1.append_ops(gid, {{"devZ:1", "devZ", b64i(5)}});
            (void)s1.put_snapshot(gid, 1, b64i(7));
        }
        {
            SyncRelayStateStd s2(dir);  // 重启续存
            SyncRelayMeta m = s2.group_meta(gid);
            assert(m.latest_seq == 1 && m.op_count == 1);
            SyncRelayPull p = s2.pull_ops(gid, 0, 200);
            assert(p.ops.size() == 1);
            assert(p.ops[0].op_id == "devZ:1" && p.ops[0].device_id == "devZ");
            assert(p.ops[0].payload == b64i(5));
            SyncRelaySnapshot sn = s2.get_snapshot(gid);
            assert(sn.up_to_seq == 1 && sn.payload == b64i(7));
            // 幂等键在重启后仍生效（已存 op_id 视为重复）
            SyncRelayAppendResult d = s2.append_ops(gid, {{"devZ:1", "devZ", b64i(5)}});
            assert(d.assigned.empty() && d.duplicate_op_ids.size() == 1);
        }
        // 坏文件：非本格式 / 版本不符 / 行损坏 → 整体空起
        const std::vector<std::string> corrupt_files = {
            std::string("garbage not a state file\n"),
            std::string("unidict-relay-state 99\n"),
            "unidict-relay-state 1\nG " + kGid + " xx 0 - - -\n",
            "unidict-relay-state 1\nO " + kGid + " 1 0 zz zz AAAA\n",
            // G 在但 O 行 hex 损坏
            "unidict-relay-state 1\nG " + kGid + " 5 0 - - -\nO " + kGid +
                " 1 0 zz zz AAAA\n",
            // 快照字段坏（up_to_seq 非整数）
            "unidict-relay-state 1\nG " + kGid + " 5 10 xx 0 AAAA\n",
            // 未知记录类型
            "unidict-relay-state 1\nX junk line here\n",
        };
        for (const std::string& content : corrupt_files) {
            TempDir tdb("corrupt");
            {
                std::ofstream f(tdb.path / "relay_state.json");
                f << content;
            }
            SyncRelayStateStd s3(tdb.path.string());  // 按空起，不抛
            must_err(404, "group_not_found", [&] { (void)s3.group_meta(kGid); });
        }
        // 合法文件带空行/负 created_at/大写 hex：宽容读入
        {
            TempDir tdb("neg");
            {
                std::ofstream f(tdb.path / "relay_state.json");
                f << "unidict-relay-state 1\n\n"
                  << "G " << kGid << " -5 0 - - -\n"
                  << "O " << kGid << " 1 7 ABCD 4142 QUJD\n";
            }
            SyncRelayStateStd s4(tdb.path.string());
            assert(s4.group_meta(kGid).created_at == -5);
            SyncRelayPull p = s4.pull_ops(kGid, 0, 200);
            assert(p.ops.size() == 1 && p.ops[0].seq == 1);
            // ABCD→{0xAB,0xCD}、4142→"AB"
            assert(p.ops[0].op_id == std::string("\xab\xcd", 2));
            assert(p.ops[0].device_id == "AB" && p.ops[0].payload == "QUJD");
        }
        // 原子替换失败：state 路径被目录占住 → 首次变更 500
        {
            TempDir tdb("blocking");
            fs::create_directory(tdb.path / "relay_state.json");
            SyncRelayStateStd s5(tdb.path.string());
            must_err(500, "persist_failed",
                     [&] { (void)s5.create_group("I0123456789abcdefghij"); });
        }
        // 写失败：.tmp 路径被目录占住 → 500
        {
            TempDir tdb("tmpblock");
            fs::create_directory(tdb.path / "relay_state.json.tmp");
            SyncRelayStateStd s6(tdb.path.string());
            must_err(500, "persist_failed",
                     [&] { (void)s6.create_group("J0123456789abcdefghij"); });
        }
        // 数据目录建不出来（父路径是文件）→ 构造期直接失败
        {
            TempDir tdb("nodir");
            const std::string file = (tdb.path / "plainfile").string();
            {
                std::ofstream f(file);
                f << "x";
            }
            bool threw = false;
            try {
                SyncRelayStateStd s7(file + "/sub");
            } catch (const std::runtime_error&) {
                threw = true;
            }
            assert(threw);
        }
    }

    std::puts("sync_relay_state_std_test: all assertions passed");
    return 0;
}
