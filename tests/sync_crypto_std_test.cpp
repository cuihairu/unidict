// B3-a 同步密钥管理层（server_plan §7）：组密钥生命周期（建组/轮换/
// 退出销毁/版本选钥）、指令密封的版本头与 AAD 绑定、动态配对码的
// 签发/归一化/单次消费/时效。密码学原语本身由 crypto_std_test 用
// RFC 向量钉死，这里测的是语义编排。
#include <stdexcept>

#include <cassert>
#include <cstdio>
#include <string>

#include "std/sync_crypto_std.h"

using namespace UnidictCoreStd;

namespace {

template <typename Fn>
bool throws_invalid_argument(Fn&& fn) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
    }
    return false;
}

template <typename Fn>
bool throws_logic_error(Fn&& fn) {
    try {
        fn();
    } catch (const std::logic_error&) {
        return true;
    } catch (...) {
    }
    return false;
}

// 版本头手工拼装（契约：4B LE 版本 || nonce24 || ct || tag16）
std::string with_version(std::uint32_t v, const std::string& body) {
    std::string s(4, '\0');
    s[0] = (char)(v & 0xff);
    s[1] = (char)((v >> 8) & 0xff);
    s[2] = (char)((v >> 16) & 0xff);
    s[3] = (char)((v >> 24) & 0xff);
    return s + body;
}

}  // namespace

int main() {
    // ---- 组密钥生命周期 ----
    {
        SyncKeyRingStd ring;
        assert(!ring.has_current());
        assert(ring.current_version() == 0);
        // 空环上操作 = 逻辑错误
        assert(throws_logic_error([&] { (void)ring.rotate(); }));
        assert(throws_logic_error([&] { (void)ring.seal_command("x"); }));

        ring.init_new_group();
        assert(ring.has_current());
        assert(ring.current_version() == 1);

        // 指令往返 + 版本头形状（sealed = ver4 || nonce24 || ct || tag16）
        const std::string cmd = "add_word {\"w\":\"hello\"}";
        const std::string sealed = ring.seal_command(cmd);
        assert(sealed.size() == kSyncSealedOverhead + cmd.size());
        std::string got;
        assert(ring.open_command(sealed, got));
        assert(got == cmd);

        // 重复建组：旧密文全部作废（旧钥被擦除，版本也重置）
        const std::string sealed2 = ring.seal_command("second group");
        ring.init_new_group();
        std::string got2;
        assert(!ring.open_command(sealed2, got2));  // 旧组密文解不开
        assert(ring.current_version() == 1);
        assert(ring.open_command(ring.seal_command("fresh"), got2));
        assert(got2 == "fresh");
    }

    // ---- 轮换：旧版本钥保留解在途指令 ----
    {
        SyncKeyRingStd ring;
        ring.init_new_group();
        const std::string v1_cmd = "cmd before rotate";
        const std::string sealed_v1 = ring.seal_command(v1_cmd);

        const std::uint32_t v2 = ring.rotate();
        assert(v2 == 2);
        assert(ring.current_version() == 2);

        const std::string v2_cmd = "cmd after rotate";
        const std::string sealed_v2 = ring.seal_command(v2_cmd);

        // 两条都解得开：v1 封的走历史钥，v2 封的走当前钥
        std::string got;
        assert(ring.open_command(sealed_v1, got));
        assert(got == v1_cmd);
        assert(ring.open_command(sealed_v2, got));
        assert(got == v2_cmd);

        // 新封指令用当前版本（头 = 2）
        const unsigned char* p = reinterpret_cast<const unsigned char*>(sealed_v2.data());
        assert(p[0] == 2 && p[1] == 0 && p[2] == 0 && p[3] == 0);

        // 未知版本头 → false（出参不动）
        const std::string unknown =
            with_version(99, aead_xchacha20poly1305_seal(std::string(32, '\x42'),
                                                         "x", kSyncCommandAad));
        got = "sentinel";
        assert(!ring.open_command(unknown, got));
        assert(got == "sentinel");

        // 长度不足（低于 ver+nonce+tag 最小开销）→ false
        assert(!ring.open_command(std::string(kSyncSealedOverhead - 1, '\x07'), got));

        // 密文位篡改 → false
        std::string tampered = sealed_v2;
        tampered[4 + 5] ^= 0x08;
        assert(!ring.open_command(tampered, got));

        // 异组同版本号：版本查得到但钥不同 → tag 校验拒
        SyncKeyRingStd other;
        other.init_new_group();
        assert(other.current_version() == 1);
        const std::string other_cmd = "other group cmd";
        const std::string other_sealed = other.seal_command(other_cmd);
        assert(!ring.open_command(other_sealed, got));   // ring v1 ≠ other v1
        std::string ogot;
        assert(other.open_command(other_sealed, ogot));
        assert(ogot == other_cmd);
        assert(!other.open_command(sealed_v1, ogot));    // 反向也拒

        // 密钥注入面（B3-b PAKE 换钥完成后调用）
        assert(throws_invalid_argument([&] { ring.import_key(3, std::string(31, '\0')); }));
        assert(throws_invalid_argument([&] { ring.import_key(0, std::string(32, '\0')); }));
        assert(throws_invalid_argument([&] { ring.import_key(1, std::string(32, '\0')); }));
        const std::string injected(32, '\x5a');
        ring.import_key(7, injected);  // 版本必须严格递增
        assert(ring.current_version() == 7);
        assert(ring.open_command(sealed_v1, got));  // 历史钥仍在环内
        assert(ring.open_command(sealed_v2, got));  // v2 钥也保留

        // 退出/销毁：全部密钥擦除，任何密文都成死信
        ring.destroy();
        assert(!ring.has_current());
        assert(!ring.open_command(sealed_v1, got));
        assert(throws_logic_error([&] { (void)ring.seal_command("x"); }));
        assert(throws_logic_error([&] { (void)ring.rotate(); }));
    }

    // ---- 配对码编码：确定性向量 + 字母表形状 ----
    {
        assert(pairing_code_encode(std::string("\x00\x01\x02\x03\x04", 5)) ==
               "000G40R4");
        assert(pairing_code_encode(std::string("\xff\xff\xff\xff\xff", 5)) ==
               "ZZZZZZZZ");
        // 5 字节熵 = 8 字符
        assert(pairing_code_encode(std::string(5, '\x2a')).size() == kPairingCodeLen);
        assert(throws_invalid_argument([] { (void)pairing_code_encode("4bytes"); }));
        assert(throws_invalid_argument([] { (void)pairing_code_encode("6bytes!!"); }));
    }

    // ---- 输入归一化：分隔符/小写/Crockford 别名/非法字符 ----
    {
        assert(pairing_code_normalize("000G40R4") == "000G40R4");
        assert(pairing_code_normalize("000g40r4") == "000G40R4");       // 小写
        assert(pairing_code_normalize("000-g40 r4") == "000G40R4");     // 展示分组
        assert(pairing_code_normalize("000\tG40R4") == "000G40R4");     // 制表符
        assert(pairing_code_normalize("OoIiLl") == "001111");           // 别名映射
        assert(pairing_code_normalize("U").empty());                    // 排除字母
        assert(pairing_code_normalize("u").empty());                    // 小写同判
        assert(pairing_code_normalize("ABC!DEF").empty());              // 字母表外
        assert(pairing_code_normalize("IAG") == "1AG");                 // I→1 且不整串拒
        assert(pairing_code_normalize("").empty());
    }

    // ---- 配对码签发与单次消费 ----
    {
        PairingCodeManagerStd mgr;
        assert(!mgr.has_active());
        // 未签发时消费 → false
        assert(!mgr.consume("000G40R4", 0));

        const PairingCodeStd c1 = mgr.issue(1000);
        assert(mgr.has_active());
        assert(c1.code.size() == kPairingCodeLen);
        assert(c1.issued_at_ms == 1000);
        assert(c1.expires_at_ms == 1000 + kPairingDefaultTtlMs);
        assert(mgr.active().code == c1.code);

        // 归一化形态（小写+连字符）也能消费
        std::string messy = c1.code;
        messy.insert(4, "-");
        for (char& ch : messy)
            if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
        assert(mgr.consume(messy, 1500));

        // 单次有效：再消费 → false
        assert(!mgr.consume(c1.code, 1501));

        // 重签发作废旧码：旧码消费不了，新码可用
        const PairingCodeStd c2 = mgr.issue(2000);
        assert(c2.code != c1.code);
        assert(!mgr.consume(c1.code, 2001));
        assert(mgr.consume(c2.code, 2001));

        // 时效：到期边界（now >= expires 拒），提前 1ms 仍可
        const PairingCodeStd c3 = mgr.issue(3000, 100);
        assert(c3.expires_at_ms == 3100);
        assert(mgr.consume(c3.code, 3099));
        assert(!mgr.consume(c3.code, 3099));  // 已消费
        const PairingCodeStd c4 = mgr.issue(4000, 100);
        assert(!mgr.consume(c4.code, 4100));  // 过期点
        assert(!mgr.consume("wrongcode", 4050));  // 错码（归一化后不等）

        // 过期码消费不动 consumed_，换成新码后仍能消费（签发即复位）
        const PairingCodeStd c5 = mgr.issue(5000);
        assert(mgr.consume(c5.code, 5000));

        // 重置：码作废、状态回初始
        mgr.reset();
        assert(!mgr.has_active());
        assert(!mgr.consume(c5.code, 5001));

        // 自定义短时效
        const PairingCodeStd c6 = mgr.issue(6000, 30 * 1000);
        assert(c6.expires_at_ms == 6000 + 30 * 1000);
        assert(mgr.consume(c6.code, 6000));
    }

    std::puts("sync_crypto_std_test: all assertions passed");
    return 0;
}
