// JNI 胶水失败路径钉（深审 P2）：FindClass/GetMethodID/NewObjectArray
// 失败返回 NULL 时旧码继续喂 NULL——真实 JVM 直接 fatal abort 进程（且
// R8 release 包里 data class 未 keep 时就是缺类常态）。host 上用真
// jni.h 构造 JNIEnv 函数表模拟：NULL 喂入即记 crash 账（等价 JVM 中止
// 语义），修复后所有失败路径必须短路返回 nullptr、让挂起的异常浮出、
// 不再触碰任何 NULL。需要本机 JDK 的 jni.h，缺席时 CMake 不注册本测。
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <map>
#include <set>
#include <string>

#include <jni.h>

#include "jni_util.h"

using namespace unidict_jni;

namespace {

// —— 模拟 JVM 账本：crash = 真实 JVM 里 fatal error 直接终止进程 ——
int g_crashes = 0;
std::string g_first_crash;
bool g_exception_pending = false;
std::set<std::string> g_classes;       // 本轮可用的类（模拟 R8 keep 缺失）
bool g_ctor_ok = true;                 // GetMethodID 是否命中
int g_set_elem_calls = 0;
int g_deleted_refs = 0;

void crash(const char* what) {
    if (g_crashes == 0) g_first_crash = what;
    ++g_crashes;
}

// 各返回值哨兵 token（数值即可，mock 只判空与非空）
jclass class_token(const char* name) {
    return reinterpret_cast<jclass>(0x100 + std::strlen(name));
}
const std::map<std::string, jclass>& class_tokens() {
    static const std::map<std::string, jclass> m = {
        {"java/lang/String", class_token("java/lang/String")},
        {"dev/unidict/mobile/SearchHit", class_token("dev/unidict/mobile/SearchHit")},
        {"dev/unidict/mobile/VocabItem", class_token("dev/unidict/mobile/VocabItem")},
        {"dev/unidict/mobile/DictMetaInfo", class_token("dev/unidict/mobile/DictMetaInfo")},
    };
    return m;
}

jclass MOCK_FindClass(JNIEnv*, const char* name) {
    const auto& m = class_tokens();
    const auto it = m.find(name);
    if (it != m.end() && g_classes.count(name)) return it->second;
    g_exception_pending = true;  // 真实 JVM：挂 NoClassDefFoundError
    return nullptr;
}
jmethodID MOCK_GetMethodID(JNIEnv*, jclass cls, const char*, const char*) {
    if (!cls) {
        crash("GetMethodID(null class)");
        return nullptr;
    }
    if (!g_ctor_ok) {
        g_exception_pending = true;
        return nullptr;
    }
    return reinterpret_cast<jmethodID>(0x201);
}
jobject MOCK_NewObject(JNIEnv*, jclass cls, jmethodID ctor, ...) {
    if (!cls || !ctor) {
        crash("NewObject(null class or ctor)");
        return nullptr;
    }
    return reinterpret_cast<jobject>(0x401);
}
// C++ jni.h 的 inline NewObject 把变参转 va_list 后调 functions->NewObjectV
// （jni.h:878-882）——V 槽才是实际被调面，直接槽一并挂同实现兜底
jobject MOCK_NewObjectV(JNIEnv*, jclass cls, jmethodID ctor, va_list) {
    return MOCK_NewObject(nullptr, cls, ctor);
}
jobjectArray MOCK_NewObjectArray(JNIEnv*, jsize, jclass cls, jobject) {
    if (!cls) {
        crash("NewObjectArray(null class)");
        return nullptr;
    }
    return reinterpret_cast<jobjectArray>(0x301);
}
void MOCK_SetObjectArrayElement(JNIEnv*, jobjectArray arr, jsize, jobject) {
    if (!arr) {
        crash("SetObjectArrayElement(null array)");
        return;
    }
    ++g_set_elem_calls;
}
void MOCK_DeleteLocalRef(JNIEnv*, jobject) { ++g_deleted_refs; }
jstring MOCK_NewStringUTF(JNIEnv*, const char*) {
    return reinterpret_cast<jstring>(0x501);
}
const char* MOCK_GetStringUTFChars(JNIEnv*, jstring s, jboolean*) {
    if (!s) {
        crash("GetStringUTFChars(null string)");
        return nullptr;
    }
    return strdup("probe");
}
void MOCK_ReleaseStringUTFChars(JNIEnv*, jstring, const char* p) { free(const_cast<char*>(p)); }
jboolean MOCK_ExceptionCheck(JNIEnv*) { return g_exception_pending ? JNI_TRUE : JNI_FALSE; }

// 组装 JNIEnv：真 jni.h 的 C++ 形态是 functions 指针 + 内联转发，
// 填好函数表挂上去即可（host 等价真机语义）
JNIEnv* mock_env() {
    static JNINativeInterface_ tbl = {};
    static JNIEnv env;
    tbl.FindClass = MOCK_FindClass;
    tbl.GetMethodID = MOCK_GetMethodID;
    tbl.NewObject = MOCK_NewObject;
    tbl.NewObjectV = MOCK_NewObjectV;
    tbl.NewObjectArray = MOCK_NewObjectArray;
    tbl.SetObjectArrayElement = MOCK_SetObjectArrayElement;
    tbl.DeleteLocalRef = MOCK_DeleteLocalRef;
    tbl.NewStringUTF = MOCK_NewStringUTF;
    tbl.GetStringUTFChars = MOCK_GetStringUTFChars;
    tbl.ReleaseStringUTFChars = MOCK_ReleaseStringUTFChars;
    tbl.ExceptionCheck = MOCK_ExceptionCheck;
    env.functions = &tbl;
    return &env;
}

void reset(bool string_ok, bool search_hit_ok, bool vocab_item_ok,
           bool dict_meta_ok, bool ctor_ok = true) {
    g_crashes = 0;
    g_first_crash.clear();
    g_exception_pending = false;
    g_set_elem_calls = 0;
    g_deleted_refs = 0;
    g_ctor_ok = ctor_ok;
    g_classes.clear();
    if (string_ok) g_classes.insert("java/lang/String");
    if (search_hit_ok) g_classes.insert("dev/unidict/mobile/SearchHit");
    if (vocab_item_ok) g_classes.insert("dev/unidict/mobile/VocabItem");
    if (dict_meta_ok) g_classes.insert("dev/unidict/mobile/DictMetaInfo");
}

}  // namespace

int main() {
    JNIEnv* env = mock_env();

    // 快乐路径：全类在册，转换无 crash、逐元素写入
    {
        reset(true, true, true, true);
        const std::vector<std::string> tags = {"a", "b", "c"};
        jobjectArray arr = to_jstring_array(env, tags);
        assert(arr != nullptr);
        assert(g_set_elem_calls == 3);
        assert(g_crashes == 0);
        assert(!g_exception_pending);
    }

    // String 类缺失：必须短路返回 nullptr、零 crash、异常挂起浮出。
    // 旧码拿 NULL cls 继续 NewObjectArray/SetObjectArrayElement = JVM 中止
    {
        reset(false, false, false, false);
        const jobjectArray arr = to_jstring_array(env, {"x"});
        assert(arr == nullptr);
        assert(g_crashes == 0);
        assert(g_exception_pending);
    }

    // SearchHit 类缺失（R8 未 keep 的常态）：new_search_hit 不得喂 NULL
    // 给 GetMethodID；数组构建必须在 NewObjectArray 处短路
    {
        reset(true, false, true, true);
        const jobject hit = new_search_hit(env, "d", "w", "def");
        assert(hit == nullptr);
        assert(g_crashes == 0);
        assert(g_exception_pending);

        g_exception_pending = false;
        const jobjectArray arr = to_search_hit_array(env, {{"d", "w", "def"}});
        assert(arr == nullptr);
        assert(g_crashes == 0);
        assert(g_exception_pending);
    }

    // 构造签名失配（GetMethodID NULL）：不得喂 NULL 给 NewObject
    {
        reset(true, true, true, true, /*ctor_ok=*/false);
        const jobject hit = new_search_hit(env, "d", "w", "def");
        assert(hit == nullptr);
        assert(g_crashes == 0);
        assert(g_exception_pending);
    }

    // VocabItem 类缺失：数组构建短路
    {
        reset(true, true, false, true);
        const jobjectArray arr = to_vocab_item_array(
            env, {{"w", "d", "t", "n"}});
        assert(arr == nullptr);
        assert(g_crashes == 0);
        assert(g_exception_pending);
    }

    // VocabItem 快乐路径（元素逐个释放，无泄漏面）
    {
        reset(true, true, true, true);
        const jobjectArray arr = to_vocab_item_array(env, {{"w", "d", "t", "n"}});
        assert(arr != nullptr);
        assert(g_set_elem_calls == 1);
        assert(g_crashes == 0);
        assert(g_deleted_refs >= 1);
    }

    // DictMetaInfo 类缺失：new_dict_meta 短路
    {
        reset(true, true, true, false);
        const jobject meta = new_dict_meta(env, "d", 3, "desc");
        assert(meta == nullptr);
        assert(g_crashes == 0);
        assert(g_exception_pending);
    }

    // DictMetaInfo 快乐路径
    {
        reset(true, true, true, true);
        const jobject meta = new_dict_meta(env, "d", 3, "desc");
        assert(meta != nullptr);
        assert(g_crashes == 0);
    }

    // 句柄往返与字符串转换（附带走查）
    {
        reset(true, true, true, true);
        const int probe = 7;
        assert(handle_of<int>(to_handle(&probe)) == &probe);
        assert(to_std(env, reinterpret_cast<jstring>(0x601)) == "probe");
        assert(g_crashes == 0);
    }

    std::printf("OK\n");
    return 0;
}
