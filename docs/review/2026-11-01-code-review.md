# 代码深审报告 — unidict（2026-11-01/02）

**范围**：全仓（`core/std`、`adapters/qt`、`adapters/android/jni`、`cli-std`、`gui`、`qmlui`、`scripts`）。
**方法**：① 安全静态扫描（硬编码密钥 / shell 注入 / SQL 拼接 / eval / 反序列化 / 路径穿越）；② 隐藏 bug 猎捕（错误处理缺失 / 竞态 / 越界 / 资源泄漏 / off-by-one / 并发）；③ 质量门（lint / 类型检查 / 测试基线）。
**结论**：P0=0；P1=8（安全 6 + bug 2，其中 bug-P1[2] 与安全 stardict 重叠）；P2=10；P3=21。**已修 P1=8**，其余 P2/P3 记 todo.md。

---

## 一、已修复（P1，全部当场修，测试绿）

### 反序列化 OOM/DoS 族（安全 P1 ×6）— 根因：文件控制长度/计数未校验即 reserve/resize

| # | file:line | 根因 | 修复 |
|---|---|---|---|
| 1 | `core/std/mdict_parser_std.cpp:253,287,321,356` | `items.reserve(n)`：`n` 为解析器魔数后紧读的文件控制 u32，**先**于逐项边界检查分配。~20 字节畸形 .mdx/.mdd 带 `n=0xFFFFFFFF` → ~160GB reserve → bad_alloc/length_error | 各项钳制 `reserve(std::min(n, 剩余字节/最小项大小))`（`/10u`、`/14u`） |
| 2 | `core/std/mdict_parser_std.cpp:370` | `bdec.reserve(blocks)`：`blocks` 为 RBCT 文件控制 u32 | 钳制 `reserve(std::min(blocks, 剩余/9u))` |
| 3 | `core/std/mdict_parser_std.cpp:441-443` | 逐块 inflate 输出累积，单块 16MB 封顶但块数无界 → zlib 炸弹（~1MB 输入 → ~1TB） | 加 `MAX_TOTAL_DECODED = 512MB` 累计上限（两处，GCOVR_EXCL_LINE） |
| 4 | `core/std/mdd_resource_std.cpp:521-522` | `resources_.reserve(count)` / `resource_keys_.reserve(count)`：`count` 为文件控制 u32 | 钳制 `cap = min(count, 剩余字节/8)`（经 ftell/fseek 求文件余量） |
| 5 | `core/std/stardict_parser_std.cpp:340`（根因 111-116） | `lookup_raw` 的 `out.resize(sz)`：`sz` 取自 .idx 未与 .dict 实际尺寸校验 | 加 `dict_size_`（open_dict 时 seekg/tellg 实测）+ `if (off+sz > dict_size_) return {}` |
| 6 | `core/std/fulltext_index_std.cpp:389-414` | `load` 从文件控制 u32 字段先分配后截断检查：`signature_.resize(siglen)`、`doc_tf_/doc_map_.resize(docs)`（≤4e9×8B）、`term.resize(len)`、`buf.resize(blen)`、`ent.vec.reserve(n)` | 加 `remaining()` 余量函数，5 处守卫（`truncated (sig/docmap/term/compressed data/posting)`） |

### bug P1 ×2（另 bug-P1[2] 与安全 #5 重叠，已随 #5 修复）

| # | file:line | 根因 | 修复 |
|---|---|---|---|
| 7 | `cli-std/main.cpp:649-657` | `--ft-index-upgrade-dir` 驱动 `recursive_directory_iterator` 抛掷构造/隐式递增，无 try/catch；main 无顶层 handler → 不存在/权限拒目录中止 CLI（uncaught filesystem_error） | 改 error_code 重载 while 循环：`recursive_directory_iterator dit(dir, fec), dend; ... dit.increment(fec); if (fec) break;` |
| 8 | `core/std/mdd_resource_std.cpp:885-889,926,954` | 记录块 `decomp_size` 未钳制，`total_decomp += decomp_size` 可环绕破坏块区间单调性；块搜索只查 `pos < decomp_start+decomp_size` 不查 `pos >= decomp_start` → `from = pos - decomp_start` 下溢成 OOB 迭代器 | 加 `MAX_MDICT_TOTAL_DECOMP = 4GB` 累计上限（环绕即畸形快速失败）+ `pos >= b.decomp_start` 下界 |

### 修复验证

- 回归测试 `tests/parser_alloc_guard_std_test.cpp`（新增，CTest：`test_parser_alloc_guard_std`）：MDX KIDX/KEYB/KBIX/KBIX-multirb `count=0xFFFFFFFF` + 短体 → 种子兜底不中止；fulltext UDFT2 `docs=0xFFFFFFFF` → `load` 返回 false `truncated (docmap)`；StarDict 越界 sz → `lookup` 空返回。
- `tests/mdd_mdict_std_test.cpp`：新增 `rbigdec` 手术用例（record 块 `decomp_size=5GB` → 快速失败），覆盖 4GB 累计上限真分支。
- 全量 ctest：std 138/138、Qt 主套件全绿（仅 `ui_click_audit` 环境性失败：无 speech-dispatcher 守护进程 → `hasLocalTts()=false`，审计期望经 `pronOnlineStatusText` 发射，与实现信号错位——非本批回归，`gui_click_audit` 通过）。

---

## 二、已记录 todo（P2，10 项）

| # | file:line | 问题 | 严重度 |
|---|---|---|---|
| 1 | `adapters/qt/ai_service_qt.cpp:86-92` | `waitForFinished(15000)` 返回值忽略、`exitCode()` 不查 → 失败命令的 stderr 当 AI 答案返回 | P2 |
| 2 | `core/std/index_engine_std.cpp:211` | `std::stoi(freq_s)` 未捕获（畸形索引行抛 invalid_argument 中止）；getline 超长行局部 DoS（= 安全 P2） | P2 |
| 3 | `core/std/fulltext_index_std.cpp:426-447` | `ensure_postings` const 方法经 const_cast 懒解码，并发 search 数据竞争（当前仅 UI 线程调用，潜在） | P2 |
| 4 | `core/std/dictionary_manager_std.cpp:601-618` | 全文本/前缀索引 const_cast 懒构建非线程安全（与 #3 组合成竞态） | P2 |
| 5 | `core/std/sync_engine_std.cpp`（state_/outbox_/cursor_/local_seq_） | 可变状态无同步，多线程调用竞态（当前单线程驱动） | P2 |
| 6 | `core/std/mdx_v2_reader_std.cpp:588` | `stream.size()+sz.second > kMaxStreamBytes` u64 加法环绕，512MB 上限失效（下游尺寸检查兜底，良性） | P2 |
| 7 | `core/std/mdx_v2_reader_std.cpp:592` | `data.size() < cursor+sz.first` 同类环绕，截断检查可绕过（substr 钳制，良性） | P2 |
| 8 | `qmlui/mobile_utils.cpp:103` | 静态 `AndroidActivityResultReceiver` 永久绑定首实例、从不 delete；首实例析构后回调解引用悬垂 this（泄漏+UAF） | P2 |
| 9 | `adapters/android/jni/jni_util.h:32-41,72-86,109-124` | FindClass/NewObjectArray 结果未判空 → 缺类/OOM 使 GetMethodID(nullptr)/SetObjectArrayElement(nullptr) 中止 JVM | P2 |
| 10 | `adapters/qt/sync_service_qt.cpp:119→133,287→318,456` | 本地清空+重建先于远端写；远端写失败本地已变；`f.write(...)` ×3 未检查（磁盘满静默丢失） | P2 |

## 三、已记录 todo（P3，21 项 = bug 16 + 安全 5）

### bug P3 ×16

| # | file:line | 问题 |
|---|---|---|
| 1 | `adapters/qt/data_store_qt.cpp:190-224` | `exportNotesPdf` 截断目标后不查 `doc.print(&writer)`，无条件返回成功 |
| 2 | `core/std/data_store_std.cpp:348-419` | `save()` 仅查 `if(!out)`，写后不复查流态 → 磁盘满报成功丢数据 |
| 3 | `adapters/qt/sync_manager_qt.cpp:436` | `exportBackup` 的 `f.write(...)` 未检查 |
| 4 | `core/std/stardict_parser_std.cpp:110,114` | `p + 8 + 4 > end` 近 end 时可能算出一过端指针（算术 UB，未解引用，表面性） |
| 5 | `core/std/dictionary_std.cpp:63` | `.mdd` 伴生扫描的 `de.is_regular_file()` 用抛掷重载，中途权限变化可抛 |
| 6 | `core/std/mdict_parser_std.cpp:1007-1041` | 未识别容器兜底整读 .mdx 后逐偏移 zlib 扫描 → 严重 CPU/内存膨胀（仅兜底路径） |
| 7 | `core/std/mdict_parser_std.cpp:776-796` | `rewrite_attr` 匹配 `src=` 时也匹配 `data-src=`，误改写资源引用（无边界检查） |
| 8 | `core/std/sync_engine_std.cpp:686-689` | 宽容 JSON 扫描的负值强转 u64 环绕 |
| 9 | `qmlui/lookup_adapter.cpp:615-616` | `static_cast<int>(data.size())` size_t→int 截断（实际 < 2^31） |
| 10 | `core/std/dsl_parser_std.cpp:176-184` | 备用词头逗号切分不发最后一段（逻辑缺口） |
| 11 | `core/std/ripemd128_std.cpp:103,109` | `((len+8)/64+1)*64` len 近 SIZE_MAX 环绕，memcpy 越界（需 ~16EB 输入，潜伏/不现实） |
| 12 | `qmlui/tray_controller.cpp:122` | `m_menu = new QMenu()` 无父、setContextMenu 不接管 → 析构泄漏（进程级） |
| 13 | `core/std/mdd_resource_std.cpp:500,577` | `parse_simplekv_fallback` 用 32 位 fseek/ftell（Windows）→ >2GB .mdd 误析（仅 SimpleKV） |
| 14 | `adapters/android/jni/store_jni.cpp:54-57`、`dict_jni.cpp:20-28` | raw `new` 转 jlong；未 close 即泄漏（有界，JNI 惯例） |
| 15 | `adapters/android/jni/jni_util.h:44-46` | `handle_of` 不校验：0/陈旧句柄变空/UAF 指针而非干净错误；双重 close = 双释放 |
| 16 | `core/std/mdd_resource_std.cpp:980` | `MAX_RESOURCE_SIZE`（10MB）静默丢弃更大资源，无诊断（设计上限） |

### 安全 P3 ×5（均良性）

| # | file:line | 问题 |
|---|---|---|
| 17 | `adapters/qt/ai_service_qt.cpp:64-93` | 已核：`p.start(cmd_, args)` 参数数组无 shell、提示走 stdin，无注入（良性，正确模式） |
| 18 | `qmlui/components/QuickLookupPane.qml:251,255` | 词典 HTML 以 `TextEdit.RichText` 渲染，无脚本引擎暴露；未净化定义 HTML（信息性，无 XSS 原语） |
| 19 | `core/std/mdd_resource_std.cpp:1051-1052` | `extract_to_cache` 仅替换 `/` 不替换 `\\`，Windows 上 `..\\..\\evil` 可逸出 cache_dir；但**无生产调用者**（死路径，纵深防御注记） |
| 20 | CLI `--dict-pass`（argv 密码） | 密码可见于 ps 进程列表（常见 CLI 权衡，信息性） |
| 21 | 测试/夹具 | shell×2 命中均在测试夹具（`tests/cli_std_output_test.cpp:42` popen 二进制路径字符串），非生产 |

**清洁核验**：secrets 0（全仓无真密钥）、SQL 0（无 DB；server/sync_relay worker.js 全部参数化 `.bind()`，唯一动态 `IN (${chunk.map(()=>"?")})` 只插占位符）、eval 0（无 eval/new Function/QJSEngine；`.exec()` 均为事件循环/菜单）、deser 其余（JSON 迭代式括号深度扫描器无递归栈溢出；无 QDataStream 外部反序列化）。

---

## 四、提交清单

| commit | 内容 |
|---|---|
| （本批） | `fix(core): bound deserialization-driven allocations in parsers` — 安全 P1 反序列化 OOM 族 + bug-P1[8] mdd 环绕（含回归测试） |
| （本批） | `fix(cli): tolerate directory iteration errors on ft-index upgrade` — bug-P1[7] cli 递归迭代器 |

## 五、发现/已修/剩余

- **发现**：P1=8（安全 6 + bug 2），P2=10，P3=21（bug 16 + 安全 5），P0=0。
- **已修**：P1=8（全部，当场修）。
- **剩余**：P2=10、P3=21 → 已登记 todo.md；P0=0。
