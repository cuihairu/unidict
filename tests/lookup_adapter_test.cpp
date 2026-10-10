#include <QtTest>

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QWindow>
#include <QCursor>

#include "qmlui/lookup_adapter.h"
#include "qmlui/clipboard_monitor.h"
#include "qmlui/selection_monitor.h"
#include "qmlui/global_hotkeys.h"
#include "core/unidict_core.h"
#include "core/data_store.h"
#include "path_utils.h"
#include "mdict_fixture.h"

using namespace UnidictCore;

class LookupAdapterTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanup();
    void mdict_password_env_roundtrip();
    void reload_replaces_dictionaries_from_env();
    void multiple_paths_split_by_platform_separator();
    // Q-4 覆盖补齐
    void lookup_history_and_vocab_roundtrip();
    void strip_html_for_storage_variants();
    void vocab_export_success_and_failure();
    void vocab_tags_notes_m3();
    void vocab_skill_forwarding_p7();
    void search_wrappers_hit_miss_and_garbage();
    // P-11 批三：miss 建议行升编辑距离（模糊优先 + 前缀补位）
    void miss_suggests_fuzzy_corrections();
    void dictionaries_meta_and_category();
    void reload_empty_env_bumps_stamp();
    void autoplay_lookup_direct_and_delayed();
    void aggregate_lookup_variants();
    void aggregate_lookup_fulltext_fallback_gui_path();
    // BUG-009 增强：卡头音标提取——英/美直通、中文词头拼音回退、无数据空
    void phonetics_extraction_variants();
    void navigation_round_trip();
    void clipboard_signal_forwarding_and_settings();
    void selection_signal_forwarding_and_settings();
    void hotkey_signal_forwarding_and_settings();
    void tts_wrappers_presets_and_info();
    void mdd_remount_after_file_swap();
    void cache_dir_path_caliber();
    // P-6 导入管道：扫描→分块装载→终局建索引发 stamp；取消与重复启动
    void directory_import_pipeline();

private:
    static QString writeJsonDict(const QString& dirPath,
                                 const QString& fileName,
                                 const QString& dictName,
                                 const QList<QPair<QString, QString>>& entries);
    static QVariantMap vocabEntry(const QVariantList& vocab, const QString& word);
    void clearStore();

    QTemporaryDir m_storeDir;  // DataStore 重定向到这里，别写进仓库 CWD
};

void LookupAdapterTest::initTestCase() {
    QVERIFY(m_storeDir.isValid());
    DataStore::instance().setStoragePath(
        QDir(m_storeDir.path()).filePath(QStringLiteral("store.json")));
    // .mdd 解出的资源缓存在 CacheLocation 下——本二进制独占（app 名即
    // test_lookup_adapter），开头清一次，避免上轮遗留的解包文件影响断言
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .removeRecursively();
}

void LookupAdapterTest::init() {
    // P-6 切 std 后每个 adapter 自持词典管理器，无跨实例共享态可清
    qunsetenv("UNIDICT_DICTS");
    qunsetenv("UNIDICT_MDICT_PASSWORD");
    qunsetenv("UNIDICT_PASSWORD");
}

void LookupAdapterTest::cleanup() {
    qunsetenv("UNIDICT_DICTS");
    qunsetenv("UNIDICT_MDICT_PASSWORD");
    qunsetenv("UNIDICT_PASSWORD");
}

void LookupAdapterTest::mdict_password_env_roundtrip() {
    LookupAdapter adapter;

    QVERIFY(!adapter.hasMdictPassword());
    QVERIFY(!adapter.setMdictPassword(QString()));

    QVERIFY(adapter.setMdictPassword("secret"));
    QVERIFY(adapter.hasMdictPassword());
    QCOMPARE(qEnvironmentVariable("UNIDICT_MDICT_PASSWORD"), QString("secret"));
    QVERIFY(qEnvironmentVariable("UNIDICT_PASSWORD").isEmpty());

    qputenv("UNIDICT_PASSWORD", QByteArray("legacy"));
    QVERIFY(adapter.hasMdictPassword());

    adapter.clearMdictPassword();
    QVERIFY(!adapter.hasMdictPassword());
    QVERIFY(qEnvironmentVariable("UNIDICT_MDICT_PASSWORD").isEmpty());
    QVERIFY(qEnvironmentVariable("UNIDICT_PASSWORD").isEmpty());
}

QString LookupAdapterTest::writeJsonDict(const QString& dirPath,
                                         const QString& fileName,
                                         const QString& dictName,
                                         const QList<QPair<QString, QString>>& entries) {
    const QString path = dirPath + "/" + fileName;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {};
    }

    QByteArray json;
    json += "{\n";
    json += "  \"name\": \"" + dictName.toUtf8() + "\",\n";
    json += "  \"description\": \"test dictionary\",\n";
    json += "  \"entries\": [\n";
    for (int i = 0; i < entries.size(); ++i) {
        const auto& entry = entries.at(i);
        json += "    {\"word\":\"" + entry.first.toUtf8() + "\",\"definition\":\"" + entry.second.toUtf8() + "\"}";
        if (i + 1 < entries.size()) {
            json += ",";
        }
        json += "\n";
    }
    json += "  ]\n";
    json += "}\n";

    file.write(json);
    file.close();
    return path;
}

// vocabulary()/vocabularyMeta() 返回 QVariantList<{word,...}>——按词取一条，
// 找不到返回带哨兵键的 map，让调用方 QVERIFY 直接可见地失败而不是静默越界
QVariantMap LookupAdapterTest::vocabEntry(const QVariantList& vocab, const QString& word) {
    for (const QVariant& v : vocab) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("word")).toString() == word) {
            return m;
        }
    }
    return {};
}

void LookupAdapterTest::clearStore() {
    DataStore::instance().clearHistory();
    DataStore::instance().clearVocabulary();
}

void LookupAdapterTest::reload_replaces_dictionaries_from_env() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dictA = writeJsonDict(
        tempDir.path(),
        "dict_a.json",
        "Dict A",
        {{"hello", "from dict a"}, {"alpha", "entry a"}});
    const QString dictB = writeJsonDict(
        tempDir.path(),
        "dict_b.json",
        "Dict B",
        {{"world", "from dict b"}, {"beta", "entry b"}});

    QVERIFY(!dictA.isEmpty());
    QVERIFY(!dictB.isEmpty());

    LookupAdapter adapter;
    QSignalSpy stampSpy(&adapter, &LookupAdapter::dictionariesStampChanged);

    qputenv("UNIDICT_DICTS", dictA.toUtf8());
    QVERIFY(adapter.loadDictionariesFromEnv());
    QCOMPARE(adapter.loadedDictionaries(), (QStringList{"Dict A"}));
    QCOMPARE(adapter.lookupDefinition("hello"), QString("from dict a"));
    QVERIFY(adapter.lookupDefinition("world").startsWith("Word not found"));

    qputenv("UNIDICT_DICTS", dictB.toUtf8());
    QVERIFY(adapter.reloadDictionariesFromEnv());

    QCOMPARE(adapter.loadedDictionaries(), (QStringList{"Dict B"}));
    QCOMPARE(adapter.lookupDefinition("world"), QString("from dict b"));
    QVERIFY(adapter.lookupDefinition("hello").startsWith("Word not found"));
    QCOMPARE(adapter.indexedWordCount(), 2);
    QCOMPARE(adapter.dictionariesStamp(), 2);
    QCOMPARE(stampSpy.count(), 2);
}

// 回归：UNIDICT_DICTS 的多路径分隔符必须跟平台 PATH 惯例（Windows ';',
// POSIX ':'），且不能把另一平台的分隔符也当成分隔符——否则 Windows 的
// 盘符 'C:\...' 自带冒号会被劈碎，字典全部加载失败（曾致 Windows CI 独挂）
void LookupAdapterTest::multiple_paths_split_by_platform_separator() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A", {{"hello", "from dict a"}});
    const QString dictB = writeJsonDict(
        tempDir.path(), "dict_b.json", "Dict B", {{"world", "from dict b"}});
    QVERIFY(!dictA.isEmpty());
    QVERIFY(!dictB.isEmpty());

    LookupAdapter adapter;

    QByteArray joined = dictA.toUtf8();
#if defined(Q_OS_WIN)
    joined += ';';
#else
    joined += ':';
#endif
    joined += dictB.toUtf8();
    qputenv("UNIDICT_DICTS", joined);

    QVERIFY(adapter.loadDictionariesFromEnv());
    QCOMPARE(adapter.loadedDictionaries(), (QStringList{"Dict A", "Dict B"}));
    QCOMPARE(adapter.lookupDefinition("hello"), QString("from dict a"));
    QCOMPARE(adapter.lookupDefinition("world"), QString("from dict b"));
}

// ============================================================================
// Q-4 覆盖补齐
// ============================================================================

// 查词写历史 → searchHistory 读回；生词本增/查/删/清空全链
void LookupAdapterTest::lookup_history_and_vocab_roundtrip() {
    clearStore();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A", {{"hello", "from dict a"}});
    QVERIFY(!dictA.isEmpty());
    qputenv("UNIDICT_DICTS", dictA.toUtf8());

    LookupAdapter adapter;
    QVERIFY(adapter.loadDictionariesFromEnv());

    QCOMPARE(adapter.lookupDefinition("hello"), QString("from dict a"));
    QCOMPARE(adapter.lookupDefinition("hello"), QString("from dict a"));  // 去重
    QStringList history = adapter.searchHistory(10);
    QCOMPARE(history, (QStringList{"hello"}));
    adapter.clearHistory();                       // adapter 自己的清空 wrapper
    QVERIFY(adapter.searchHistory(10).isEmpty());

    adapter.addToVocabulary("hello", "from dict a");
    QVariantList vocab = adapter.vocabulary();
    QCOMPARE(vocab.size(), 1);
    QCOMPARE(vocabEntry(vocab, "hello").value("definition").toString(),
             QString("from dict a"));
    QVariantList meta = adapter.vocabularyMeta();
    QCOMPARE(meta.size(), 1);
    QVERIFY(meta.at(0).toMap().contains(QStringLiteral("added_at")));

    adapter.removeVocabularyWord("hello");
    QVERIFY(adapter.vocabulary().isEmpty());
    adapter.addToVocabulary("temp", "x");
    adapter.clearVocabulary();
    QVERIFY(adapter.vocabulary().isEmpty());
}

// stripHtmlForStorage：纯文本早退、只含 '<' 早退、富文本三段替换 + 去标签
// + &nbsp; + trim——存进生词本的定义必须是清洗后的纯文本
void LookupAdapterTest::strip_html_for_storage_variants() {
    clearStore();
    LookupAdapter adapter;

    adapter.addToVocabulary("plain", "no tags at all");
    adapter.addToVocabulary("lt", "5 < 6");  // 只有 '<' 没有 '>' → 原样
    adapter.addToVocabulary("rich", "<p>hello</p><br>one&nbsp;two<div>x</div>");
    adapter.addToVocabulary("entity", "a &nbsp; b");  // 无标签但有实体 → 早退

    QVariantList vocab = adapter.vocabulary();
    QCOMPARE(vocabEntry(vocab, "plain").value("definition").toString(),
             QString("no tags at all"));
    QCOMPARE(vocabEntry(vocab, "lt").value("definition").toString(),
             QString("5 < 6"));
    // 逐条对照 stripHtmlForStorage 的替换序：<br>→\n、</p>→\n\n、</div>→\n
    // （换行在闭标签处、即内容**之后**）、去标签、&nbsp;→空格、trim。
    QCOMPARE(vocabEntry(vocab, "rich").value("definition").toString(),
             QString("hello\n\n\none twox"));
    QCOMPARE(vocabEntry(vocab, "entity").value("definition").toString(),
             QString("a &nbsp; b"));
}

// exportVocabCsv 成功路径与打开失败路径。失败路径用"父路径是普通文件"
// （ENOTDIR）构造——chmod 方案在 root 下假绿，这里不依赖权限位
void LookupAdapterTest::vocab_export_success_and_failure() {
    clearStore();
    LookupAdapter adapter;
    adapter.addToVocabulary("word,comma", "def with \"quotes\"");
    adapter.addToVocabulary("two", "second");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString good = QDir(tempDir.path()).filePath("vocab.csv");
    QVERIFY(adapter.exportVocabCsv(good));
    QFile f(good);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray csv = f.readAll();
    // M3 口径：UTF-8 BOM + word,definition,tags,note 四列
    QVERIFY(csv.startsWith("\xEF\xBB\xBFword,definition,tags,note\n"));
    QVERIFY(csv.contains("\"word,comma\""));   // CSV 引号包裹
    QVERIFY(csv.contains("\"def with \"\"quotes\"\"\""));  // 引号转义

    QFile blocker(QDir(tempDir.path()).filePath("blocker"));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("x");
    blocker.close();
    QVERIFY(!adapter.exportVocabCsv(
        QDir(tempDir.path()).filePath("blocker/vocab.csv")));

    adapter.clearVocabulary();
    QVERIFY(adapter.vocabulary().isEmpty());
}

// M3-B：标签增删/按标签筛选/笔记经 adapter 的端到端——qmlui 生词本
// 编辑页与 Android 侧共用同一 core 口径，这里守 adapter 转发不跑偏
void LookupAdapterTest::vocab_tags_notes_m3() {
    clearStore();
    LookupAdapter adapter;
    adapter.addToVocabulary("hello", "greeting def");
    adapter.addToVocabulary("world", "earth def");

    QVERIFY(adapter.addVocabTag("hello", "CET4"));
    QVERIFY(adapter.addVocabTag("hello", "CET4"));   // 幂等去重
    QVERIFY(adapter.addVocabTag("hello", "greeting"));
    QVERIFY(!adapter.addVocabTag("hello", ""));      // 空标签拒
    QVERIFY(adapter.addVocabTag("world", "CET4"));

    // meta 携带 tags（生词本卡片 chip 的数据源）
    const QVariantList meta = adapter.vocabularyMeta();
    QCOMPARE(meta.size(), 2);
    const QStringList helloTags =
        meta.at(0).toMap().value("tags").toStringList();
    QVERIFY(helloTags.contains(QStringLiteral("CET4")));
    QVERIFY(helloTags.contains(QStringLiteral("greeting")));

    // 按标签筛选：保持存储序，未命中为空
    const QVariantList byTag = adapter.vocabularyByTag("CET4");
    QCOMPARE(byTag.size(), 2);
    QCOMPARE(byTag.at(0).toMap().value("word").toString(), QString("hello"));
    QCOMPARE(byTag.at(1).toMap().value("word").toString(), QString("world"));
    QVERIFY(adapter.vocabularyByTag("nope").isEmpty());

    // 删标签：双命中才真，二次删不重复命中
    QVERIFY(adapter.removeVocabTag("hello", "greeting"));
    QVERIFY(!adapter.removeVocabTag("hello", "greeting"));
    QVERIFY(!adapter.removeVocabTag("hello", "nope"));
    QCOMPARE(adapter.vocabularyByTag("greeting").size(), 0);

    // 笔记 roundtrip：写入/覆盖/空串即删
    adapter.setVocabNote("hello", "m3 note");
    QCOMPARE(adapter.getVocabNote("hello"), QString("m3 note"));
    adapter.setVocabNote("hello", "m3 note v2");
    QCOMPARE(adapter.getVocabNote("hello"), QString("m3 note v2"));
    adapter.setVocabNote("hello", "");
    QVERIFY(adapter.getVocabNote("hello").isEmpty());
    QVERIFY(adapter.getVocabNote("world").isEmpty());  // 词间互不串

    adapter.clearVocabulary();
    QVERIFY(adapter.vocabulary().isEmpty());
}

// P-7 批四 复习入口写入口的转发 pin：四技能标记经 adapter 落 DataStore
// （skill ∈ {listen,speak,read,write} 大小写不敏感、level 0-2，非法参数/
// 词未命中返回假不动数据），meta 条目带四技能键供复习清单过滤
void LookupAdapterTest::vocab_skill_forwarding_p7() {
    clearStore();
    LookupAdapter adapter;
    adapter.addToVocabulary("hello", "greeting def");

    // 合法写：三档口径（0=未练 1=不稳 2=稳）
    QVERIFY(adapter.setVocabularySkill("hello", "listen", 1));
    QVERIFY(adapter.setVocabularySkill("hello", "SPEAK", 2));  // 大小写不敏感
    QVERIFY(adapter.setVocabularySkill("hello", "read", 0));
    QVERIFY(adapter.setVocabularySkill("hello", "Write", 1));

    const QVariantMap m = adapter.vocabularyMeta().first().toMap();
    QCOMPARE(m.value("listen").toInt(), 1);
    QCOMPARE(m.value("speak").toInt(), 2);
    QCOMPARE(m.value("read").toInt(), 0);
    QCOMPARE(m.value("write").toInt(), 1);

    // 非法参数矩阵：假不动数据（不静默钳位）
    QVERIFY(!adapter.setVocabularySkill("hello", "hear", 1));   // 未知技能
    QVERIFY(!adapter.setVocabularySkill("hello", "listen", 3)); // 越界档位
    QVERIFY(!adapter.setVocabularySkill("hello", "listen", -1));
    QVERIFY(!adapter.setVocabularySkill("nope", "listen", 1));  // 词未命中
    const QVariantMap m2 = adapter.vocabularyMeta().first().toMap();
    QCOMPARE(m2.value("listen").toInt(), 1);
    QCOMPARE(m2.value("write").toInt(), 1);

    adapter.clearVocabulary();
    QVERIFY(adapter.vocabulary().isEmpty());
}

// suggestPrefix/suggestFuzzy/searchWildcard/searchRegex 的命中、落空与
// 垃圾输入分支：prefix=前缀索引；fuzzy=编辑距离 ≤2 近邻（P-6 切 std 后
// 的语义升级：legacy 实为前缀匹配）；wildcard=全词通配（trim 后空先拒）；
// regex=大小写不敏感、非法正则空手而回
void LookupAdapterTest::search_wrappers_hit_miss_and_garbage() {
    clearStore();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A",
        {{"hello", "greeting"}, {"world", "planet"}});
    QVERIFY(!dictA.isEmpty());

    LookupAdapter adapter;
    qputenv("UNIDICT_DICTS", dictA.toUtf8());
    QVERIFY(adapter.loadDictionariesFromEnv());

    QCOMPARE(adapter.suggestPrefix("hel", 10), (QStringList{"hello"}));
    QVERIFY(adapter.suggestPrefix("zzz", 10).isEmpty());
    QCOMPARE(adapter.suggestFuzzy("hel", 10), (QStringList{"hello"}));
    // "helo"→"hello" 编辑距离 1：legacy（前缀匹配）落空，std 命中——
    // 语义升级钉在这里
    QCOMPARE(adapter.suggestFuzzy("helo", 10), (QStringList{"hello"}));

    QCOMPARE(adapter.searchWildcard("hello", 10), (QStringList{"hello"}));
    QVERIFY(adapter.searchWildcard("*notthere*", 10).isEmpty());
    QVERIFY(adapter.searchWildcard("   ", 10).isEmpty());         // trim 后为空

    QCOMPARE(adapter.searchRegex("h.*o", 10), (QStringList{"hello"}));
    QVERIFY(adapter.searchRegex("(", 10).isEmpty());  // 非法正则 → 空

    QCOMPARE(adapter.lookupDefinition("hello"), QString("greeting"));
    QVERIFY(adapter.lookupDefinition("nope").startsWith("Word not found"));
    QVERIFY(adapter.searchHistory(10).contains(QStringLiteral("hello")));
    QVERIFY(!adapter.searchHistory(10).contains(QStringLiteral("nope")));
}

// P-11 批三：miss 建议行升编辑距离——lookupDefinition 全 miss 时建议
// 由 suggest_corrections 出（模糊优先 + 前缀补位），文案格式不变
void LookupAdapterTest::miss_suggests_fuzzy_corrections() {
    clearStore();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A",
        {{"hello", "greeting"}, {"help", "assist"}});
    QVERIFY(!dictA.isEmpty());

    LookupAdapter adapter;
    qputenv("UNIDICT_DICTS", dictA.toUtf8());
    QVERIFY(adapter.loadDictionariesFromEnv());

    // "helo"：五层全 miss（词形还原无臂/非前缀/释义不含）→ 建议行
    // 列 hello/help（各距 1，同分按词头字典序），格式与 legacy 逐字节同形
    const QString def = adapter.lookupDefinition("helo");
    QVERIFY(def.startsWith(QStringLiteral("Word not found")));
    QVERIFY(def.contains(QStringLiteral("Did you mean:")));
    QVERIFY(def.contains(QStringLiteral("hello")));
    QVERIFY(def.contains(QStringLiteral("help")));
    // 无近邻词：无建议行，裸 miss 文案
    const QString bare = adapter.lookupDefinition("zzzzz");
    QCOMPARE(bare, QStringLiteral("Word not found: zzzzz"));
}

// dictionariesMeta 的逐条映射、getDictionariesByCategory、P-6 接真的
// 优先级/启停 setter（变更发 stamp，启停只影响查询路径）
void LookupAdapterTest::dictionaries_meta_and_category() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A",
        {{"hello", "greeting"}, {"world", "planet"}});
    QVERIFY(!dictA.isEmpty());
    qputenv("UNIDICT_DICTS", dictA.toUtf8());

    LookupAdapter adapter;
    QVERIFY(adapter.loadDictionariesFromEnv());

    QVariantList metas = adapter.dictionariesMeta();
    QCOMPARE(metas.size(), 1);
    QCOMPARE(metas.at(0).toMap().value("name").toString(), QString("Dict A"));
    QCOMPARE(metas.at(0).toMap().value("wordCount").toInt(), 2);
    QCOMPARE(metas.at(0).toMap().value("description").toString(),
             QString("test dictionary"));

    QVariantList byCat = adapter.getDictionariesByCategory("english");
    QCOMPARE(byCat.size(), 1);
    QCOMPARE(byCat.at(0).toMap().value("id").toString(), QString("Dict A"));
    QCOMPARE(byCat.at(0).toMap().value("category").toString(), QString("english"));

    QSignalSpy stampSpy(&adapter, &LookupAdapter::dictionariesStampChanged);
    adapter.setDictionaryPriority("Dict A", 5);
    adapter.setDictionaryEnabled("Dict A", false);
    QCOMPARE(stampSpy.count(), 2);
    // 禁用词典仍在装载清单（管理页要能看到禁用态），但不再参与查询
    QCOMPARE(adapter.loadedDictionaries(), (QStringList{"Dict A"}));
    QVERIFY(adapter.aggregateLookup("hello", QVariantMap()).isEmpty());
    adapter.setDictionaryEnabled("Dict A", true);
    QCOMPARE(adapter.aggregateLookup("hello", QVariantMap()).size(), 1);
    adapter.setDictionaryEnabled("ghost", false);  // 未知名：不变更不发电
    QCOMPARE(stampSpy.count(), 3);
}

// 环境未设 UNIDICT_DICTS 时的 reload：清空 + 发 stamp，返回 false
void LookupAdapterTest::reload_empty_env_bumps_stamp() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A", {{"hello", "greeting"}});
    QVERIFY(!dictA.isEmpty());

    LookupAdapter adapter;
    qputenv("UNIDICT_DICTS", dictA.toUtf8());
    QVERIFY(adapter.loadDictionariesFromEnv());
    QCOMPARE(adapter.loadedDictionaries().size(), 1);

    qunsetenv("UNIDICT_DICTS");
    QSignalSpy stampSpy(&adapter, &LookupAdapter::dictionariesStampChanged);
    QVERIFY(!adapter.reloadDictionariesFromEnv());
    QCOMPARE(stampSpy.count(), 1);
    QCOMPARE(adapter.dictionariesStamp(), 2);
    QVERIFY(adapter.loadedDictionaries().isEmpty());
    QVERIFY(!adapter.loadDictionariesFromEnv());  // env 为空 → false 分支
}

// 自动朗读：命中词 + 延迟 0（同步说）与延迟 >0（singleShot 里说）两条
// 路径，外加未命中不触发。引擎是否真出声与断言无关（headless 无语音
// 后端，say 是安全的空操作），这里断言的是查词结果与开关状态本身。
void LookupAdapterTest::autoplay_lookup_direct_and_delayed() {
    clearStore();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A", {{"hello", "greeting"}});
    QVERIFY(!dictA.isEmpty());
    qputenv("UNIDICT_DICTS", dictA.toUtf8());

    LookupAdapter adapter;
    QVERIFY(adapter.loadDictionariesFromEnv());
    QVERIFY(!adapter.isAutoPlayEnabled());          // 默认关
    QCOMPARE(adapter.getAutoPlayDelay(), 1000);     // 默认 1s

    adapter.setAutoPlayEnabled(true);
    QVERIFY(adapter.isAutoPlayEnabled());
    adapter.setAutoPlayDelay(-5);                   // qMax(0,·) 夹到 0
    QCOMPARE(adapter.getAutoPlayDelay(), 0);
    QCOMPARE(adapter.lookupDefinition("hello"), QString("greeting"));  // 同步支

    adapter.setAutoPlayDelay(30);
    QCOMPARE(adapter.getAutoPlayDelay(), 30);
    QCOMPARE(adapter.lookupDefinition("hello"), QString("greeting"));  // 定时支
    QTest::qWait(200);                              // 让 singleShot lambda 触发

    QVERIFY(adapter.lookupDefinition("nothere").startsWith("Word not found"));
    adapter.setAutoPlayEnabled(false);              // 关闭支（未命中/关都跳过）
    QCOMPARE(adapter.lookupDefinition("hello"), QString("greeting"));
}

// aggregateLookup：双词典聚合、maxTotalResults 截断、清洗/链接重写的
// 开关组合、空结果不写历史，以及结果非空时的自动朗读两支
void LookupAdapterTest::aggregate_lookup_variants() {
    clearStore();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString rich = "<script>alert(1)</script>see entry://alpha";
    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A", {{"hello", rich}});
    const QString dictB = writeJsonDict(
        tempDir.path(), "dict_b.json", "Dict B", {{"hello", "plain b"}});
    QVERIFY(!dictA.isEmpty() && !dictB.isEmpty());
    qputenv("UNIDICT_DICTS",
            (dictA + QDir::listSeparator() + dictB).toUtf8());

    LookupAdapter adapter;
    QVERIFY(adapter.loadDictionariesFromEnv());

    // BUG-009 分组形态：[{dictionary, dictionaryId, entries:[…]}]。断言走
    // 摊平视图（组结构另有 core_lookup 的 searchGroupedTiersAndDedup 钉）
    QVariantList all = adapter.aggregateLookup("hello", QVariantMap());
    QCOMPARE(all.size(), 2);  // 双词典 → 两个分组
    QVariantList flat;
    for (const QVariant& v : all) {
        const QVariantMap g = v.toMap();
        QVERIFY(!g.value("dictionary").toString().isEmpty());
        const QVariantList es = g.value("entries").toList();
        QCOMPARE(es.size(), 1);
        flat.append(es);
    }
    QCOMPARE(flat.size(), 2);
    for (const QVariant& v : flat) {
        const QVariantMap e = v.toMap();
        QCOMPARE(e.value("word").toString(), QString("hello"));
        QVERIFY(e.contains(QStringLiteral("relevance")));
        QVERIFY(!e.value("dictionary").toString().isEmpty());
        const QString def = e.value("definition").toString();
        QVERIFY2(!def.contains("<script"), qPrintable(def));  // 默认清洗开
    }
    const QVariantMap a = flat.at(0).toMap();
    if (a.value("dictionary").toString() == QStringLiteral("Dict A")) {
        QVERIFY(a.value("definition").toString().contains("unidict://lookup?word=alpha"));
    } else {
        QVERIFY(flat.at(1).toMap().value("definition").toString()
                    .contains("unidict://lookup?word=alpha"));
    }

    QVariantMap opts;
    opts.insert("maxTotalResults", 1);
    const QVariantList capped = adapter.aggregateLookup("hello", opts);
    QCOMPARE(capped.size(), 1);  // 满额即止：只剩首个分组
    QCOMPARE(capped.first().toMap().value("entries").toList().size(), 1);

    QVariantMap raw;
    raw.insert("sanitizeHtml", false);
    raw.insert("rewriteCrossRefs", false);
    QVariantList rawRes = adapter.aggregateLookup("hello", raw);
    bool sawRawScript = false;
    for (const QVariant& v : rawRes) {
        for (const QVariant& ev : v.toMap().value("entries").toList()) {
            if (ev.toMap().value("definition").toString().contains("<script")) {
                sawRawScript = true;
            }
        }
    }
    QVERIFY(sawRawScript);  // 关清洗后原文透传

    QVERIFY(adapter.aggregateLookup("nothere", QVariantMap()).isEmpty());

    adapter.setAutoPlayEnabled(true);
    adapter.setAutoPlayDelay(0);
    QCOMPARE(adapter.aggregateLookup("hello", QVariantMap()).size(), 2);  // 同步支
    adapter.setAutoPlayDelay(30);
    adapter.aggregateLookup("hello", QVariantMap());                      // 定时支
    QTest::qWait(200);
    adapter.setAutoPlayEnabled(false);
}

// BUG-005 的 GUI 口径：MainDesktop.qml 查词走 aggregateLookup，顶栏计数走
// indexedWordCount。此处按随包 CC-CEDICT 的形状造词典（词头全汉字、英文只在
// 释义里），断言「计数与可查一致」在 GUI 这一层同样成立：大写形态命中词头、
// 纯英文词走释义全文兜底且带 matchType=fulltext、词头命中不掺兜底。
void LookupAdapterTest::aggregate_lookup_fulltext_fallback_gui_path() {
    clearStore();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dict = writeJsonDict(tempDir.path(), "cedict_shape.json", "CC-CEDICT 形状", {
        {"你好", "[ni3 hao3] hello; hi"},
        {"好", "[hao3] good; fine"},
        {"计算机", "[ji4 suan4 ji1] computer"},
    });
    QVERIFY(!dict.isEmpty());
    qputenv("UNIDICT_DICTS", dict.toUtf8());

    LookupAdapter adapter;
    QVERIFY(adapter.loadDictionariesFromEnv());

    // 顶栏口径：3 个词头 → 计数 3（用户现场是 125170，形状一致）
    QCOMPARE(adapter.indexedWordCount(), 3);

    // 词头命中（原样 + 大写形态，都不掺全文兜底）；分组形态下取组内首条
    for (const QString& q : {QStringLiteral("你好"), QStringLiteral("计算机")}) {
        const QVariantList hits = adapter.aggregateLookup(q, QVariantMap());
        QCOMPARE(hits.size(), 1);  // 单词典 → 单分组
        const QVariantMap entry = hits.first()
                                      .toMap()
                                      .value(QStringLiteral("entries"))
                                      .toList()
                                      .first()
                                      .toMap();
        QCOMPARE(entry.value("word").toString(), q);
        QVERIFY(entry.value("metadata").toMap()
                    .value("matchType").toString().isEmpty());
    }
    QCOMPARE(adapter.aggregateLookup("HELLO", QVariantMap()).size(), 1);

    // 纯英文词不在词头索引里 → 释义全文兜底，且标注 matchType=fulltext
    const QVariantList good = adapter.aggregateLookup("good", QVariantMap());
    QVERIFY(!good.isEmpty());
    const QVariantMap goodEntry = good.first()
                                      .toMap()
                                      .value(QStringLiteral("entries"))
                                      .toList()
                                      .first()
                                      .toMap();
    QCOMPARE(goodEntry.value("metadata").toMap()
                 .value("matchType").toString(), QStringLiteral("fulltext"));

    // 真不存在的词仍为空（兜底不是无脑返回）
    QVERIFY(adapter.aggregateLookup("zzqqxx", QVariantMap()).isEmpty());
}

// 卡头音标（LookupAdapter::extractPhonetics）：英文词走释义开头的
// "英 […] 美 […]" 惯例双字段；中文词头（汉英词典）没有英/美，回退
// 释义开头的 [拼音]（CC-CEDICT 形态）；两者皆无时返回空 map——UI 据
// 此决定显示英/美行、拼 [yin] 行还是裸朗读喇叭
void LookupAdapterTest::phonetics_extraction_variants() {
    LookupAdapter adapter;

    // core 提取器返回的字段不含方括号（"英 [x] 美 [y]" → british=x）
    const QVariantMap both = adapter.extractPhonetics(
        QStringLiteral("英 [həˈləʊ] 美 [həˈloʊ] interjection"));
    QCOMPARE(both.value("british").toString(), QStringLiteral("həˈləʊ"));
    QCOMPARE(both.value("american").toString(), QStringLiteral("həˈloʊ"));
    QVERIFY(!both.contains(QStringLiteral("pinyin")));

    const QVariantMap pinyin = adapter.extractPhonetics(
        QStringLiteral("[shuo1 ming2] to explain; to illustrate"));
    QVERIFY(!pinyin.contains(QStringLiteral("british")));
    QVERIFY(!pinyin.contains(QStringLiteral("american")));
    QCOMPARE(pinyin.value("pinyin").toString(), QStringLiteral("shuo1 ming2"));

    QVERIFY(adapter.extractPhonetics(QStringLiteral("plain text no markers")).isEmpty());
    // 有英/美就不掺拼音（中文括号内容不当拼音误报）
    const QVariantMap englishOnly = adapter.extractPhonetics(
        QStringLiteral("英 [kæt] cat"));
    QCOMPARE(englishOnly.value("british").toString(), QStringLiteral("kæt"));
    QVERIFY(!englishOnly.contains(QStringLiteral("pinyin")));
}

// 前进/后退栈的全套转移：入栈、回退、前进、清空、空栈取回
void LookupAdapterTest::navigation_round_trip() {
    LookupAdapter adapter;

    QVERIFY(!adapter.canGoBack());
    QVERIFY(!adapter.canGoForward());
    QCOMPARE(adapter.navigationHistorySize(), 0);
    QCOMPARE(adapter.goBack(), QString());      // 空栈 → 空
    QCOMPARE(adapter.goForward(), QString());

    adapter.navigateToWord("alpha");
    adapter.navigateToWord("alpha");            // 同词不重复入栈
    QCOMPARE(adapter.navigationHistorySize(), 0);

    adapter.navigateToWord("beta");             // 换词 → alpha 入 back
    QVERIFY(adapter.canGoBack());
    QCOMPARE(adapter.navigationHistorySize(), 1);

    QCOMPARE(adapter.goBack(), QString("alpha"));
    QVERIFY(adapter.canGoForward());
    QCOMPARE(adapter.goForward(), QString("beta"));

    adapter.navigateToWord("gamma");            // 新导航清空 forward
    QVERIFY(!adapter.canGoForward());
    QCOMPARE(adapter.navigationHistorySize(), 2);  // goForward 回填的 alpha + beta
    adapter.clearNavigationHistory();
    QCOMPARE(adapter.navigationHistorySize(), 0);
    QVERIFY(!adapter.canGoBack());
}

// 剪贴板监视器 → adapter 转发：开关关时咽掉，开时发出 clipboardWordDetected。
// 不靠真实剪贴板轮询（时序不定且依赖平台），直接触发监视器信号——被连接
// 的恰是构造函数里装的那条 lambda。设置类 wrapper 顺带全打一遍。
void LookupAdapterTest::clipboard_signal_forwarding_and_settings() {
    LookupAdapter adapter;
    ClipboardMonitor* monitor = adapter.findChild<ClipboardMonitor*>();
    QVERIFY(monitor);

    int fired = 0;
    QString lastWord;
    QObject::connect(&adapter, &LookupAdapter::clipboardWordDetected,
                     [&](const QString& w) { ++fired; lastWord = w; });

    QVERIFY(!adapter.isClipboardAutoLookupEnabled());
    monitor->wordDetected(QStringLiteral("hello"));   // 关 → 咽掉
    QCOMPARE(fired, 0);

    adapter.setClipboardAutoLookupEnabled(true);
    QVERIFY(adapter.isClipboardAutoLookupEnabled());
    monitor->wordDetected(QStringLiteral("world"));   // 开 → 转发
    QCOMPARE(fired, 1);
    QCOMPARE(lastWord, QString("world"));

    adapter.setClipboardPollInterval(120);
    QCOMPARE(monitor->getPollInterval(), 120);
    adapter.setClipboardMinWordLength(3);
    QCOMPARE(monitor->getMinWordLength(), 3);
    adapter.setClipboardMaxWordLength(20);
    QCOMPARE(monitor->getMaxWordLength(), 20);
    adapter.addClipboardExcludePattern(QStringLiteral("password"));
    adapter.clearClipboardExcludePatterns();

    QVERIFY(!adapter.isClipboardMonitoring());
    adapter.startClipboardMonitoring();
    QVERIFY(adapter.isClipboardMonitoring());
    QVERIFY(monitor->isMonitoring());
    adapter.stopClipboardMonitoring();
    QVERIFY(!adapter.isClipboardMonitoring());
}

// 划词取词（X11 PRIMARY selection）→ adapter 转发：信号直发断言转发链；
// 非 xcb（offscreen）平台 isSupported=false 且 start 必须 no-op（不谎报
// 监控中）——这就是 platform gate 的失败分支断言。配置 wrapper 顺带全打。
void LookupAdapterTest::selection_signal_forwarding_and_settings() {
    LookupAdapter adapter;
    SelectionMonitor* monitor = adapter.findChild<SelectionMonitor*>();
    QVERIFY(monitor);

    int fired = 0;
    QString lastWord;
    QObject::connect(&adapter, &LookupAdapter::selectionWordDetected,
                     [&](const QString& w) { ++fired; lastWord = w; });

    // offscreen 无独立 selection：不支持 + start no-op（诚实不假装）
    QVERIFY(!adapter.isSelectionMonitoringSupported());
    adapter.startSelectionMonitoring();
    QVERIFY(!adapter.isSelectionMonitoring());
    monitor->stop();   // 未监控时 stop 幂等不崩

    emit monitor->selectionDetected(QStringLiteral("hello"));
    QCOMPARE(fired, 1);
    QCOMPARE(lastWord, QString("hello"));

    adapter.setSelectionPollInterval(200);
    QCOMPARE(monitor->getPollInterval(), 200);
    adapter.setSelectionMinLength(3);
    QCOMPARE(monitor->getMinLength(), 3);
    adapter.setSelectionMaxLength(60);
    QCOMPARE(monitor->getMaxLength(), 60);
}

// 热键分支（构造函数装的 lambda 的 if/else-if 链）：P-5 收口后
// show_window/quick_lookup 各发只属于自己的信号、lookup_selection 仍
// 是平台化占位分支、unknown 全忽略——四类各触发断言信号计数。再把
// 注册/注销/开关 wrapper 走一遍。Linux 上 isPlatformSupported 为假，
// 注册必须失败——这本身就是失败分支断言。
void LookupAdapterTest::hotkey_signal_forwarding_and_settings() {
    LookupAdapter adapter;
    GlobalHotkeys* hotkeys = adapter.findChild<GlobalHotkeys*>();
    QVERIFY(hotkeys);

    int quickFired = 0, showFired = 0;
    QObject::connect(&adapter, &LookupAdapter::quickLookupRequested,
                     [&] { ++quickFired; });
    QObject::connect(&adapter, &LookupAdapter::showMainWindowRequested,
                     [&] { ++showFired; });

    // Windows 的注册路径要求进程内有顶层窗口可挂热键（无窗口时报
    // "no top-level window to attach hotkey"——生产里 GUI 常驻主窗口
    // 场景成立，测试自己造一个锚定真实契约；不受支持的平台本就不走
    // 注册分支，造了也无妨，这里按平台成立性开分支是为了表达意图）
    QWindow hotkeyWindow;
    if (GlobalHotkeys::isPlatformSupported()) {
        hotkeyWindow.create();
    }

    hotkeys->hotkeyPressed(QStringLiteral("lookup_selection"));
    QCOMPARE(quickFired, 0);
    QCOMPARE(showFired, 0);                              // 平台化占位分支
    hotkeys->hotkeyPressed(QStringLiteral("show_window"));
    QCOMPARE(quickFired, 0);
    QCOMPARE(showFired, 1);                              // 主窗前置
    hotkeys->hotkeyPressed(QStringLiteral("quick_lookup"));
    QCOMPARE(quickFired, 1);                             // 取词窗
    QCOMPARE(showFired, 1);
    hotkeys->hotkeyPressed(QStringLiteral("something_else"));
    QCOMPARE(quickFired, 1);
    QCOMPARE(showFired, 1);                              // unknown 全忽略

    QCOMPARE(adapter.isGlobalHotkeysSupported(),
             GlobalHotkeys::isPlatformSupported());

    // P-5 取词窗定位：光标坐标与 QCursor::pos() 逐值一致（offscreen 下
    // pos() 恒在 (0,0) 也得（0,0），断言本质是转发契约不是坐标值）
    QCOMPARE(adapter.cursorScreenPos(), QCursor::pos());
    QVERIFY(adapter.isGlobalHotkeysEnabled());           // 默认开

    const bool regOk = adapter.registerGlobalHotkey(
        QStringLiteral("quick_lookup"), QStringLiteral("Ctrl+Alt+L"));
    QCOMPARE(regOk, GlobalHotkeys::isPlatformSupported());
    QCOMPARE(adapter.registeredHotkeyActions().size(), regOk ? 1 : 0);
    QCOMPARE(adapter.getHotkeyForAction(QStringLiteral("quick_lookup")),
             regOk ? QString(QStringLiteral("Ctrl+Alt+L")) : QString());
    QVERIFY(adapter.getHotkeyForAction(QStringLiteral("nope")).isEmpty());

    adapter.unregisterGlobalHotkey(QStringLiteral("quick_lookup"));
    QVERIFY(adapter.registeredHotkeyActions().isEmpty());
    adapter.registerGlobalHotkey(QStringLiteral("a"), QStringLiteral("Ctrl+J"));
    adapter.unregisterAllGlobalHotkeys();
    QVERIFY(adapter.registeredHotkeyActions().isEmpty());

    adapter.setGlobalHotkeysEnabled(false);
    QVERIFY(!adapter.isGlobalHotkeysEnabled());
    adapter.setGlobalHotkeysEnabled(true);
    QVERIFY(adapter.isGlobalHotkeysEnabled());
}

// TTS 全部 wrapper 与预设/信息接口。headless 机器没有可用语音后端
// （QTextToSpeech 状态为 BackendError），say/pause 等是安全空操作，
// 所以断言全落在 adapter 自己维护的状态上（rate/pitch/volume 夹取、
// 预设查表、getVoiceInfo 汇总）——不依赖任何声音输出。
void LookupAdapterTest::tts_wrappers_presets_and_info() {
    LookupAdapter adapter;

    QCOMPARE(adapter.getRate(), 1.0);
    QCOMPARE(adapter.getPitch(), 0.0);
    QCOMPARE(adapter.getVolume(), 0.8);

    adapter.setRate(5.0);
    QCOMPARE(adapter.getRate(), 2.0);      // 上夹
    adapter.setRate(-3.0);
    QCOMPARE(adapter.getRate(), 0.1);      // 下夹
    adapter.setPitch(9.0);
    QCOMPARE(adapter.getPitch(), 1.0);
    adapter.setVolume(-1.0);
    QCOMPARE(adapter.getVolume(), 0.0);
    adapter.setVolume(0.6);
    QCOMPARE(adapter.getVolume(), 0.6);

    adapter.speakText(QStringLiteral("hello"));
    adapter.speakText(QStringLiteral("   "));      // trim 空 → 不说
    QVERIFY(!adapter.isSpeaking());
    QVERIFY(!adapter.isPaused());
    adapter.pauseSpeaking();
    adapter.resumeSpeaking();
    adapter.stopSpeaking();

    const QStringList voices = adapter.availableVoices();
    if (!voices.isEmpty()) {
        adapter.setVoice(voices.first());
    } else {
        adapter.setVoice(QStringLiteral("Bob"));            // 扫不到的名字
    }
    // 不假设机器有语音引擎（headless 下 voice 名可为空串），但
    // getVoiceInfo 的汇总必须与直查接口一致
    QCOMPARE(adapter.getVoiceInfo().value(QStringLiteral("voice")).toString(),
             adapter.getCurrentVoice());
    QCOMPARE(adapter.getVoiceInfo().value(QStringLiteral("availableVoices"))
                 .toStringList(), voices);

    QCOMPARE(adapter.getVoicePresets(),
             (QStringList{"Calm Study", "Default", "Quick Review"}));  // QMap 序
    adapter.applyVoicePreset(QStringLiteral("Nope"));       // 查无预设 → 不改
    QCOMPARE(adapter.getRate(), 0.1);
    adapter.applyVoicePreset(QStringLiteral("Calm Study"));
    QCOMPARE(adapter.getRate(), 0.8);
    QCOMPARE(adapter.getPitch(), -0.1);
    QCOMPARE(adapter.getVolume(), 0.9);
    adapter.applyVoicePreset(QStringLiteral("Default"));
    QCOMPARE(adapter.getRate(), 1.0);

    QVariantMap info = adapter.getVoiceInfo();
    QCOMPARE(info.value("speaking").toBool(), false);
    QCOMPARE(info.value("paused").toBool(), false);
    QCOMPARE(info.value("rate").toDouble(), 1.0);
    QCOMPARE(info.value("pitch").toDouble(), 0.0);
    QCOMPARE(info.value("volume").toDouble(), 0.8);
    QCOMPARE(info.value("autoPlayEnabled").toBool(), false);
    QCOMPARE(info.value("autoPlayDelay").toInt(), 1000);
    QVERIFY(info.contains(QStringLiteral("availableVoices")));
    QVERIFY(info.contains(QStringLiteral("voice")));
}

// .mdd 换文件的重新挂载：P-6 切 std 后词典身份 = 词典名，同名词典
// "Swap Book" 先挂 book.mdd，删掉 book.mdd、词典换注册到 Book.json+
// Book.mdd 后 ensureMdd 必须把旧记录摘掉再装新的——回归此前"先 load 后
// unload 把新解析器删掉却返回 true"的顺序 bug（换过 .mdd 的词典资源
// 永久全黑）。
void LookupAdapterTest::mdd_remount_after_file_swap() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString dir = tempDir.path();

    // 定义里不能带双引号——writeJsonDict 不做 JSON 转义，"<img src=\"…\">"
    // 会把生成的 JSON 打碎。媒体 HTML 走 presentEntry 的入参直传，不经词典。
    const QString lowerJson = writeJsonDict(
        dir, "book.json", "Swap Book", {{"hello", "see pic a"}});
    QVERIFY(!lowerJson.isEmpty());
    const QString lowerMdd = QDir(dir).filePath("book.mdd");
    QVERIFY(UnidictMdictFixture::writeMddResource(
        dir, "book", {{QStringLiteral("pic/a.png"),
                       UnidictMdictFixture::fakePng(16)}}));

    LookupAdapter adapter;
    qputenv("UNIDICT_DICTS", lowerJson.toUtf8());
    QVERIFY(adapter.loadDictionariesFromEnv());
    const QString id = QStringLiteral("Swap Book");   // std 面：词典名即 id

    // 第一挂：a.png 可读、可解析成 file:// URL
    QVERIFY(adapter.hasDictionaryResource(id, QStringLiteral("pic/a.png")));
    QCOMPARE(adapter.loadDictionaryResourceData(id, QStringLiteral("pic/a.png")),
             UnidictMdictFixture::fakePng(16));
    const QString url1 =
        adapter.dictionaryResourceUrl(id, QStringLiteral("pic/a.png"));
    QVERIFY(url1.startsWith(QStringLiteral("file://")));

    // 换文件：删 book.mdd，同名词典重新注册到带 b.png 的 Book.mdd
    // （大小写不敏感文件系统上 Book.json/book.mdd 就地覆盖，语义一致）
    QVERIFY(QFile::remove(lowerMdd));
    const QString upperJson = writeJsonDict(
        dir, "Book.json", "Swap Book", {{"hello", "see pic b"}});
    QVERIFY(!upperJson.isEmpty());
    QVERIFY(UnidictMdictFixture::writeMddResource(
        dir, "Book", {{QStringLiteral("pic/b.png"),
                       UnidictMdictFixture::fakePng(24)}}));
    qputenv("UNIDICT_DICTS", upperJson.toUtf8());
    QVERIFY(adapter.reloadDictionariesFromEnv());
    QCOMPARE(adapter.loadedDictionaries(), (QStringList{"Swap Book"}));  // 同名换体

    const QString url2 =
        adapter.dictionaryResourceUrl(id, QStringLiteral("pic/b.png"));
    QVERIFY2(!url2.isEmpty(),
             "重挂载后新 .mdd 的资源必须仍能解析（顺序 bug 回归哨兵）");
    QVERIFY(url2.startsWith(QStringLiteral("file://")));
    QCOMPARE(adapter.loadDictionaryResourceData(id, QStringLiteral("pic/b.png")),
             UnidictMdictFixture::fakePng(24));
    QVERIFY(!adapter.hasDictionaryResource(id, QStringLiteral("pic/gone.png")));
    QVERIFY(adapter.dictionaryResourceUrl(id, QStringLiteral("pic/gone.png"))
                .isEmpty());
    QVERIFY(adapter.dictionaryResourceUrl(id, QString()).isEmpty());   // 空键
    QVERIFY(adapter.dictionaryResourceUrl(QString(),
                                          QStringLiteral("pic/b.png")).isEmpty());
    QVERIFY(adapter.dictionaryResourceUrl(QStringLiteral("/no/such/dict"),
                                          QStringLiteral("pic/b.png")).isEmpty());

    // presentEntry 一站式管线在重挂载后的词典上仍给出 found=true 清单
    QVariantMap presented = adapter.presentEntry(
        QStringLiteral("<img src=\"pic/b.png\"><img src=\"missing.png\">"), id);
    QVariantList refs = presented.value(QStringLiteral("resources")).toList();
    QCOMPARE(refs.size(), 2);
    QCOMPARE(refs.at(0).toMap().value("found").toBool(), true);
    QCOMPARE(refs.at(1).toMap().value("found").toBool(), false);
    QVERIFY(presented.value("html").toString().contains(QStringLiteral("file://")));

    // 空/垃圾 id 在 has/load/loadData 三个入口的安全返回
    QVERIFY(!adapter.hasDictionaryResource(QString(), QStringLiteral("k")));
    QVERIFY(!adapter.hasDictionaryResource(id, QString()));
    QVERIFY(adapter.loadDictionaryResourceData(QStringLiteral("bogus"),
                                               QStringLiteral("k")).isEmpty());
    QVERIFY(adapter.rewriteResourceUrls(QStringLiteral("<img src=\"x.png\">"),
                                        QStringLiteral("bogus"))
                .contains(QStringLiteral("x.png")));  // 解不开就原样留着
    QCOMPARE(adapter.sanitizeHtml(QString("<b>hi</b>")).contains("<b>"), true);
    QCOMPARE(adapter.extractTextFromHtml(QString("<b>hi</b>")), QString("hi"));
    const QString xref = adapter.rewriteCrossReferenceLinks(
        QStringLiteral("bword://gamma and @@@LINK=delta"), id);
    QVERIFY(xref.contains(QStringLiteral("unidict://lookup?word=gamma")));
    QVERIFY(xref.contains(QStringLiteral("and delta")));  // @@@LINK 就地替换
    QVERIFY(!xref.contains(QStringLiteral("@@@LINK")));
}

// cache_dir 路径口径（平台存量债 ①，与 path_utils_std_branches_test T1
// 同口径、经 core/path_utils.h 的 Qt 门面）：env 非空原样返回；空/未设
// 回落 <cwd>/data[/cache]。回落侧期望值必须按路径语义构造——Windows 原
// 生分隔符是 '\'，生产回落拼接走 std::filesystem 的 operator/，字符串
// 字面拼 "/cache" 的期望只在 POSIX 成立（std 版同款断言此前挂 Windows
// CI 的根因，生产侧 fs 拼接合规、消费方全经 fs::path 消化）。
void LookupAdapterTest::cache_dir_path_caliber() {
    // 单进程共享 env：进槽存快照、出槽恢复；空值视同未设（生产侧
    // getenv_c 本就把空串归一为未设置，恢复语义无损）
    struct EnvGuard {
        QByteArray data = qgetenv("UNIDICT_DATA_DIR");
        QByteArray cache = qgetenv("UNIDICT_CACHE_DIR");
        ~EnvGuard() {
            if (data.isEmpty()) qunsetenv("UNIDICT_DATA_DIR");
            else qputenv("UNIDICT_DATA_DIR", data);
            if (cache.isEmpty()) qunsetenv("UNIDICT_CACHE_DIR");
            else qputenv("UNIDICT_CACHE_DIR", cache);
        }
    } guard;

    // 门面回落返回平台原生分隔符（Windows '\'）、QDir 恒 '/'：两侧都过
    // fromNativeSeparators 再比，口径只看归属不看分隔符
    const auto norm = [](const QString& p) {
        return QDir::cleanPath(QDir::fromNativeSeparators(p));
    };

    QTemporaryDir base;
    QVERIFY(base.isValid());

    // 非空：各自只认自己的变量，原样返回（不做任何加工）
    const QString dataEnv = QDir(base.path()).filePath("d1");
    const QString cacheEnv = QDir(base.path()).filePath("c1");
    qputenv("UNIDICT_DATA_DIR", dataEnv.toUtf8());
    qputenv("UNIDICT_CACHE_DIR", cacheEnv.toUtf8());
    QCOMPARE(PathUtils::dataDir(), dataEnv);
    QCOMPARE(PathUtils::cacheDir(), cacheEnv);

    // cache 未设：回落 data_dir/cache
    qunsetenv("UNIDICT_CACHE_DIR");
    QCOMPARE(norm(PathUtils::cacheDir()),
             norm(PathUtils::dataDir()) + "/cache");

    // 两者都空：回落 <cwd>/data[/cache]
    qunsetenv("UNIDICT_DATA_DIR");
    QCOMPARE(norm(PathUtils::dataDir()), QDir::currentPath() + "/data");
    QCOMPARE(norm(PathUtils::cacheDir()),
             QDir::currentPath() + "/data/cache");
}

void LookupAdapterTest::directory_import_pipeline() {
    LookupAdapter adapter;

    // 目录 A：两本有效词典 + 一本损坏（扩展名支持但解析失败 → 隔离档）
    QTemporaryDir dirA;
    QVERIFY(dirA.isValid());
    const QString dirAPath = QDir(dirA.path()).filePath("dicts");
    QDir().mkpath(dirAPath);
    QVERIFY(!writeJsonDict(dirAPath, "alpha.json", "Alpha",
                           {{"alpha", "a-def"}}).isEmpty());
    QVERIFY(!writeJsonDict(dirAPath, "beta.json", "Beta",
                           {{"beta", "b-def"}}).isEmpty());
    {
        QFile bad(QDir(dirAPath).filePath("broken.json"));
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("{ not valid json");
    }

    QSignalSpy scanSpy(&adapter, &LookupAdapter::importScanFinished);
    QSignalSpy progressSpy(&adapter, &LookupAdapter::importProgress);
    QSignalSpy finishSpy(&adapter, &LookupAdapter::importFinished);
    QSignalSpy stampSpy(&adapter, &LookupAdapter::dictionariesStampChanged);

    QVERIFY(!adapter.isDirectoryImportRunning());
    adapter.startDirectoryImport(dirAPath);

    // 扫描同步完成：3 个候选（broken.json 解析失败在装载段才算）
    QCOMPARE(scanSpy.count(), 1);
    QCOMPARE(scanSpy.first().first().toInt(), 3);
    QVERIFY(adapter.isDirectoryImportRunning());

    // 重复启动忽略：只维持当前会话
    adapter.startDirectoryImport(dirAPath);
    QCOMPARE(scanSpy.count(), 1);

    // 装载在事件循环分块步进——转循环到终局
    QTRY_COMPARE(finishSpy.count(), 1);
    QVERIFY(!adapter.isDirectoryImportRunning());
    QCOMPARE(finishSpy.first().at(0).toInt(), 2);   // imported
    QCOMPARE(finishSpy.first().at(1).toInt(), 1);   // failed（隔离档）
    QCOMPARE(progressSpy.count(), 3);
    // 进度累计口径：imported+failed 单调推进到 total
    QCOMPARE(progressSpy.at(2).at(0).toInt(), 2);
    QCOMPARE(progressSpy.at(2).at(1).toInt(), 1);
    QCOMPARE(progressSpy.at(2).at(2).toInt(), 3);
    // 终局建索引 + stamp：装载后可查
    QCOMPARE(stampSpy.count(), 1);
    QVERIFY(adapter.lookupDefinition("alpha").contains("a-def"));
    QVERIFY(adapter.lookupDefinition("beta").contains("b-def"));

    // 重复导入同目录：已装载/隔离去重 → 0 候选，空会话收尾不发 stamp
    const int stampBefore = stampSpy.count();
    scanSpy.clear();
    finishSpy.clear();
    adapter.startDirectoryImport(dirAPath);
    QCOMPARE(scanSpy.first().first().toInt(), 0);
    QTRY_COMPARE(finishSpy.count(), 1);
    QCOMPARE(finishSpy.first().at(0).toInt(), 0);
    QCOMPARE(stampSpy.count(), stampBefore);

    // 目录 B：运行中取消——start 同步入队、cancel 终局（已装部分照常
    // 建索引收口），挂起的步进见 !running 直接返回，不再发 finished
    QTemporaryDir dirB;
    QVERIFY(dirB.isValid());
    QVERIFY(!writeJsonDict(dirB.path(), "gamma.json", "Gamma",
                           {{"gamma", "g-def"}}).isEmpty());
    QVERIFY(!writeJsonDict(dirB.path(), "delta.json", "Delta",
                           {{"delta", "d-def"}}).isEmpty());
    scanSpy.clear();
    finishSpy.clear();
    stampSpy.clear();
    adapter.startDirectoryImport(dirB.path());
    QVERIFY(adapter.isDirectoryImportRunning());
    adapter.cancelDirectoryImport();
    QVERIFY(!adapter.isDirectoryImportRunning());
    QTRY_COMPARE(finishSpy.count(), 1);
    QCOMPARE(finishSpy.first().at(0).toInt(), 0);  // cancel 时尚未装载任何
    // 挂起步进被取消标志拦下：终局只此一份
    QTest::qWait(20);
    QCOMPARE(finishSpy.count(), 1);

    // 取消已终局的会话：no-op（不发第二份 finished）
    adapter.cancelDirectoryImport();
    QCOMPARE(finishSpy.count(), 1);
}

QTEST_MAIN(LookupAdapterTest)
#include "lookup_adapter_test.moc"
