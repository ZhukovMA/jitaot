#include "analysis.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>

using namespace std;

std::vector<int> computeRPO(const Graph &P) {
    const int N = (int)P.blocks.size();
    std::vector<char> vis(N, 0);
    std::vector<int> post, rpo;
    if (P.entry < 0 || P.entry >= N)
        return rpo;

    std::function<void(int)> dfs = [&](int b) {
        vis[b] = 1;
        for (int s : P.blocks[b].successors) {
            if (s < 0 || s >= N)
                continue;
            if (!vis[s])
                dfs(s);
        }
        post.push_back(b);
    };
    dfs(P.entry);

    rpo.assign(post.rbegin(), post.rend());
    return rpo;
}

DomInfo computeDominators(const Graph &P) {
    const int N = (int)P.blocks.size();
    if (N == 0)
        return {};
    const int ROOT = P.entry;

    vector<int> parent(N, -1), sdom(N, -1), idom(N, -1), ancestor(N, -1), label(N, -1), vertex(N + 1, -1);
    vector<vector<int>> bucket(N);
    int T = 0;

    function<void(int)> dfs = [&](int v) {
        sdom[v] = ++T;
        vertex[T] = v;
        label[v] = v;
        for (int w : P.blocks[v].successors)
            if (sdom[w] == -1) {
                parent[w] = v;
                dfs(w);
            }
    };
    if (ROOT >= 0)
        dfs(ROOT);
    if (T == 0)
        return {vector<unordered_set<int>>(N), vector<int>(N, -1), vector<vector<int>>(N), vector<unordered_set<int>>(N)};

    function<int(int)> eval = [&](int v) -> int {
        if (ancestor[v] == -1)
            return label[v];
        vector<int> path;
        int u = v;
        while (ancestor[u] != -1 && ancestor[ancestor[u]] != -1) {
            path.push_back(u);
            u = ancestor[u];
        }
        for (int i = (int)path.size() - 1; i >= 0; --i) {
            int x = path[i];
            int a = ancestor[x];
            if (sdom[label[a]] < sdom[label[x]])
                label[x] = label[a];
            ancestor[x] = ancestor[a];
        }
        int a = ancestor[v];
        if (a != -1 && sdom[label[a]] < sdom[label[v]])
            label[v] = label[a];
        return label[v];
    };

    for (int i = T; i >= 2; --i) {
        int w = vertex[i];
        for (int v : P.blocks[w].predecessors) {
            if (v < 0 || v >= N)
                continue;
            if (sdom[v] == -1)
                continue;
            int u = eval(v);
            if (sdom[u] < sdom[w])
                sdom[w] = sdom[u];
        }
        bucket[vertex[sdom[w]]].push_back(w);
        ancestor[w] = parent[w];
        auto &B = bucket[parent[w]];
        for (int v : B) {
            int u = eval(v);
            idom[v] = (sdom[u] < sdom[v]) ? u : parent[w];
        }
        B.clear();
    }
    for (int i = 2; i <= T; ++i) {
        int w = vertex[i];
        if (idom[w] != vertex[sdom[w]])
            idom[w] = idom[idom[w]];
    }
    idom[ROOT] = -1;

    vector<vector<int>> kids(N);
    for (int v = 0; v < N; ++v)
        if (idom[v] >= 0)
            kids[idom[v]].push_back(v);
    vector<unordered_set<int>> DF(N);
    for (int b = 0; b < N; ++b) {
        if (sdom[b] == -1)
            continue;
        if (P.blocks[b].predecessors.size() < 2)
            continue;
        for (int p : P.blocks[b].predecessors) {
            if (p < 0 || p >= N)
                continue;
            if (sdom[p] == -1)
                continue;
            int r = p;
            while (r != idom[b] && r != -1) {
                DF[r].insert(b);
                r = idom[r];
            }
        }
    }

    DomInfo out{vector<unordered_set<int>>(N), std::move(idom), std::move(kids), std::move(DF)};
    return out;
}

void placePhi(Graph &P, const DomInfo &D) {
    unordered_map<string, unordered_set<int>> defB;
    unordered_set<string> registers;
    for (int b = 0; b < (int)P.blocks.size(); ++b)
        for (Inst *I = P.blocks[b].head; I; I = I->next) {
            if (isRegister(I->def)) {
                registers.insert(I->def);
                defB[I->def].insert(b);
            }
            for (auto &u : I->useNames)
                if (isRegister(u))
                    registers.insert(u);
        }
    for (const string &v : registers) {
        unordered_set<int> hasPhi;
        queue<int> W;
        for (int b : defB[v])
            W.push(b);
        while (!W.empty()) {
            int X = W.front();
            W.pop();
            for (int Y : D.DF[X])
                if (!hasPhi.count(Y)) {
                    auto &BB = P.blocks[Y];
                    BB.phiArgs[v].assign(BB.predecessors.size(), "");
                    hasPhi.insert(Y);
                    if (!defB[v].count(Y))
                        W.push(Y);
                }
        }
    }
}

std::string RS::toSSAName(const std::string &b) {
    return b + "_" + std::to_string(nextId++);
}
std::string RS::top(const std::string &b) {
    auto it = st.find(b);
    return (it == st.end() || it->second.empty()) ? "" : it->second.back();
}
void RS::push(const std::string &b, const std::string &n) {
    st[b].push_back(n);
}
void RS::pop(const std::string &b) {
    auto &v = st[b];
    if (!v.empty())
        v.pop_back();
}

void rename2SSA(Graph &P,
                const DomInfo &D,
                int b,
                int &nextId,
                std::unordered_map<std::string, std::string> cur) {
    auto toSSAName = [&](const std::string &base) -> std::string {
        return base + "_" + std::to_string(nextId++);
    };

    Block &BB = P.blocks[b];

    for (auto &kv : BB.phiArgs) {
        const std::string &base = kv.first;
        std::string name = toSSAName(base); // v0_*
        BB.phiDef[base] = name;
        cur[base] = name;
    }

    // переименовываем use/def внутри блока
    for (Inst *I = BB.head; I; I = I->next) {
        for (auto &u : I->useNames)
            if (isRegister(u)) {
                auto it = cur.find(u);
                if (it == cur.end()) {
                    std::string z = toSSAName(u);
                    cur[u] = z;
                    u = z;
                } else {
                    u = it->second;
                }
            }
        if (isRegister(I->def)) {
            std::string base = I->def;
            std::string ver = toSSAName(base);
            I->def = ver;
            cur[base] = ver;
        }
    }

    // запоминаем фи-аргументы у всех преемников по ребру
    for (int s : BB.successors) {
        auto &SB = P.blocks[s];
        auto itp = std::find(SB.predecessors.begin(), SB.predecessors.end(), b);
        if (itp == SB.predecessors.end())
            continue;
        int predIdx = int(itp - SB.predecessors.begin());

        for (auto &kv : SB.phiArgs) {
            const std::string &base = kv.first;
            std::string val;
            auto it = cur.find(base);
            if (it == cur.end()) {
                val = toSSAName(base);
                cur[base] = val;
            } else {
                val = it->second;
            }
            kv.second[predIdx] = val;
        }
    }

    for (int c : D.kids[b]) {
        rename2SSA(P, D, c, nextId, cur);
    }
}

void phi2Instructions(Graph &P) {
    for (auto &BB : P.blocks)
        for (auto &kv : BB.phiArgs) {
            auto I_unique = make_unique<Inst>();
            Inst *I = I_unique.get();
            I->pc = BB.startPC;
            I->op = "phi";
            I->def = BB.phiDef[kv.first];
            I->useNames = kv.second;
            I->text = I->def + " = phi(...)";
            BB.prepend(I);
            P.orderInstructions.push_back(I);
            P.instructions.emplace_back(std::move(I_unique));
        }
}

void buildUseDefLinks(Graph &P) {
    unordered_map<string, Inst *> defOf;
    for (auto &BB : P.blocks)
        for (Inst *I = BB.head; I; I = I->next) {
            I->inputs.clear();
            for (auto &u : I->useNames) {
                auto it = defOf.find(u);
                if (it != defOf.end()) {
                    Inst *defI = it->second;
                    I->inputs.push_back(defI);
                    defI->users.push_back(I);
                } else
                    I->inputs.push_back(nullptr);
            }
            if (!I->def.empty())
                defOf[I->def] = I;
        }
}

void dumpIR(const Graph &P) {
    cout << "\nSSA:\n";
    for (int b = 0; b < (int)P.blocks.size(); ++b) {
        const auto &BB = P.blocks[b];
        cout << "B" << b << "PC" << BB.startPC << ":\n";
        for (Inst *I = BB.head; I; I = I->next) {
            if (!I->def.empty())
                cout << "  " << I->def << " = " << I->op;
            else
                cout << "  " << I->op;
            if (!I->useNames.empty()) {
                cout << " ";
                for (size_t i = 0; i < I->useNames.size(); ++i) {
                    if (i)
                        cout << ", ";
                    cout << I->useNames[i];
                }
            }
            cout << "\n";
        }
    }
}

static inline const std::vector<int> &predsOf(const Graph &P, int b) {
    return P.blocks[b].predecessors;
}
static inline const std::vector<int> &succsOf(const Graph &P, int b) {
    return P.blocks[b].successors;
}

bool dominatesViaIdom(const DomInfo &D, int a, int b) {
    for (int x = b; x != -1; x = D.idom[x])
        if (x == a)
            return true;
    return false;
}

bool runUnitTests(const Graph &P, const DomInfo &D, std::string *outErr) {
    auto fail = [&](const std::string &msg) { if(outErr) *outErr = msg; return false; };
    const int N = (int)P.blocks.size();

    int header = -1;
    for (int b = 0; b < N; ++b) {
        auto &B = P.blocks[b];
        const auto &PR = predsOf(P, b);
        if (B.tail && B.tail->isCondBr && (int)PR.size() >= 2) {
            header = b;
            break;
        }
    }
    if (header < 0)
        return fail("header not found");

    int preheader = -1, latch = -1;
    for (int p : predsOf(P, header)) {
        if (P.blocks[p].startPC < P.blocks[header].startPC)
            preheader = p;
        else
            latch = p;
    }
    if (preheader < 0 || latch < 0)
        return fail("preheader/latch not found");

    int body = -1, exitb = -1;
    for (int s : succsOf(P, header)) {
        const auto &S = P.blocks[s];
        if (S.tail && S.tail->isRet)
            exitb = s;
        else
            body = s;
    }
    if (body < 0 || exitb < 0)
        return fail("body/exit not found");

    auto eq = [&](int a, int b) { return a == b; };
    if (!eq(D.idom[header], preheader))
        return fail("expected idom[header]=preheader, got idom[header]=" + std::to_string(D.idom[header]) + " preheader=" + std::to_string(preheader));
    if (!eq(D.idom[body], header))
        return fail("expected idom[body]=header");
    if (!eq(D.idom[exitb], header))
        return fail("expected idom[exit]=header");

    if (!dominatesViaIdom(D, header, body))
        return fail("header must dominate body");
    if (!dominatesViaIdom(D, header, exitb))
        return fail("header must dominate exit");
    if (!dominatesViaIdom(D, preheader, header))
        return fail("preheader must dominate header");
    if (!dominatesViaIdom(D, preheader, body))
        return fail("preheader must dominate body");
    if (!dominatesViaIdom(D, preheader, exitb))
        return fail("preheader must dominate exit");

    return true;
}

void printDominatorTree(const Graph &P, const DomInfo &D) {
    const int N = (int)P.blocks.size();
    vector<vector<int>> kids(N);
    for (int v = 0; v < N; ++v) {
        if (v == P.entry)
            continue;
        int p = D.idom[v];
        if (p >= 0 && p < N)
            kids[p].push_back(v);
    }
    function<void(int, int)> dfs = [&](int v, int depth) { cout << string(depth*2, ' ') << "B" + to_string(v) + "(PC" + to_string(P.blocks[v].startPC) + ")" << "\n"; for (int u : kids[v]) dfs(u, depth+1); };
    cout << "\nDominator Tree:\n";
    if (P.entry >= 0 && P.entry < N)
        dfs(P.entry, 0);
    cout << "\n";
}
