// lookup 域：精确/前缀/模糊/全文/聚合/词条定义（mobile_plan §4 域拆分）。
// M2 起：聚合面走 SearchHit data class（来源词典+词+释义）；释义经
// HtmlRendererStd::extract_text 净化为纯文本（首期纯文本渲染口径，
// WebView 净化流留后批评估）。词表类（建议/联想）沿用 M1 平铺 Array。
#include <jni.h>

#include <tuple>

#include "jni_util.h"
#include "std/dictionary_manager_std.h"
#include "std/html_renderer_std.h"

using UnidictCoreStd::DictionaryManagerStd;
using UnidictCoreStd::HtmlRendererStd;
using unidict_jni::handle_of;
using unidict_jni::to_jstring;
using unidict_jni::to_jstring_array;
using unidict_jni::to_search_hit_array;
using unidict_jni::to_std;

namespace {

// HTML 释义 → 纯文本（mdx/dsl 词条是 HTML；json/csv 本就纯文本，
// extract_text 对无标签输入原样通过、顺带解码实体）
std::string plain_text(const std::string& definition) {
    static const HtmlRendererStd renderer;
    return renderer.extract_text(definition);
}

} // namespace

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
Java_dev_unidict_mobile_UnidictCore_searchAll(JNIEnv* env, jclass, jlong h,
                                              jstring word, jint limit) {
    const auto entries =
        handle_of<DictionaryManagerStd>(h)->search_all(to_std(env, word));
    std::vector<std::tuple<std::string, std::string, std::string>> hits;
    hits.reserve(entries.size());
    for (const auto& e : entries) {
        if (static_cast<jint>(hits.size()) >= limit) break;
        hits.emplace_back(e.dict_name, e.word, plain_text(e.definition));
    }
    return to_search_hit_array(env, hits);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_fullTextSearchEntries(JNIEnv* env, jclass,
                                                          jlong h,
                                                          jstring query,
                                                          jint limit) {
    const auto entries = handle_of<DictionaryManagerStd>(h)->full_text_search(
        to_std(env, query), limit);
    std::vector<std::tuple<std::string, std::string, std::string>> hits;
    hits.reserve(entries.size());
    for (const auto& e : entries) {
        hits.emplace_back(e.dict_name, e.word, plain_text(e.definition));
    }
    return to_search_hit_array(env, hits);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_fullTextSearch(JNIEnv* env, jclass,
                                                   jlong h, jstring query,
                                                   jint limit) {
    // 全文词表（联想/计数用）；结构化条目走 fullTextSearchEntries
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
    return to_jstring(env, plain_text(
        handle_of<DictionaryManagerStd>(h)->search_word(to_std(env, word))));
}

} // extern "C"
