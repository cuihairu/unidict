// StarDict 资源表（stardict_resource_std）专项：res 键三形态（目录/
// 清单文件/指向不存在）+ 散装兜底 + 键归一 + DictionaryStd 挂接 +
// manager 便利方法。纯 std（无 Qt），与 json_parser_std_edge_test 同式。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/dictionary_manager_std.h"
#include "std/stardict_resource_std.h"

namespace fs = std::filesystem;
using namespace UnidictCoreStd;

static fs::path write_file(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
    return p;
}

// 最小可用 .ifo（解析器成功的前提：.idx 存在且非空——load_idx 拒空表；
// 资源表独立于词条体）
static fs::path write_min_ifo(const fs::path& dir, const std::string& name,
                              const std::string& extra_lines = {}) {
    const fs::path base = dir / name;
    const std::string word = "hello";
    const std::string def = "greeting";
    std::string idx;
    idx += word;
    idx += '\0';
    const unsigned int off = 0, len = (unsigned int)def.size();
    for (int i = 3; i >= 0; --i) idx += (char)((off >> (8 * i)) & 0xff);
    for (int i = 3; i >= 0; --i) idx += (char)((len >> (8 * i)) & 0xff);
    write_file(base.string() + ".ifo",
               "StarDict's dict ifo file\n"
               "version=2.4.2\n"
               "bookname=" + name + "\n"
               "wordcount=1\n"
               "idxfilesize=" + std::to_string(idx.size()) + "\n"
               "description=t\n" + extra_lines);
    write_file(base.string() + ".idx", idx);
    write_file(base.string() + ".dict", def);
    return base.string() + ".ifo";
}

int main() {
    const fs::path root = fs::current_path() / "build-local" / "sdres";
    fs::remove_all(root);
    fs::create_directories(root);

    // ① res 键 = 目录：递归收录，键 = 相对词典目录的 POSIX 路径
    {
        const fs::path d = root / "resdir";
        const fs::path ifo = write_min_ifo(d, "rd", "res = pics\n");
        write_file(d / "pics" / "a.png", "PNG");
        write_file(d / "pics" / "sub" / "b.jpg", "JPG");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.is_loaded());
        // 双键索引：2 文件 ×（资源根相对键 + 含目录名键）= 4 条目
        assert(p.resource_count() == 4);
        assert(p.has_resource("pics/a.png"));       // 含目录名键
        assert(p.has_resource("a.png"));            // 资源根相对键
        assert(p.has_resource("pics/sub/b.jpg"));
        assert(p.has_resource("sub/b.jpg"));
        assert(!p.has_resource("pics/missing.png"));
        // 两键同指一文件
        assert(p.resource_path("a.png") == p.resource_path("pics/a.png"));
        assert(fs::exists(p.resource_path("pics/a.png")));
    }

    // ② res 键 = 清单文件：逐行文件名（含子目录），空行/# 注释跳过
    {
        const fs::path d = root / "reslist";
        const fs::path ifo = write_min_ifo(d, "rl", "res = my.res\n");
        write_file(d / "my.res", "x.png\n\n# comment\nsound/y.mp3\n");
        write_file(d / "x.png", "PNG");
        write_file(d / "sound" / "y.mp3", "MP3");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.resource_count() == 2);
        assert(p.has_resource("x.png"));
        assert(p.has_resource("sound/y.mp3"));
        assert(fs::exists(p.resource_path("sound/y.mp3")));
    }

    // ③ res 键指向不存在目标：显式声明优先，不静默兜底（空表但可预期）
    {
        const fs::path d = root / "resmissing";
        const fs::path ifo = write_min_ifo(d, "rm", "res = nope\n");
        write_file(d / "loose.png", "PNG");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.is_loaded());
        assert(p.resource_count() == 0);
        assert(!p.has_resource("loose.png"));
    }

    // ④ 无 res 键 → 散装兜底：直属媒体文件入表，骨架/非媒体排除
    {
        const fs::path d = root / "implicit";
        const fs::path ifo = write_min_ifo(d, "im");
        write_file(d / "pic.png", "PNG");
        write_file(d / "notes.txt", "text");   // 非媒体 → 不入表
        write_file(d / "cover.jpg", "JPG");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.resource_count() == 2);
        assert(p.has_resource("pic.png"));
        assert(p.has_resource("cover.jpg"));
        assert(!p.has_resource("notes.txt"));
        assert(!p.has_resource("im.ifo"));  // 骨架不入表
    }

    // ⑤ 无 res 键 + res/ 命名目录（无键形态）：递归收录
    {
        const fs::path d = root / "resfolder";
        const fs::path ifo = write_min_ifo(d, "rf");
        write_file(d / "res" / "deep" / "c.gif", "GIF");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.resource_count() == 2);   // 双键：deep/c.gif + res/deep/c.gif
        assert(p.has_resource("deep/c.gif"));
        assert(p.has_resource("res/deep/c.gif"));
    }

    // ⑥ 键归一：反斜杠/前导斜杠/协议前缀/?query/#fragment/大小写
    {
        const fs::path d = root / "norm";
        const fs::path ifo = write_min_ifo(d, "nm", "res = r\n");
        write_file(d / "r" / "Pic.PNG", "PNG");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.has_resource("r/Pic.PNG"));
        assert(p.has_resource("r\\pic.png"));          // 反斜杠
        assert(p.has_resource("/r/pic.png"));         // 前导斜杠
        assert(p.has_resource("sound://r/pic.png"));   // 协议前缀
        assert(p.has_resource("r/pic.png?x=1"));      // query
        assert(p.has_resource("r/pic.png#frag"));      // fragment
        // "./" 前缀由 adapter 侧（resolveOne）在调用前剥除，解析器不重复
        assert(p.resource_path("R/PIC.PNG") == p.resource_path("r/pic.png"));
    }

    // ⑦ 中文文件名（UTF-8 直通）
    {
        const fs::path d = root / "unicode";
        const fs::path ifo = write_min_ifo(d, "uc", "res = 图片\n");
        write_file(d / "图片" / "你好.png", "PNG");
        StarDictResourceParser p;
        assert(p.load(ifo.string()));
        assert(p.has_resource("图片/你好.png"));
        assert(fs::exists(p.resource_path("图片/你好.png")));
    }

    // ⑧ DictionaryStd 挂接：load .ifo 后 star_dict_parsers 非空，
    // manager 便利方法按词典名命中
    {
        const fs::path d = root / "attach";
        const fs::path ifo = write_min_ifo(d, "at", "res = media\n");
        write_file(d / "media" / "z.png", "PNG");
        DictionaryStd dict;
        assert(dict.load(ifo.string()));
        assert(dict.star_dict_parsers().size() == 1);
        assert(dict.star_dict_parsers()[0]->has_resource("media/z.png"));

        DictionaryManagerStd mgr;
        assert(mgr.add_dictionary(ifo.string()));
        assert(mgr.star_dict_has_resource("at", "media/z.png"));
        assert(!mgr.star_dict_has_resource("at", "media/none.png"));
        assert(!mgr.star_dict_has_resource("no-such-dict", "media/z.png"));
        const std::string path = mgr.star_dict_resource_path("at", "media/z.png");
        assert(!path.empty());
        assert(fs::exists(path));
        // 未知名/未挂资源词典：空串不炸
        assert(mgr.star_dict_resource_path("at", "nope").empty());
    }

    // ⑨ 无资源词典挂接：star_dict_parsers 为空但 load 成功（资源损坏
    // 不拒词典本体）
    {
        const fs::path d = root / "plain";
        const fs::path ifo = write_min_ifo(d, "pl");
        DictionaryStd dict;
        assert(dict.load(ifo.string()));
        assert(dict.star_dict_parsers().empty());
    }

    fs::remove_all(root);
    std::printf("OK\n");
    return 0;
}
