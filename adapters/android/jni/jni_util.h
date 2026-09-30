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

} // namespace unidict_jni
