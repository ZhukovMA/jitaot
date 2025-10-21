#include "parser.h"
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_map>

using namespace std;

optional<Graph> parseByteCode(const string &path, string *err) {
    ifstream in(path);
    if (!in) {
        if (err)
            *err = "Cannot open " + path;
        return nullopt;
    }
    vector<string> raw;
    string line;
    while (getline(in, line)) {
        string s = trim(line);
        if (s.empty())
            continue;
        raw.push_back(s);
    }

    unordered_map<string, int> label2pc;
    int pc = 0;
    for (auto &s : raw) {
        if (s.back() == ':') {
            string lab = trim(s.substr(0, s.size() - 1));
            if (label2pc.count(lab)) {
                return nullopt;
            }
            label2pc[lab] = pc;
        } else
            pc += 4;
    }

    Graph P;
    P.entry = 0;
    pc = 0;

    auto splitArgs = [&](const string &s) -> vector<string> {
        auto p = s.find(' ');
        string tail = (p == string::npos ? "" : s.substr(p + 1));
        vector<string> out;
        string cur;
        for (char c : tail) {
            if (c == ',' || c == ' ' || c == '\t') {
                if (!cur.empty()) {
                    out.push_back(trim(cur));
                    cur.clear();
                }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty())
            out.push_back(trim(cur));
        return out;
    };
    auto isSSAName = [&](const string &tok) -> bool {
        if (tok.size() >= 2 && tok[0] == 'v')
            return true;
        if (tok.size() >= 2 && tok[0] == 'a')
            return true;
        return false;
    };

    for (auto &s : raw) {
        if (s.back() == ':')
            continue;
        Inst *I = P.makeInst();
        I->pc = pc;
        I->text = s;
        string lw = lower(s);
        size_t sp = lw.find_first_of(" \t");
        string head = (sp == string::npos ? lw : lw.substr(0, sp));
        I->op = head;
        if (head.rfind("ret", 0) == 0)
            I->isRet = true;
        else if (head == "jmp") {
            I->isUncondBr = true;
            auto p2 = s.find(' ');
            string lab = trim(s.substr(p2 + 1));
            if (!label2pc.count(lab)) {
                return nullopt;
            }
            I->target_pc = label2pc[lab];
        } else if (head == "ja" || head == "jb" || head == "je" || head == "jne") {
            I->isCondBr = true;
            auto p2 = s.find(' ');
            string lab = trim(s.substr(p2 + 1));
            if (!label2pc.count(lab)) {
                return nullopt;
            }
            I->target_pc = label2pc[lab];
        }

        vector<string> args = splitArgs(s);
        if (head.rfind("movi", 0) == 0 || head.rfind("u32tou64", 0) == 0) {
            if (!args.empty() && isSSAName(args[0]))
                I->def = args[0];
            for (size_t k = 1; k < args.size(); ++k)
                if (isSSAName(args[k]))
                    I->useNames.push_back(args[k]);
        } else if (head.rfind("mul", 0) == 0 || head.rfind("addi", 0) == 0) {
            if (!args.empty() && isSSAName(args[0]))
                I->def = args[0];
            for (size_t k = 1; k < args.size(); ++k)
                if (isSSAName(args[k]))
                    I->useNames.push_back(args[k]);
        } else if (head.rfind("cmp", 0) == 0) {
            for (auto &a : args)
                if (isSSAName(a))
                    I->useNames.push_back(a);
        } else if (I->isCondBr) {
        } else if (I->isUncondBr || I->isRet) {
            for (auto &a : args)
                if (isSSAName(a))
                    I->useNames.push_back(a);
        } else {
            if (!args.empty() && isSSAName(args[0]) && args[0][0] == 'v')
                I->def = args[0];
            for (size_t k = 1; k < args.size(); ++k)
                if (isSSAName(args[k]))
                    I->useNames.push_back(args[k]);
        }

        P.orderInstructions.push_back(I);
        pc += 4;
    }
    return P;
}

static vector<int> findStartInst(const Graph &P) {
    unordered_set<int> L;
    if (!P.orderInstructions.empty())
        L.insert(P.orderInstructions.front()->pc);
    auto after = [&](const Inst *I) { return I->pc + 4; };
    for (auto *I : P.orderInstructions) {
        if (I->isUncondBr)
            L.insert(I->target_pc);
        if (I->isCondBr) {
            L.insert(I->target_pc);
            L.insert(after(I));
        }
    }
    vector<int> v(L.begin(), L.end());
    sort(v.begin(), v.end());
    return v;
}

void buildBlocks(Graph &P) {
    auto startInst = findStartInst(P);
    for (size_t i = 0; i < startInst.size(); ++i) {
        int start = startInst[i];
        int end = (i + 1 < startInst.size() ? startInst[i + 1] : numeric_limits<int>::max());
        Block b;
        b.startPC = start;
        b.endPC = end;
        P.startInstPC2blockIdx[start] = (int)P.blocks.size();
        P.blocks.push_back(std::move(b));
    }
    P.entry = 0;
    size_t bi = 0;
    for (auto *I : P.orderInstructions) {
        while (bi + 1 < P.blocks.size() && I->pc >= P.blocks[bi].endPC)
            ++bi;
        P.blocks[bi].append(I);
    }
    auto after = [&](const Inst *I) { return I->pc + 4; };
    auto getBlockIdxWithLeaderPc = [&](int pc) -> int {
        auto it = P.startInstPC2blockIdx.find(pc);
        return it == P.startInstPC2blockIdx.end() ? -1 : it->second;
    };
    auto add = [&](int u, int v) {
        if (v < 0)
            return;
        P.blocks[u].successors.push_back(v);
        P.blocks[v].predecessors.push_back(u);
    };
    for (int b = 0; b < (int)P.blocks.size(); ++b) {
        Block &B = P.blocks[b];
        if (!B.tail)
            continue;
        Inst *last_block = B.tail;
        if (last_block->isUncondBr)
            add(b, getBlockIdxWithLeaderPc(last_block->target_pc));
        else if (last_block->isCondBr) {
            add(b, getBlockIdxWithLeaderPc(last_block->target_pc));
            add(b, getBlockIdxWithLeaderPc(after(last_block)));
        } else if (last_block->isRet) {
        } else {
            if (b + 1 < (int)P.blocks.size())
                add(b, b + 1);
        }
    }
}
