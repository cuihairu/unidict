// store 域：生词本/历史（mobile_plan §4 域拆分）。
// M1 面只开词表与历史；M3-B 补生词本全量——标签单条增删、按标签筛选、
// 笔记写入、CSV 导出（core/std DataStoreStd 的 M3 口径，语义与桌面同源）。
// 文件随 app 数据目录，storeOpen 即 load，saveStore 显式落盘。
#include <jni.h>

#include <ctime>
#include <tuple>

#include "jni_util.h"
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::VocabItemStd;
using unidict_jni::handle_of;
using unidict_jni::to_handle;
using unidict_jni::to_jstring_array;
using unidict_jni::to_std;
using unidict_jni::to_vocab_item_array;

namespace {

// 标签数组 → ';' 平铺（core 约定标签不含分号，CSV 同口径）
std::string join_tags(const std::vector<std::string>& tags) {
    std::string out;
    for (const auto& t : tags) {
        if (!out.empty()) out += ';';
        out += t;
    }
    return out;
}

// 词条数组 → 平铺元组（词/释义/标签/笔记；笔记按词大小写不敏感联查）
std::vector<std::tuple<std::string, std::string, std::string, std::string>>
flatten_items(const DataStoreStd& ds,
              const std::vector<VocabItemStd>& items) {
    std::vector<std::tuple<std::string, std::string, std::string, std::string>>
        out;
    out.reserve(items.size());
    for (const auto& v : items) {
        out.emplace_back(v.word, v.definition, join_tags(v.tags),
                         ds.get_note(v.word));
    }
    return out;
}

} // namespace

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

// —— M3-B 生词本全量（标签/笔记/CSV，core DataStoreStd M3 口径） ——

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_vocabItems(JNIEnv* env, jclass, jlong h) {
    const auto* ds = handle_of<DataStoreStd>(h);
    return to_vocab_item_array(env, flatten_items(*ds, ds->get_vocabulary()));
}

JNIEXPORT jobjectArray JNICALL
Java_dev_unidict_mobile_UnidictCore_vocabByTag(JNIEnv* env, jclass, jlong h,
                                               jstring tag) {
    const auto* ds = handle_of<DataStoreStd>(h);
    return to_vocab_item_array(
        env, flatten_items(*ds, ds->get_vocabulary_by_tag(to_std(env, tag))));
}

JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_addVocabTag(JNIEnv* env, jclass, jlong h,
                                                jstring word, jstring tag) {
    return handle_of<DataStoreStd>(h)->add_vocabulary_item_tag(
               to_std(env, word), to_std(env, tag))
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_removeVocabTag(JNIEnv* env, jclass,
                                                   jlong h, jstring word,
                                                   jstring tag) {
    return handle_of<DataStoreStd>(h)->remove_vocabulary_item_tag(
               to_std(env, word), to_std(env, tag))
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_removeVocab(JNIEnv* env, jclass, jlong h,
                                                jstring word) {
    handle_of<DataStoreStd>(h)->remove_vocabulary_item(to_std(env, word));
}

// 笔记 upsert（空串 = 移除该词笔记，core 同语义）
JNIEXPORT void JNICALL
Java_dev_unidict_mobile_UnidictCore_setVocabNote(JNIEnv* env, jclass, jlong h,
                                                 jstring word,
                                                 jstring text) {
    handle_of<DataStoreStd>(h)->set_note(to_std(env, word),
                                         to_std(env, text));
}

// CSV 导出：UTF-8 BOM + word,definition,tags,note 四列（M3 口径）。
// 路径由壳层给（app 私有目录直落 / SAF 目标先落盘再拷贝）。
JNIEXPORT jboolean JNICALL
Java_dev_unidict_mobile_UnidictCore_exportVocabCsv(JNIEnv* env, jclass,
                                                   jlong h, jstring path) {
    return handle_of<DataStoreStd>(h)->export_vocabulary_csv(to_std(env, path))
               ? JNI_TRUE
               : JNI_FALSE;
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
