#include "lemma_std.h"

#include <cctype>
#include <unordered_map>

namespace UnidictCoreStd::Lemma {

namespace {

// 不规则变化表：屈折形 → 原形。只收高频词——生僻不规则词靠词典自身的
// 交叉引用（近义词/参见）兜底，表不是目标。
const std::unordered_map<std::string, std::string>& irregular_map() {
    static const std::unordered_map<std::string, std::string> m = {
        // 名词复数（f/fe→ves、元音变换、外来残余）
        {"wolves", "wolf"},   {"knives", "knife"},  {"lives", "life"},
        {"leaves", "leaf"},   {"halves", "half"},   {"wives", "wife"},
        {"shelves", "shelf"}, {"loaves", "loaf"},   {"thieves", "thief"},
        {"calves", "calf"},   {"elves", "elf"},     {"selves", "self"},
        {"men", "man"},       {"women", "woman"},   {"children", "child"},
        {"feet", "foot"},     {"teeth", "tooth"},   {"mice", "mouse"},
        {"lice", "louse"},    {"geese", "goose"},   {"oxen", "ox"},
        {"people", "person"},
        // -oes 名词（goes/does 是动词变化，单列）
        {"heroes", "hero"},   {"tomatoes", "tomato"}, {"potatoes", "potato"},
        {"echoes", "echo"},   {"vetoes", "veto"},
        // 动词不规则过去式/过去分词
        {"ran", "run"},       {"went", "go"},       {"gone", "go"},
        {"eaten", "eat"},     {"written", "write"}, {"spoken", "speak"},
        {"took", "take"},     {"given", "give"},    {"seen", "see"},
        {"known", "know"},    {"grew", "grow"},     {"flew", "fly"},
        {"drew", "draw"},     {"drove", "drive"},   {"broke", "break"},
        {"chose", "choose"},  {"fell", "fall"},     {"began", "begin"},
        {"drank", "drink"},   {"sang", "sing"},     {"swam", "swim"},
        {"rang", "ring"},     {"wore", "wear"},     {"tore", "tear"},
        {"rose", "rise"},     {"arose", "arise"},   {"forgot", "forget"},
        {"built", "build"},   {"sent", "send"},     {"lost", "lose"},
        {"met", "meet"},      {"held", "hold"},     {"kept", "keep"},
        {"left", "leave"},    {"felt", "feel"},     {"found", "find"},
        {"brought", "bring"}, {"thought", "think"}, {"bought", "buy"},
        {"caught", "catch"},  {"taught", "teach"},  {"sold", "sell"},
        {"told", "tell"},     {"won", "win"},       {"sat", "sit"},
        {"stood", "stand"},   {"understood", "understand"},
        {"made", "make"},     {"said", "say"},      {"paid", "pay"},
        {"laid", "lay"},      {"led", "lead"},
        {"meant", "mean"},    {"heard", "hear"},
        // 形容词级（查 good/bad 而非 gooder 类伪词）
        {"better", "good"},   {"best", "good"},
        {"worse", "bad"},     {"worst", "bad"},
        // 高频动词三单/复数歧义形
        {"goes", "go"},       {"does", "do"},       {"has", "have"},
    };
    return m;
}

bool ascii_only(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return false;
    }
    return true;
}

std::string lcase(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool ends_with(const std::string& s, const char* suf) {
    const std::string x(suf);
    return s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

// 双写辅音回退：running→runn→run、stopped→stopp→stop。规则：末两字
// 相同且为辅音（排除 w/x/y 这类英语里不双写的），去一位。
bool collapse_doubled(std::string& s) {
    if (s.size() < 3) return false;
    const char a = s[s.size() - 2], b = s[s.size() - 1];
    if (a != b) return false;
    if (a == 'w' || a == 'x' || a == 'y' || !std::isalpha(static_cast<unsigned char>(a))) {
        return false;
    }
    s.pop_back();
    return true;
}

void push_unique(std::vector<std::string>& out, const std::string& cand,
                 const std::string& self) {
    if (cand == self) return;  // 契约：候选不含输入词自身
    for (const auto& v : out) {
        if (v == cand) return;
    }
    out.push_back(cand);
}

} // namespace

std::vector<std::string> lemma_candidates(const std::string& word) {
    std::vector<std::string> out;
    if (word.size() < 3 || !ascii_only(word)) return out;  // 短词/非 ASCII 空手
    const std::string w = lcase(word);

    // 1) 不规则表一击即中（表形即屈折形，无歧义）
    const auto& irr = irregular_map();
    const auto it = irr.find(w);
    if (it != irr.end()) {
        push_unique(out, it->second, w);
        return out;
    }

    // 2) -ied → -y（studied→study）优先于 -ed（否则会拆出伪词 studi）
    if (ends_with(w, "ied") && w.size() >= 5) {
        push_unique(out, w.substr(0, w.size() - 3) + "y", w);
        return out;
    }

    // 3) -ies → -y（studies→study、flies→fly）
    if (ends_with(w, "ies") && w.size() >= 5) {
        push_unique(out, w.substr(0, w.size() - 3) + "y", w);
        return out;
    }

    // 4) -es：s/x/z 后或 ch/sh 后（boxes→box、buses→bus、matches→match、
    // washes→wash）
    if (ends_with(w, "es") && w.size() >= 5) {
        const char prev = w[w.size() - 3];
        if (prev == 's' || prev == 'x' || prev == 'z' || ends_with(w, "ches") ||
            ends_with(w, "shes")) {
            push_unique(out, w.substr(0, w.size() - 2), w);
            return out;
        }
    }

    // 5) -ing（≥5 保证去后缀后剩 ≥2 位）：裸去 + 双写回退 + e 复原三候选
    if (ends_with(w, "ing") && w.size() >= 5) {
        std::string stem = w.substr(0, w.size() - 3);
        push_unique(out, stem, w);               // doing→do、fishing→fish
        std::string collapsed = stem;
        if (collapse_doubled(collapsed)) push_unique(out, collapsed, w);  // running→run
        push_unique(out, stem + "e", w);         // making→make、using→use
        return out;
    }

    // 6) -ed（≥4 保证去后缀后剩 ≥2 位）：裸去 + 双写回退 + e 复原三候选
    if (ends_with(w, "ed") && w.size() >= 4) {
        std::string stem = w.substr(0, w.size() - 2);
        push_unique(out, stem, w);               // looked→look
        std::string collapsed = stem;
        if (collapse_doubled(collapsed)) push_unique(out, collapsed, w);  // stopped→stop
        push_unique(out, stem + "e", w);         // loved→love、died→die
        return out;
    }

    // 7) -s：排除 ss/us/is（glass/bus/this 不是复数）；≥4 保证去后缀剩 ≥3
    if (ends_with(w, "s") && w.size() >= 4 && !ends_with(w, "ss") &&
        !ends_with(w, "us") && !ends_with(w, "is")) {
        push_unique(out, w.substr(0, w.size() - 1), w);  // cats→cat、walks→walk
        return out;
    }

    return out;
}

} // namespace UnidictCoreStd::Lemma
