#include "graph_builders.h"
#include <cassert>
#include <vector>
#include <map>
#include <string>
#include <set>

using namespace ir;
using namespace analysis;

void expectOrder(const std::vector<BasicBlock *> &got,
                        std::initializer_list<const char *> expect) {
    assert(got.size() == expect.size());
    size_t i = 0;
    for (auto *b : got) {
        auto it = expect.begin();
        std::advance(it, i);
        assert(b->label == *it && "order mismatch");
        ++i;
    }
}
void expectIdoms(const DominatorTree &DT, const BuiltCFG &W,
                        const std::map<std::string, std::string> &exp) {
    for (auto &[n, p] : exp) {
        BasicBlock *b = W.byName.at(n);
        BasicBlock *ip = p.empty() ? nullptr : W.byName.at(p);
        auto it = DT.idom_map.find(b);
        assert(it != DT.idom_map.end());
        assert(it->second == ip && "idom mismatch");
    }
}


static BuiltCFG buildExample1() {
    BuiltCFG W;
    auto *A = BB(W, "A");
    auto *B = BB(W, "B");
    auto *C = BB(W, "C");
    auto *E = BB(W, "E");
    auto *F = BB(W, "F");
    auto *D = BB(W, "D");
    auto *G = BB(W, "G");

    EDGE(A, B);
    EDGE(B, C);
    EDGE(B, F);
    EDGE(C, E);
    EDGE(C, D);
    EDGE(F, E);
    EDGE(F, G);
    EDGE(E, D);
    EDGE(G, D);

    W.entry = A;
    return W;
}


static BuiltCFG buildExample2() {
    BuiltCFG W;
    auto *A = BB(W, "A");
    auto *B = BB(W, "B");
    auto *C = BB(W, "C");
    auto *D = BB(W, "D");
    auto *E = BB(W, "E");
    auto *F = BB(W, "F");
    auto *G = BB(W, "G");
    auto *H = BB(W, "H");
    auto *I = BB(W, "I");
    auto *J = BB(W, "J");
    auto *K = BB(W, "K");

    EDGE(A, B);
    EDGE(B, C);
    EDGE(B, J);
    EDGE(C, D);
    EDGE(D, C);
    EDGE(D, E);
    EDGE(E, F);
    EDGE(F, E);
    EDGE(F, G);
    EDGE(G, H);
    EDGE(G, I);
    EDGE(I, K);
    EDGE(H, B);
    EDGE(J, C);

    W.entry = A;
    return W;
}


static BuiltCFG buildExample3() {
    BuiltCFG W;
    auto *A = BB(W, "A");
    auto *B = BB(W, "B");
    auto *C = BB(W, "C");
    auto *D = BB(W, "D");
    auto *E = BB(W, "E");
    auto *F = BB(W, "F");
    auto *G = BB(W, "G");
    auto *H = BB(W, "H");
    auto *I = BB(W, "I");

    EDGE(A, B);
    EDGE(B, C);
    EDGE(B, E);
    EDGE(C, D);
    EDGE(E, F);
    EDGE(E, D);
    EDGE(F, H);
    EDGE(D, G);
    EDGE(H, I);
    EDGE(G, I);
    
    EDGE(H, B);
    EDGE(G, C);

    W.entry = A;
    return W;
}


void testExample1() {
    auto W = buildExample1();

    DFSOrder dfs;
    dfs.run(W.entry);
    expectOrder(dfs.preorder, {"A", "B", "C", "E", "D", "F", "G"});

    RPOOrder rpo;
    rpo.run(W.entry);
    expectOrder(rpo.rpo, {"A", "B", "F", "G", "C", "E", "D"});

    DominatorTree DT;
    DT.build(W.entry);
    expectIdoms(DT, W, {
                           {"A", ""},
                           {"B", "A"},
                           {"C", "B"},
                           {"F", "B"},
                           {"E", "B"},
                           {"G", "F"},
                           {"D", "B"},
                       });
}

void testExample2() {
    auto W = buildExample2();

    DFSOrder dfs;
    dfs.run(W.entry);
    expectOrder(dfs.preorder, {"A", "B", "C", "D", "E", "F", "G", "H", "I", "K", "J"});

    RPOOrder rpo;
    rpo.run(W.entry);
    expectOrder(rpo.rpo, {"A", "B", "J", "C", "D", "E", "F", "G", "I", "K", "H"});

    DominatorTree DT;
    DT.build(W.entry);
    expectIdoms(DT, W, {
                           {"A", ""},
                           {"B", "A"},
                           {"J", "B"},
                           {"C", "B"},
                           {"D", "C"},
                           {"E", "D"},
                           {"F", "E"},
                           {"G", "F"},
                           {"H", "G"},
                           {"I", "G"},
                           {"K", "I"},
                       });
}

void testExample3() {
    auto W = buildExample3();

    DFSOrder dfs;
    dfs.run(W.entry);
    expectOrder(dfs.preorder, {"A", "B", "C", "D", "G", "I", "E", "F", "H"});

    RPOOrder rpo;
    rpo.run(W.entry);
    expectOrder(rpo.rpo, {"A", "B", "E", "F", "H", "C", "D", "G", "I"});

    DominatorTree DT;
    DT.build(W.entry);
    expectIdoms(DT, W, {
                           {"A", ""},
                           {"B", "A"},
                           {"C", "B"},
                           {"E", "B"},
                           {"D", "B"}, 
                           {"F", "E"},
                           {"G", "D"},
                           {"H", "F"},
                           {"I", "B"}, 
                       });
}