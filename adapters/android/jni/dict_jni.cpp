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

// 逐词典装载（SAF 导入路径）：返回 add_dictionary 成败，仓库层据此
// 给清单条目标「装载失败」。与 dictOpen(emptyArray) + dictRebuildIndex
// 组合使用（M2 DictRepository.rebuild 流程）。
JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_dictAdd(JNIEnv* env, jclass, jlong h,
                                            jstring path) {
    return handle_of<DictionaryManagerStd>(h)->add_dictionary(to_std(env, path))
               ? JNI_TRUE
               : JNI_FALSE;
}

// 索引重建：逐个 dictAdd 之后统一建（前缀/模糊/联想的前提）
JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_dictRebuildIndex(JNIEnv*, jclass,
                                                     jlong h) {
    handle_of<DictionaryManagerStd>(h)->build_index();
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

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_dictionariesMeta(JNIEnv* env, jclass,
                                                     jlong h) {
    const auto metas =
        handle_of<DictionaryManagerStd>(h)->dictionaries_meta();
    const jclass cls = env->FindClass("dev/unidict/mobile/DictMetaInfo");
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(metas.size()),
                                           cls, nullptr);
    for (jsize i = 0; i < static_cast<jsize>(metas.size()); ++i) {
        jobject m = unidict_jni::new_dict_meta(
            env, metas[static_cast<size_t>(i)].name,
            metas[static_cast<size_t>(i)].word_count,
            metas[static_cast<size_t>(i)].description);
        env->SetObjectArrayElement(arr, i, m);
        env->DeleteLocalRef(m);
    }
    return arr;
}

JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_isDictEnabled(JNIEnv* env, jclass,
                                                  jlong h, jstring name) {
    return handle_of<DictionaryManagerStd>(h)->is_dictionary_enabled(
               to_std(env, name))
               ? JNI_TRUE
               : JNI_FALSE;
}

} // extern "C"
