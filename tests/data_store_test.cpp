#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "core/data_store.h"

using namespace UnidictCore;

class DataStoreTest : public QObject {
    Q_OBJECT
private slots:
    void history_add_dedupe_order();
    void vocab_add_and_clear();
    void vocab_tags_persist_and_meta();
    void vocab_tag_add_remove_filter_and_csv();
    void vocab_skills_meta_and_set();
    void notes_upsert_remove_and_persist();
    void notes_export_html_and_pdf();
    void pron_records_and_storage_path();
};

void DataStoreTest::history_add_dedupe_order() {
    auto& ds = DataStore::instance();
    ds.setStoragePath("data/unidict_test.json");
    ds.clearHistory();
    ds.addSearchHistory("hello");
    ds.addSearchHistory("world");
    ds.addSearchHistory("hello"); // 重查回到表头（P-7 合一后 manager 口径：新→旧）
    const QStringList h = ds.getSearchHistory(10);
    QCOMPARE(h.size(), 2);
    QCOMPARE(h.at(0), QString("hello"));
    QCOMPARE(h.at(1), QString("world"));
}

void DataStoreTest::vocab_add_and_clear() {
    auto& ds = DataStore::instance();
    ds.clearVocabulary();
    DictionaryEntry e; e.word = "foo"; e.definition = "bar"; ds.addVocabularyItem(e);
    auto items = ds.getVocabulary();
    QVERIFY(items.size() >= 1);
    bool found = false;
    for (const auto& it : items) if (it.word == "foo" && it.definition == "bar") { found = true; break; }
    QVERIFY(found);
    ds.clearVocabulary();
}

// 标签链路（Qt 门面）：setVocabularyItemTags 命中/未命中 + getVocabularyMeta 带 tags
void DataStoreTest::vocab_tags_persist_and_meta() {
    auto& ds = DataStore::instance();
    ds.clearVocabulary();
    DictionaryEntry e; e.word = "persist"; e.definition = "to last"; ds.addVocabularyItem(e);

    QVERIFY(!ds.setVocabularyItemTags("missing-word", {"x"}));
    QVERIFY(ds.setVocabularyItemTags("persist", {"exam", "high-freq"}));

    bool found = false;
    for (const auto& meta : ds.getVocabularyMeta()) {
        const QVariantMap m = meta.toMap();
        if (m.value("word").toString() == QLatin1String("persist")) {
            found = true;
            const QVariantList tags = m.value("tags").toList();
            QCOMPARE(tags.size(), 2);
            QCOMPARE(tags.at(0).toString(), QString("exam"));
            QCOMPARE(tags.at(1).toString(), QString("high-freq"));
        }
    }
    QVERIFY(found);
    ds.clearVocabulary();
}

// M3 标签管理链路（Qt 门面）：单标签增删（幂等/大小写不敏感/边界）+
// 按标签筛选 + CSV 升级口径（UTF-8 BOM、tags/note 列）
void DataStoreTest::vocab_tag_add_remove_filter_and_csv() {
    auto& ds = DataStore::instance();
    ds.clearVocabulary();
    DictionaryEntry a; a.word = "Apple"; a.definition = "fruit"; ds.addVocabularyItem(a);
    DictionaryEntry b; b.word = "banana"; b.definition = "yellow fruit"; ds.addVocabularyItem(b);

    // 增删边界（语义同 std 侧）
    QVERIFY(!ds.addVocabularyItemTag("missing", "x"));
    QVERIFY(!ds.addVocabularyItemTag("apple", QString()));
    QVERIFY(ds.addVocabularyItemTag("APPLE", "exam"));       // 词大小写不敏感
    QVERIFY(ds.addVocabularyItemTag("apple", "exam"));       // 幂等
    QVERIFY(!ds.removeVocabularyItemTag("apple", "nope"));
    QVERIFY(ds.removeVocabularyItemTag("apple", "exam"));

    // 筛选：banana 带 fruit-tag → 只命中 banana
    QVERIFY(ds.addVocabularyItemTag("banana", "fruit-tag"));
    const QVariantList by_tag = ds.getVocabularyByTag("fruit-tag");
    QCOMPARE(by_tag.size(), 1);
    const QVariantMap m = by_tag.at(0).toMap();
    QCOMPARE(m.value("word").toString(), QString("banana"));
    QCOMPARE(m.value("tags").toList().size(), 1);

    // CSV 升级口径：BOM + 四列表头 + 标签/笔记列
    ds.setNote("banana", "smoothie staple");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString csv = QDir(dir.path()).filePath("m3_export.csv");
    QVERIFY(ds.exportVocabularyCSV(csv));
    QFile f(csv);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray all = f.readAll();
    f.close();
    QVERIFY(all.startsWith("\xEF\xBB\xBF"));
    const QString body = QString::fromUtf8(all.mid(3));
    QVERIFY(body.startsWith(QLatin1String("word,definition,tags,note\n")));
    QVERIFY(body.contains(QLatin1String(
        "\"banana\",\"yellow fruit\",\"fruit-tag\",\"smoothie staple\"\n")));
    QVERIFY(body.contains(QLatin1String("\"Apple\",\"fruit\",\"\",\"\"\n")));

    // 现场恢复：单例存储被后续 slots 共享，笔记残留会破 notes 用例
    ds.setNote("banana", "");
    ds.clearVocabulary();
}

// P-7 四技能字段位（Qt 门面）：写入口转发 + getVocabularyMeta 条目带四技能键
void DataStoreTest::vocab_skills_meta_and_set() {
    auto& ds = DataStore::instance();
    ds.clearVocabulary();
    DictionaryEntry e; e.word = "skill"; e.definition = "n."; ds.addVocabularyItem(e);

    // 非法参数假（未知技能/等级越界/词未命中），合法写大小写不敏感
    QVERIFY(!ds.setVocabularyItemSkill("skill", "grammar", 1));
    QVERIFY(!ds.setVocabularyItemSkill("skill", "listen", 5));
    QVERIFY(!ds.setVocabularyItemSkill("missing", "listen", 1));
    QVERIFY(ds.setVocabularyItemSkill("SKILL", "Speak", 2));

    bool found = false;
    for (const auto& meta : ds.getVocabularyMeta()) {
        const QVariantMap m = meta.toMap();
        if (m.value("word").toString() == QLatin1String("skill")) {
            found = true;
            QCOMPARE(m.value("listen").toInt(), 0);
            QCOMPARE(m.value("speak").toInt(), 2);
            QCOMPARE(m.value("read").toInt(), 0);
            QCOMPARE(m.value("write").toInt(), 0);
        }
    }
    QVERIFY(found);
    ds.clearVocabulary();
}

// 笔记链路（Qt 门面）：upsert/大小写不敏感/空串删除 + getNotes 元数据
void DataStoreTest::notes_upsert_remove_and_persist() {
    auto& ds = DataStore::instance();
    ds.setNote("note_word", "first");
    ds.setNote("note_word", "second");           // upsert 覆盖
    ds.setNote("NOTE_WORD", "case-insensitive"); // 同一条
    QCOMPARE(ds.getNote("note_word"), QString("case-insensitive"));

    bool found = false;
    for (const auto& meta : ds.getNotes()) {
        const QVariantMap m = meta.toMap();
        if (m.value("word").toString() == QLatin1String("note_word")) {
            found = true;
            QCOMPARE(m.value("text").toString(), QString("case-insensitive"));
            QVERIFY(m.value("updated_at").toLongLong() > 0);
        }
    }
    QVERIFY(found);

    ds.setNote("note_word", ""); // 空串删除
    QCOMPARE(ds.getNote("note_word"), QString());
    QVERIFY(ds.getNotes().isEmpty());
}

// Q-6 覆盖收口：门面上此前无测试的转发——storagePath 读写、发音练习
// 记录增查清（load/save/ensureLoaded 兼容桩已随实时落盘口径退役）
void DataStoreTest::notes_export_html_and_pdf() {
    auto& ds = DataStore::instance();
    QTemporaryDir dir;
    ds.setStoragePath(dir.filePath("notes_export_ds.json"));
    ds.setNote("hello", "export <note> & \"stuff\"\nsecond line");
    ds.setNote("中文词", "中文备注");

    // HTML：与 std 导出器（notes_export_std）同一转义/换行口径
    const QString htmlPath = dir.filePath("notes.html");
    QVERIFY(ds.exportNotesHtml(htmlPath));
    QFile hf(htmlPath);
    QVERIFY(hf.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(hf.readAll());
    hf.close();
    QVERIFY(html.contains("&lt;note&gt; &amp; &quot;stuff&quot;"));
    QVERIFY(html.contains("<br>second line"));
    QVERIFY(html.contains(QStringLiteral("<h2>中文词</h2>")));

    // PDF：Qt 排版（QPdfWriter），文件头 %PDF
    const QString pdfPath = dir.filePath("notes.pdf");
    QVERIFY(ds.exportNotesPdf(pdfPath));
    QFile pf(pdfPath);
    QVERIFY(pf.open(QIODevice::ReadOnly));
    const QByteArray head = pf.read(4);
    pf.close();
    QCOMPARE(head, QByteArray("%PDF"));

    // 不可写路径：父路径是已存在文件 → 两种格式都 false
    const QString blocked = dir.filePath("blocked");
    {
        QFile b(blocked);
        QVERIFY(b.open(QIODevice::WriteOnly));
        b.write("x");
    }
    QVERIFY(!ds.exportNotesPdf(blocked + "/out.pdf"));
    QVERIFY(!ds.exportNotesHtml(blocked + "/out.html"));
}

void DataStoreTest::pron_records_and_storage_path() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto& ds = DataStore::instance();
    const QString storage = QDir(dir.path()).filePath("pron_store.json");
    ds.setStoragePath(storage);
    QCOMPARE(ds.storagePath(), storage);

    ds.clearPronRecords();
    QVERIFY(ds.getPronRecord("never").isEmpty());

    ds.setPronRecord("apple", 71.5, 88.0, 3, 1234567890);
    const QVariantMap rec = ds.getPronRecord("apple");
    QCOMPARE(rec.value("word").toString(), QString("apple"));
    QCOMPARE(rec.value("attempts").toInt(), 3);
    QVERIFY(qFuzzyCompare(rec.value("best_score").toDouble(), 88.0));
    QVERIFY(qFuzzyCompare(rec.value("last_score").toDouble(), 71.5));
    QCOMPARE(rec.value("last_at").toLongLong(), qlonglong(1234567890));

    const QVariantList all = ds.getPronRecords();
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.at(0).toMap().value("word").toString(), QString("apple"));

    ds.clearPronRecords();
    QVERIFY(ds.getPronRecords().isEmpty());
}

QTEST_MAIN(DataStoreTest)
#include "data_store_test.moc"

