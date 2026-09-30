// lookup 域：精确/前缀/模糊/全文/词条定义（mobile_plan §4 域拆分）。
// M1 面保持最薄：列表类一律 UTF-8 词表一次性转 Array；全文与聚合的
// 结构化条目（word/definition/来源分组）留 M2 上 data class。
#include <jni.h>

#include "jni_util.h"
#include "std/dictionary_manager_std.h"

using UnidictCoreStd::DictionaryManagerStd;
using unidict_jni::handle_of;
using unidict_jni::to_jstring;
using unidict_jni::to_jstring_array;
using unidict_jni::to_std;

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_exactSearch(JNIEnv* env, jclass, jlong h,
                                                jstring word) {
    return to_jstring_array(
        env, handle_of<DictionaryManagerStd>(h)->exact_search(to_std(env, word)));
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_prefixSearch(JNIEnv* env, jclass, jlong h,
                                                 jstring prefix, jint limit) {
    return to_jstring_array(env, handle_of<DictionaryManagerStd>(h)->prefix_search(
                                     to_std(env, prefix), limit));
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_fuzzySearch(JNIEnv* env, jclass, jlong h,
                                                jstring word, jint limit) {
    return to_jstring_array(env, handle_of<DictionaryManagerStd>(h)->fuzzy_search(
                                     to_std(env, word), limit));
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_fullTextSearch(JNIEnv* env, jclass,
                                                   jlong h, jstring query,
                                                   jint limit) {
    // 全文命中先给词表（M2 换结构化条目）；词序即相关序
    const auto hits = handle_of<DictionaryManagerStd>(h)->full_text_search(
        to_std(env, query), limit);
    std::vector<std::string> words;
    words.reserve(hits.size());
    for (const auto& e : hits) words.push_back(e.word);
    return to_jstring_array(env, words);
}

JNIEXPORT jstring JNICALL
Java_dev_unidict_mobile_UnidictCore_searchDefinition(JNIEnv* env, jclass,
                                                     jlong h, jstring word) {
    return to_jstring(env, handle_of<DictionaryManagerStd>(h)->search_word(
                               to_std(env, word)));
}

} // extern "C"
