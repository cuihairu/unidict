// JNI 胶水公共件：字符串/数组转换与 jlong 句柄取还。
// 句柄 = C++ new 出来的裸指针以 jlong 持有，壳层配 close() 显式释放；
// 单线程调用起步（core 无内部锁假设，mobile_plan §4）。
//
// 编码注记：JNI 的 GetStringUTFChars/NewStringUTF 走 modified UTF-8
// （补充平面字符是代理对形式）；示例词典与常规词头 ASCII/BMP 内无差，
// 真实词库的星平面字符转换留 M2 收口（届时集中换 UTF-8 正道）。
#pragma once

#include <jni.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace unidict_jni {

// 失败口径（深审 P2）：FindClass/GetMethodID/NewObjectArray/NewObject
// 任一返回 NULL（缺类——R8 未 keep、OOM、签名失配）即短路返回 nullptr，
// 让挂起的 Java 异常浮出；NULL 继续喂给后续 JNI 调用是 fatal abort，
// 进程直接终止，不是普通错误。

inline std::string to_std(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* p = env->GetStringUTFChars(s, nullptr);
    std::string out(p ? p : "");
    if (p) env->ReleaseStringUTFChars(s, p);
    return out;
}

inline jstring to_jstring(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

inline jobjectArray to_jstring_array(JNIEnv* env,
                                     const std::vector<std::string>& v) {
    const jclass cls = env->FindClass("java/lang/String");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), cls,
                                           nullptr);
    if (!arr) return nullptr;
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        jstring s = to_jstring(env, v[static_cast<size_t>(i)]);
        env->SetObjectArrayElement(arr, i, s);
        env->DeleteLocalRef(s);
    }
    return arr;
}

// ---- 句柄登记簿（深审 P3）----
//
// jlong 裸指针句柄的全部风险面——陈旧句柄（close 后壳层残留引用再发调
// 用 → UAF）、双重 close（第二次 delete → 双释放）、垃圾值/0——统一在
// 登记簿收口：to_handle 只收 new 出来的指针并登记，handle_of 只放行
// 登记簿里的活句柄（其余一律 nullptr，调用方判空即安全网），close 走
// release_handle 注销后再 delete，重复 close 幂等。
//
// 所有权契约（storeOpen/dictOpen ↔ storeClose/dictClose）：对象生命期
// 由壳层显式管理，Kotlin 侧 Activity/Repository 生命周期成对调用
// open/close（mobile_plan §4）；壳层不 close 是唯一残余泄漏窗口，属
// 壳层契约而非本层可堵的洞。
//
// 登记簿静态存活至进程卸载（JNI so 生命期），锁保护仅防多 ClassLoader
// 双加载的病态场景；壳层单线程调用起步（mobile_plan §4），close 与
// 后续调用的真并发不在此收口。
inline std::unordered_set<uintptr_t>& live_handles() {
    static std::unordered_set<uintptr_t> set;
    return set;
}

inline std::mutex& handles_mu() {
    static std::mutex m;
    return m;
}

template <typename T>
inline T* handle_of(jlong h) {
    std::lock_guard<std::mutex> lk(handles_mu());
    if (live_handles().count(static_cast<uintptr_t>(h)) == 0) return nullptr;
    return reinterpret_cast<T*>(static_cast<uintptr_t>(h));
}

template <typename T>
inline jlong to_handle(T* p) {
    std::lock_guard<std::mutex> lk(handles_mu());
    live_handles().insert(reinterpret_cast<uintptr_t>(p));
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(p));
}

// 注销并回报是否本来是活句柄（重复 close / 垃圾值 → false，调用方据此
// 跳过 delete）
inline bool release_handle(jlong h) {
    std::lock_guard<std::mutex> lk(handles_mu());
    return live_handles().erase(static_cast<uintptr_t>(h)) > 0;
}

// 结构化条目 → Kotlin data class（mobile_plan §4：结构体平铺，避免
// jobject 频繁往返——词条页一屏至多几十条，逐条 NewObject 成本可接受）。
// 类名/构造签名与 UnidictCore.kt 里的 data class 字段序严格对齐。
inline jobject new_search_hit(JNIEnv* env, const std::string& dict_name,
                              const std::string& word,
                              const std::string& definition) {
    const jclass cls = env->FindClass("dev/unidict/mobile/SearchHit");
    if (!cls) return nullptr;
    const jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor) return nullptr;
    jstring d = to_jstring(env, dict_name);
    jstring w = to_jstring(env, word);
    jstring def = to_jstring(env, definition);
    jobject obj = env->NewObject(cls, ctor, d, w, def);
    if (!obj) return nullptr;
    env->DeleteLocalRef(d);
    env->DeleteLocalRef(w);
    env->DeleteLocalRef(def);
    return obj;
}

inline jobjectArray to_search_hit_array(
    JNIEnv* env,
    const std::vector<std::tuple<std::string, std::string, std::string>>& v) {
    const jclass cls = env->FindClass("dev/unidict/mobile/SearchHit");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), cls,
                                           nullptr);
    if (!arr) return nullptr;
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        jobject hit = new_search_hit(env, std::get<0>(v[static_cast<size_t>(i)]),
                                     std::get<1>(v[static_cast<size_t>(i)]),
                                     std::get<2>(v[static_cast<size_t>(i)]));
        if (!hit) return nullptr;
        env->SetObjectArrayElement(arr, i, hit);
        env->DeleteLocalRef(hit);
    }
    return arr;
}

// 生词条目 → Kotlin data class VocabItem(word/definition/tags/note)。
// tags 走 ';' 连接平铺（core 标签约定不含分号），避免嵌套数组 JNI 往返。
inline jobject new_vocab_item(JNIEnv* env, const std::string& word,
                              const std::string& definition,
                              const std::string& tags,
                              const std::string& note) {
    const jclass cls = env->FindClass("dev/unidict/mobile/VocabItem");
    if (!cls) return nullptr;
    const jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor) return nullptr;
    jstring w = to_jstring(env, word);
    jstring d = to_jstring(env, definition);
    jstring t = to_jstring(env, tags);
    jstring n = to_jstring(env, note);
    jobject obj = env->NewObject(cls, ctor, w, d, t, n);
    if (!obj) return nullptr;
    env->DeleteLocalRef(w);
    env->DeleteLocalRef(d);
    env->DeleteLocalRef(t);
    env->DeleteLocalRef(n);
    return obj;
}

inline jobjectArray to_vocab_item_array(
    JNIEnv* env,
    const std::vector<std::tuple<std::string, std::string, std::string,
                                 std::string>>& v) {
    const jclass cls = env->FindClass("dev/unidict/mobile/VocabItem");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), cls,
                                           nullptr);
    if (!arr) return nullptr;
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        const auto& t = v[static_cast<size_t>(i)];
        jobject item = new_vocab_item(env, std::get<0>(t), std::get<1>(t),
                                      std::get<2>(t), std::get<3>(t));
        if (!item) return nullptr;
        env->SetObjectArrayElement(arr, i, item);
        env->DeleteLocalRef(item);
    }
    return arr;
}

inline jobject new_dict_meta(JNIEnv* env, const std::string& name, int words,
                             const std::string& description) {
    const jclass cls = env->FindClass("dev/unidict/mobile/DictMetaInfo");
    if (!cls) return nullptr;
    const jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;ILjava/lang/String;)V");
    if (!ctor) return nullptr;
    jstring n = to_jstring(env, name);
    jstring desc = to_jstring(env, description);
    jobject obj = env->NewObject(cls, ctor, n, static_cast<jint>(words), desc);
    if (!obj) return nullptr;
    env->DeleteLocalRef(n);
    env->DeleteLocalRef(desc);
    return obj;
}

} // namespace unidict_jni
