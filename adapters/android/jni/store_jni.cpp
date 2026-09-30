// store 域：生词本/历史（mobile_plan §4 域拆分）。
// M1 面只开词表与历史（标签/笔记/CSV 导出随 M3 全量上壳）；文件随
// app 数据目录，storeOpen 即 load，saveStore 显式落盘。
#include <jni.h>

#include <ctime>

#include "jni_util.h"
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::VocabItemStd;
using unidict_jni::handle_of;
using unidict_jni::to_handle;
using unidict_jni::to_jstring_array;
using unidict_jni::to_std;

extern "C" {

JNIEXPORT jlong JNICALL
Java_dev_unidict_mobile_UnidictCore_storeOpen(JNIEnv* env, jclass,
                                              jstring path) {
    auto* ds = new DataStoreStd();
    ds->set_storage_path(to_std(env, path));
    ds->load();
    return to_handle(ds);
}

JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_storeClose(JNIEnv*, jclass, jlong h) {
    delete handle_of<DataStoreStd>(h);
}

JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_addVocab(JNIEnv* env, jclass, jlong h,
                                             jstring word, jstring definition) {
    VocabItemStd item;
    item.word = to_std(env, word);
    item.definition = to_std(env, definition);
    item.added_at = static_cast<long long>(::time(nullptr));
    handle_of<DataStoreStd>(h)->add_vocabulary_item(item);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_vocabWords(JNIEnv* env, jclass, jlong h) {
    const auto vocab = handle_of<DataStoreStd>(h)->get_vocabulary();
    std::vector<std::string> words;
    words.reserve(vocab.size());
    for (const auto& v : vocab) words.push_back(v.word);
    return to_jstring_array(env, words);
}

JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_addHistory(JNIEnv* env, jclass, jlong h,
                                               jstring word) {
    handle_of<DataStoreStd>(h)->add_search_history(to_std(env, word));
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_history(JNIEnv* env, jclass, jlong h,
                                            jint limit) {
    return to_jstring_array(
        env, handle_of<DataStoreStd>(h)->get_search_history(limit));
}

JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_saveStore(JNIEnv*, jclass, jlong h) {
    return handle_of<DataStoreStd>(h)->save() ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
