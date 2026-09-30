// dict 域：词典装载/启停/元信息（mobile_plan §4 域拆分）。
// 句柄 = DictionaryManagerStd*，dictOpen 时即 build_index（前缀/模糊等
// 索引检索的前提，M0 模拟器冒烟同口径）。
#include <jni.h>

#include "jni_util.h"
#include "std/dictionary_manager_std.h"

using UnidictCoreStd::DictionaryManagerStd;
using unidict_jni::handle_of;
using unidict_jni::to_handle;
using unidict_jni::to_jstring_array;
using unidict_jni::to_std;

extern "C" {

JNIEXPORT jlong JNICALL
Java_dev_unidict_mobile_UnidictCore_dictOpen(JNIEnv* env, jclass,
                                             jobjectArray paths) {
    auto* dm = new DictionaryManagerStd();
    const jsize n = env->GetArrayLength(paths);
    for (jsize i = 0; i < n; ++i) {
        jstring s = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
        dm->add_dictionary(to_std(env, s));  // 失败路径由 dictNames/词量暴露
        env->DeleteLocalRef(s);
    }
    dm->build_index();
    return to_handle(dm);
}

JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_dictClose(JNIEnv*, jclass, jlong h) {
    delete handle_of<DictionaryManagerStd>(h);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_dictNames(JNIEnv* env, jclass, jlong h) {
    return to_jstring_array(
        env, handle_of<DictionaryManagerStd>(h)->loaded_dictionaries());
}

JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_setDictEnabled(JNIEnv* env, jclass,
                                                   jlong h, jstring name,
                                                   jboolean enabled) {
    return handle_of<DictionaryManagerStd>(h)->set_dictionary_enabled(
               to_std(env, name), enabled == JNI_TRUE)
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_dev_unidict_mobile_UnidictCore_indexedWordCount(JNIEnv*, jclass,
                                                     jlong h) {
    return handle_of<DictionaryManagerStd>(h)->indexed_word_count();
}

} // extern "C"
