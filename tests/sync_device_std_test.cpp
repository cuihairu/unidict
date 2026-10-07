// B4 设备面单测（server_plan §7）：设备清单、入组/退组、仅改自己备注、
// 本机标注、列表排序、JSON 持久化往返。
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "std/sync_device_std.h"

using namespace UnidictCoreStd;

int main() {
    // ---- 基础：空管理器 ----
    {
        SyncDeviceManagerStd mgr;
        assert(mgr.current_device() == nullptr);
        assert(mgr.list_devices().empty());
        assert(mgr.find_device("dev1") == nullptr);
        assert(!mgr.leave_group("dev1"));
        assert(!mgr.update_own_remark("x", nullptr));
    }

    // ---- 设置当前设备 ----
    {
        SyncDeviceManagerStd mgr("my-device-id");
        assert(mgr.current_device() == nullptr);  // 还没入组
        assert(mgr.list_devices().empty());
    }

    // ---- 入组：当前设备 ----
    {
        SyncDeviceManagerStd mgr("dev-current");
        std::string err;
        std::string id = mgr.join_group("dev-current", "My Laptop",
                                        DevicePlatform::Windows, "Windows 11", &err);
        assert(id == "dev-current");
        assert(err.empty());

        auto* cur = mgr.current_device();
        assert(cur != nullptr);
        assert(cur->device_id == "dev-current");
        assert(cur->name == "My Laptop");
        assert(cur->platform == DevicePlatform::Windows);
        assert(cur->platform_string == "Windows 11");
        assert(cur->is_current);
        assert(cur->remark.empty());
        assert(cur->last_seen_ms == 0);

        // 列表只有一个（本机）
        auto list = mgr.list_devices();
        assert(list.size() == 1);
        assert(list[0].device_id == "dev-current");
        assert(list[0].is_current);
    }

    // ---- 入组：其他设备 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "Desktop", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-2", "Phone", DevicePlatform::Android, "Android 14", &err);
        mgr.join_group("dev-3", "", DevicePlatform::macOS, "macOS 15", &err);  // 空名 → 默认名

        auto list = mgr.list_devices();
        assert(list.size() == 3);

        // 本机置顶
        assert(list[0].device_id == "dev-1");
        assert(list[0].is_current);

        // 其余按 last_seen_ms 降序（初始均为 0，顺序不定但本机置顶）
        // 找到 dev-2 和 dev-3
        bool found2 = false, found3 = false;
        for (size_t i = 1; i < list.size(); ++i) {
            if (list[i].device_id == "dev-2") {
                found2 = true;
                assert(list[i].name == "Phone");
                assert(list[i].platform == DevicePlatform::Android);
                assert(list[i].platform_string == "Android 14");
                assert(!list[i].is_current);
            }
            if (list[i].device_id == "dev-3") {
                found3 = true;
                assert(list[i].name.rfind("macOS-", 0) == 0);  // 默认名
                assert(list[i].platform == DevicePlatform::macOS);
            }
        }
        assert(found2 && found3);

        // find_device
        assert(mgr.find_device("dev-2")->name == "Phone");
        assert(mgr.find_device("dev-4") == nullptr);
    }

    // ---- 心跳更新 last_seen_ms ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-2", "B", DevicePlatform::Windows, "", &err);

        mgr.touch_device("dev-2", 1000);
        mgr.touch_device("dev-1", 500);

        auto list = mgr.list_devices();
        // 本机置顶优先，其次 last_seen_ms 降序
        assert(list[0].device_id == "dev-1");  // 本机置顶
        assert(list[1].device_id == "dev-2");  // 1000 > 500
        assert(list[1].last_seen_ms == 1000);
    }

    // ---- 更新自己的备注 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-2", "B", DevicePlatform::Windows, "", &err);

        // 当前设备改自己的备注
        assert(mgr.update_own_remark("my laptop", &err));
        assert(err.empty());
        assert(mgr.current_device()->remark == "my laptop");

        // 尝试改别人的（通过接口不可做，只能改自己的）
        // 接口只允许改 current_device，所以无法直接改别人的
        // 这里测试：无 current_device 时失败
        SyncDeviceManagerStd mgr2;
        assert(!mgr2.update_own_remark("x", &err));
        assert(!err.empty());
    }

    // ---- 更新自己的名称 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "Old Name", DevicePlatform::Linux, "", &err);
        assert(mgr.update_own_name("New Name", &err));
        assert(err.empty());
        assert(mgr.current_device()->name == "New Name");

        // 失败路径与备注同口径：无当前设备 / 当前设备不在组内
        SyncDeviceManagerStd none;
        assert(!none.update_own_name("x", &err));
        assert(!err.empty());
        SyncDeviceManagerStd ghost("not-in-group");
        assert(!ghost.update_own_name("x", &err));
        assert(!err.empty());
    }

    // ---- 退组 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-2", "B", DevicePlatform::Windows, "", &err);

        // 退掉别的设备
        assert(mgr.leave_group("dev-2"));
        assert(mgr.list_devices().size() == 1);
        assert(mgr.find_device("dev-2") == nullptr);

        // 退掉自己
        assert(mgr.leave_group("dev-1"));
        assert(mgr.list_devices().empty());
        assert(mgr.current_device() == nullptr);
        assert(mgr.find_device("dev-1") == nullptr);

        // 再退一次 → false
        assert(!mgr.leave_group("dev-1"));
    }

    // ---- 退组清空 current_device_id ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        mgr.leave_group("dev-1");
        assert(mgr.current_device() == nullptr);
        // 此时 current_device_id_ 已清空（实现里留了 current_device_id_ 但 is_current 清了）
        // 实际上实现里 leave_group 会清 current_device_id_，这是正确的
    }

    // ---- 重复入组失败 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        assert(mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err) == "dev-1");
        std::string err2;
        assert(mgr.join_group("dev-1", "B", DevicePlatform::Windows, "", &err2).empty());
        assert(!err2.empty());
    }

    // ---- 达上限失败 ----
    {
        SyncDeviceManagerStd mgr("dev-0");
        std::string err;
        for (size_t i = 0; i < SyncDeviceManagerStd::kMaxDevices; ++i) {
            mgr.join_group("dev-" + std::to_string(i), "D" + std::to_string(i),
                           DevicePlatform::Linux, "", &err);
            assert(err.empty());
        }
        assert(mgr.join_group("dev-overflow", "X", DevicePlatform::Linux, "", &err).empty());
        assert(!err.empty());
    }

    // ---- JSON 序列化往返 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "My PC", DevicePlatform::Windows, "Win 11", &err);
        mgr.join_group("dev-2", "My Phone", DevicePlatform::Android, "Android 14", &err);
        mgr.update_own_remark("work machine", &err);
        assert(err.empty());
        mgr.touch_device("dev-2", 123456789);

        std::string json = mgr.to_json();

        SyncDeviceManagerStd mgr2;
        assert(mgr2.from_json(json, &err));
        assert(err.empty());

        assert(mgr2.current_device()->device_id == "dev-1");
        assert(mgr2.current_device()->name == "My PC");
        assert(mgr2.current_device()->remark == "work machine");

        auto list = mgr2.list_devices();
        assert(list.size() == 2);
        assert(list[0].device_id == "dev-1");
        assert(list[1].device_id == "dev-2");
        assert(list[1].last_seen_ms == 123456789);
        assert(list[1].remark.empty());
        assert(list[1].platform_string == "Android 14");
    }

    // ---- JSON 往返：is_current 以 current_device_id_ 为准 ----
    {
        // 手工造一个 JSON，里面 dev-2 标了 is_current=true
        std::string json = R"({"current_device_id":"dev-1","devices":[{"device_id":"dev-1","name":"A","platform":0,"platform_string":"","last_seen_ms":0,"remark":"","is_current":false},{"device_id":"dev-2","name":"B","platform":1,"platform_string":"","last_seen_ms":0,"remark":"","is_current":true}]})";

        SyncDeviceManagerStd mgr;
        std::string err;
        assert(mgr.from_json(json, &err));
        assert(err.empty());

        // 修正后 dev-1 是 current
        assert(mgr.current_device()->device_id == "dev-1");
        assert(mgr.current_device()->is_current);
        auto* d2 = mgr.find_device("dev-2");
        assert(d2 != nullptr);
        assert(!d2->is_current);
    }

    // ---- clear ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-2", "B", DevicePlatform::Windows, "", &err);
        mgr.clear();
        assert(mgr.list_devices().empty());
        assert(mgr.current_device() == nullptr);
    }

    // ---- 平台字符串转换 ----
    {
        assert(device_platform_to_string(DevicePlatform::Windows) == "Windows");
        assert(device_platform_to_string(DevicePlatform::macOS) == "macOS");
        assert(device_platform_to_string(DevicePlatform::Linux) == "Linux");
        assert(device_platform_to_string(DevicePlatform::Android) == "Android");
        assert(device_platform_to_string(DevicePlatform::iOS) == "iOS");
        assert(device_platform_to_string(DevicePlatform::HarmonyOS) == "HarmonyOS");
        assert(device_platform_to_string(DevicePlatform::Web) == "Web");
        assert(device_platform_to_string(DevicePlatform::Unknown) == "Unknown");

        assert(device_platform_from_string("Windows") == DevicePlatform::Windows);
        assert(device_platform_from_string("macOS") == DevicePlatform::macOS);
        assert(device_platform_from_string("Linux") == DevicePlatform::Linux);
        assert(device_platform_from_string("Android") == DevicePlatform::Android);
        assert(device_platform_from_string("iOS") == DevicePlatform::iOS);
        assert(device_platform_from_string("HarmonyOS") == DevicePlatform::HarmonyOS);
        assert(device_platform_from_string("Web") == DevicePlatform::Web);
        assert(device_platform_from_string("unknown") == DevicePlatform::Unknown);
        assert(device_platform_from_string("") == DevicePlatform::Unknown);
    }

    // ---- 入组时自动标注本机 ----
    {
        SyncDeviceManagerStd mgr("dev-x");
        std::string err;
        mgr.join_group("dev-x", "A", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-y", "B", DevicePlatform::Windows, "", &err);

        // set_current_device_id 后入组的设备也会被标注
        SyncDeviceManagerStd mgr2;
        mgr2.set_current_device_id("dev-z");
        mgr2.join_group("dev-z", "C", DevicePlatform::macOS, "", &err);
        assert(mgr2.current_device()->device_id == "dev-z");
        assert(mgr2.current_device()->is_current);
    }

    // ---- 错误路径覆盖 ----
    {
        // join_group: empty device_id
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        assert(mgr.join_group("", "A", DevicePlatform::Linux, "", &err).empty());
        assert(!err.empty());

        // join_group: max devices reached（err 是出参且仓库口径只在失败
        // 时写——每轮先清，防上一子块的 "empty device_id" 残留误炸）
        SyncDeviceManagerStd mgr2("dev-0");
        for (size_t i = 0; i < SyncDeviceManagerStd::kMaxDevices; ++i) {
            err.clear();
            mgr2.join_group("dev-" + std::to_string(i), "D" + std::to_string(i),
                           DevicePlatform::Linux, "", &err);
            assert(err.empty());
        }
        assert(mgr2.join_group("dev-overflow", "X", DevicePlatform::Linux, "", &err).empty());
        assert(!err.empty());

        // leave_group: current device leaves -> current_device_id_ cleared
        SyncDeviceManagerStd mgr3("dev-cur");
        mgr3.join_group("dev-cur", "A", DevicePlatform::Linux, "", &err);
        mgr3.join_group("dev-other", "B", DevicePlatform::Windows, "", &err);
        assert(mgr3.leave_group("dev-cur"));
        assert(mgr3.current_device() == nullptr);
        assert(mgr3.list_devices().size() == 1);
    }

    // ---- from_json 错误路径 ----
    {
        SyncDeviceManagerStd mgr;
        std::string err;

        // missing current_device_id
        assert(!mgr.from_json(R"({"devices":[]})", &err));
        assert(!err.empty());

        // missing devices array
        err.clear();
        assert(!mgr.from_json(R"({"current_device_id":"dev-1"})", &err));
        assert(!err.empty());
    }

    // ---- json_escape 控制字符 ----
    {
        // 测试控制字符转义（\b, \f, \n, \r, \t, 和其他 < 0x20）
        std::string input = "\x01\x02\x07\x08\x0c\n\r\t\x1f";
        // 这里不直接测试内部函数，而是通过 to_json 间接测试
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "\x01\x02\x07\x08\x0c\n\r\t\x1f", DevicePlatform::Linux, "", &err);
        std::string json = mgr.to_json();
        // 验证 JSON 包含转义序列
        assert(json.find("\\u") != std::string::npos);
    }

    // ---- platform string 未知值 ----
    {
        // device_platform_to_string 的 default case
        // 通过强制转换测试（虽然正常代码不会产生）
        // 这里只能测试 device_platform_from_string 的未知值
        assert(device_platform_from_string("UnknownPlatform") == DevicePlatform::Unknown);
        assert(device_platform_from_string("RandomString") == DevicePlatform::Unknown);
    }

    // ---- update_own_remark 无当前设备 ----
    {
        SyncDeviceManagerStd mgr;
        std::string err;
        assert(!mgr.update_own_remark("test", &err));
        assert(!err.empty());
    }

    // ---- update_own_remark 当前设备不在组内 ----
    {
        SyncDeviceManagerStd mgr("dev-not-in-group");
        std::string err;
        assert(!mgr.update_own_remark("test", &err));
        assert(!err.empty());
    }

    // ---- touch_device 不存在的设备（不应崩溃） ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        mgr.touch_device("dev-nonexistent", 12345);  // 应静默忽略
        // 验证现有设备未受影响
        assert(mgr.find_device("dev-1")->last_seen_ms == 0);
    }

    // ---- leave_group 退掉不存在的设备 ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        mgr.join_group("dev-1", "A", DevicePlatform::Linux, "", &err);
        assert(!mgr.leave_group("dev-nonexistent"));
    }

    // ---- set_current_device_id 更新已在组内的设备 is_current ----
    {
        SyncDeviceManagerStd mgr("dev-old");
        std::string err;
        mgr.join_group("dev-old", "A", DevicePlatform::Linux, "", &err);
        mgr.join_group("dev-new", "B", DevicePlatform::Windows, "", &err);
        // 切换当前设备
        mgr.set_current_device_id("dev-new");
        assert(mgr.current_device()->device_id == "dev-new");
        assert(mgr.current_device()->is_current);
        assert(!mgr.find_device("dev-old")->is_current);
    }

    // ---- JSON 往返无损：引号/反斜杠/全部短转义/控制字符（\u 路径）/
    //      括号字面（json_escape 不转义 []{}，span 扫描必须字符串感知） ----
    {
        SyncDeviceManagerStd mgr("dev-1");
        std::string err;
        const std::string tricky =
            "quote:\" back:\\ nl:\n tab:\t cr:\r bs:\b ff:\f ctl:\x01 "
            "brk:[]{} end";
        mgr.join_group("dev-1", tricky, DevicePlatform::Linux, "p\"s", &err);
        assert(err.empty());
        assert(mgr.update_own_remark(tricky, &err));
        assert(err.empty());

        SyncDeviceManagerStd mgr2;
        assert(mgr2.from_json(mgr.to_json(), &err));
        assert(err.empty());
        assert(mgr2.current_device()->name == tricky);
        assert(mgr2.current_device()->remark == tricky);
        assert(mgr2.current_device()->platform_string == "p\"s");
    }

    // ---- 手造 JSON 矩阵：\u 解码/代理对/孤立代理/坏 \u/未知转义/\/
    //      + 缺字段容错 + 坏值形态跳过 ----
    {
        // d1：BMP A（A→'A'）+ 代理对 😀（😀→F0 9F 98 80）
        // d2：残缺形态——platform 非数字、last_seen_ms 非数字、is_current
        //     null、remark 非字符串、"name" 键后无冒号（放末位防误吞
        //     后续键的冒号）→ 全部走 getter false 分支落缺省
        // d3：孤立高/低代理 → U+FFFD(EF BF BD)；坏 \u、未知转义字面
        //     保留；\/ → /
        // {"name":...}：无 device_id → 整机不收
        // 尾部 5：非对象元素 → 数组解析截停
        std::string json;
        json += "{\"current_device_id\":\"d1\",\"devices\":[";
        json += "{\"device_id\":\"d1\",";
        json += "\"name\":\"A\\u0041\\u00e9\\ud83d\\ude00B\",";
        json += "\"platform\":3,\"platform_string\":\"x\\/y\",";
        json += "\"last_seen_ms\":7,\"remark\":\"r\",\"is_current\":true}";
        json += ",{\"device_id\":\"d2\",";
        json += "\"platform\":\"x\",\"last_seen_ms\":\"abc\",";
        json += "\"is_current\":null,\"remark\":42,\"name\"}";
        json += ",{\"device_id\":\"d3\",";
        json += "\"name\":\"hi\\ud83dlo\",\"remark\":\"lo\\ude00 end\",";
        json += "\"platform_string\":\"bad:\\uZZZZ esc:\\q slash:a\\/b\"}";
        json += ",{\"name\":\"no-id\"}";
        json += ",5]}";

        SyncDeviceManagerStd mgr;
        std::string err;
        assert(mgr.from_json(json, &err));
        assert(err.empty());

        // d1：BMP + 代理对解码，\/ → /
        const auto* d1 = mgr.find_device("d1");
        assert(d1 != nullptr);
        assert(d1->name == std::string("A") + "A" + "\xC3\xA9" +
               "\xF0\x9F\x98\x80" + "B");
        assert(d1->platform == DevicePlatform::Linux);
        assert(d1->platform_string == "x/y");
        assert(d1->last_seen_ms == 7);
        assert(d1->remark == "r");
        assert(d1->is_current);  // 以 current_device_id=d1 归一

        // d2：残缺字段全落缺省（getter false 分支）
        const auto* d2 = mgr.find_device("d2");
        assert(d2 != nullptr);
        assert(d2->name.empty());
        assert(d2->platform == DevicePlatform::Unknown);
        assert(d2->platform_string.empty());
        assert(d2->last_seen_ms == 0);
        assert(d2->remark.empty());
        assert(!d2->is_current);

        // d3：孤立代理 → U+FFFD；坏 \u/未知转义字面保留；\/ → /
        const auto* d3 = mgr.find_device("d3");
        assert(d3 != nullptr);
        assert(d3->name == std::string("hi") + "\xEF\xBF\xBD" + "lo");
        assert(d3->remark == std::string("lo") + "\xEF\xBF\xBD" + " end");
        assert(d3->platform_string == "bad:\\uZZZZ esc:\\q slash:a/b");

        // 无 id 的不收
        assert(mgr.find_device("no-id") == nullptr);
        assert(mgr.list_devices().size() == 3);
    }

    // ---- 顶层截断：字符串值未闭合 → 整体拒读（坏文件空起由调用方兜） ----
    {
        SyncDeviceManagerStd mgr;
        std::string err;
        assert(!mgr.from_json("{\"current_device_id\":\"trunc", &err));
        assert(!err.empty());
    }

    // ---- 空设备清单可往返（to_json 自己的产物必须能回读） ----
    {
        SyncDeviceManagerStd mgr;
        mgr.set_current_device_id("solo");
        const std::string json = mgr.to_json();
        assert(json.find("\"devices\":[]") != std::string::npos);

        SyncDeviceManagerStd mgr2;
        std::string err;
        assert(mgr2.from_json(json, &err));
        assert(err.empty());
        assert(mgr2.list_devices().empty());
        assert(mgr2.current_device() == nullptr);  // id 在、未入组

        // 手造最小空清单
        SyncDeviceManagerStd mgr3;
        assert(mgr3.from_json("{\"current_device_id\":\"x\",\"devices\":[]}", &err));
        assert(err.empty());
        assert(mgr3.list_devices().empty());
    }

    // ---- devices 数组坏形态：无 '[' / 未闭合 ----
    {
        SyncDeviceManagerStd mgr;
        std::string err;
        assert(!mgr.from_json("{\"current_device_id\":\"x\",\"devices\":}", &err));
        assert(!err.empty());

        err.clear();
        assert(!mgr.from_json(
            "{\"current_device_id\":\"x\",\"devices\":[{\"device_id\":\"a\"}", &err));
        assert(!err.empty());
    }

    std::puts("sync_device_std_test: all assertions passed");
    return 0;
}