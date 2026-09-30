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
#include <string>
#include <vector>

namespace unidict_jni {

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
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), cls,
                                           nullptr);
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        jstring s = to_jstring(env, v[static_cast<size_t>(i)]);
        env->SetObjectArrayElement(arr, i, s);
        env->DeleteLocalRef(s);
    }
    return arr;
}

template <typename T>
inline T* handle_of(jlong h) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(h));
}

template <typename T>
inline jlong to_handle(T* p) {
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(p));
}

// 结构化条目 → Kotlin data class（mobile_plan §4：结构体平铺，避免
// jobject 频繁往返——词条页一屏至多几十条，逐条 NewObject 成本可接受）。
// 类名/构造签名与 UnidictCore.kt 里的 data class 字段序严格对齐。
inline jobject new_search_hit(JNIEnv* env, const std::string& dict_name,
                              const std::string& word,
                              const std::string& definition) {
    const jclass cls = env->FindClass("dev/unidict/mobile/SearchHit");
    const jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jstring d = to_jstring(env, dict_name);
    jstring w = to_jstring(env, word);
    jstring def = to_jstring(env, definition);
    jobject obj = env->NewObject(cls, ctor, d, w, def);
    env->DeleteLocalRef(d);
    env->DeleteLocalRef(w);
    env->DeleteLocalRef(def);
    return obj;
}

inline jobjectArray to_search_hit_array(
    JNIEnv* env,
    const std::vector<std::tuple<std::string, std::string, std::string>>& v) {
    const jclass cls = env->FindClass("dev/unidict/mobile/SearchHit");
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), cls,
                                           nullptr);
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        jobject hit = new_search_hit(env, std::get<0>(v[static_cast<size_t>(i)]),
                                     std::get<1>(v[static_cast<size_t>(i)]),
                                     std::get<2>(v[static_cast<size_t>(i)]));
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
    const jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jstring w = to_jstring(env, word);
    jstring d = to_jstring(env, definition);
    jstring t = to_jstring(env, tags);
    jstring n = to_jstring(env, note);
    jobject obj = env->NewObject(cls, ctor, w, d, t, n);
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
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), cls,
                                           nullptr);
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        const auto& t = v[static_cast<size_t>(i)];
        jobject item = new_vocab_item(env, std::get<0>(t), std::get<1>(t),
                                      std::get<2>(t), std::get<3>(t));
        env->SetObjectArrayElement(arr, i, item);
        env->DeleteLocalRef(item);
    }
    return arr;
}

inline jobject new_dict_meta(JNIEnv* env, const std::string& name, int words,
                             const std::string& description) {
    const jclass cls = env->FindClass("dev/unidict/mobile/DictMetaInfo");
    const jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;ILjava/lang/String;)V");
    jstring n = to_jstring(env, name);
    jstring desc = to_jstring(env, description);
    jobject obj = env->NewObject(cls, ctor, n, static_cast<jint>(words), desc);
    env->DeleteLocalRef(n);
    env->DeleteLocalRef(desc);
    return obj;
}

} // namespace unidict_jni
