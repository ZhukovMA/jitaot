
#include "graph_builders.h"
#include <cassert>
#include <vector>
#include <set>
#include <string>

using namespace ir;
using namespace analysis;

void setEq(const std::vector<BasicBlock *> &got,
                  std::initializer_list<const char *> exp) {
    std::set<std::string> A, B;
    for (auto *x : got)
        A.insert(x->label);
    for (auto *s : exp)
        B.insert(s);
    assert(A == B && "set mismatch");
}

Loop *findLoopByHeader(const LoopAnalyzer &LA, BasicBlock *hdr) {
    for (auto &up : LA.loops)
        if (up->header == hdr)
            return up.get();
    return nullptr;
}

bool hasChild(const Loop *parent, const Loop *child) {
    for (auto *c : parent->children)
        if (c == child)
            return true;
    return false;
}

static BuiltCFG buildLoop1() {
    BuiltCFG W;
    auto *S = BB(W, "S");
    auto *H = BB(W, "H");
    auto *L = BB(W, "L");
    auto *M = BB(W, "M");
    auto *Z = BB(W, "Z");
    EDGE(S, H);
    EDGE(H, L);
    EDGE(H, M);
    EDGE(M, Z);
    EDGE(Z, H); 
    W.entry = S;
    return W;
}

static BuiltCFG buildLoop2() {
    BuiltCFG W;
    auto *S = BB(W, "S");
    auto *H = BB(W, "H");
    auto *X = BB(W, "X");
    auto *Y = BB(W, "Y");
    auto *Z = BB(W, "Z");
    auto *T = BB(W, "T");
    EDGE(S, H);
    EDGE(H, X);
    EDGE(H, Z);
    EDGE(X, Y);
    EDGE(Y, Z);
    EDGE(Z, H); 
    EDGE(X, T);
    EDGE(Y, T);
    W.entry = S;
    return W;
}


static BuiltCFG buildLoop3() {
    BuiltCFG W;
    auto *A = BB(W, "A"); 
    auto *B = BB(W, "B"); 
    auto *C1 = BB(W, "C1");
    auto *C2 = BB(W, "C2");
    auto *J = BB(W, "J");
    auto *D1 = BB(W, "D1");
    auto *E = BB(W, "E");    
    auto *SINK = BB(W, "S"); 
    EDGE(A, B);
    EDGE(B, C1);
    EDGE(B, C2);
    EDGE(C1, J);
    EDGE(C2, J);
    EDGE(J, D1);
    EDGE(D1, E);
    EDGE(E, A);     
    EDGE(D1, B);    
    EDGE(C1, SINK); 
    W.entry = A;
    return W;
}


static BuiltCFG buildLoopsExample1() {
    
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
    EDGE(E, D);
    EDGE(F, E);
    EDGE(F, G);
    EDGE(G, D);
    W.entry = A;
    return W;
}

static BuiltCFG buildLoopsExample2() {
    
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

static BuiltCFG buildLoopsExample3() {
    
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


void test1() {
    auto W = buildLoop1();
    LoopAnalyzer LA;
    LA.run(W.entry);

    assert(LA.loops.size() == 1);
    Loop *L = findLoopByHeader(LA, W.byName["H"]);
    assert(L && !L->irreducible);
    setEq(L->latches, {"Z"});
    setEq(L->blocks, {"H", "M", "Z"}); 
    
    assert(L->parent == LA.rootLoop);
}

void test2() {
    auto W = buildLoop2();
    LoopAnalyzer LA;
    LA.run(W.entry);

    assert(LA.loops.size() == 1);
    Loop *L = findLoopByHeader(LA, W.byName["H"]);
    assert(L && !L->irreducible);
    setEq(L->latches, {"Z"});
    
    setEq(L->blocks, {"H", "X", "Y", "Z"});
    assert(L->parent == LA.rootLoop);
}

void test3() {
    auto W = buildLoop3();
    LoopAnalyzer LA;
    LA.run(W.entry);

    
    assert(LA.loops.size() == 2);
    Loop *outer = findLoopByHeader(LA, W.byName["A"]);
    Loop *inner = findLoopByHeader(LA, W.byName["B"]);
    assert(outer && inner);
    assert(!outer->irreducible && !inner->irreducible);

    setEq(outer->latches, {"E"});
    setEq(inner->latches, {"D1"});

    
    setEq(inner->blocks, {"B", "C1", "C2", "J", "D1"});
    
    std::set<std::string> outerSet;
    for (auto *b : outer->blocks)
        outerSet.insert(b->label);
    for (const char *s : {"A", "B", "C1", "C2", "J", "D1", "E"})
        assert(outerSet.count(s));

    
    assert(inner->parent == outer);
    assert(hasChild(outer, inner));
    assert(outer->parent == LA.rootLoop);
}


void testLoopsExample1() {
    auto W = buildLoopsExample1();
    LoopAnalyzer LA;
    LA.run(W.entry);
    assert(LA.loops.empty() && "Example1 must have no loops");
}

void testLoopsExample2() {
    auto W = buildLoopsExample2();
    LoopAnalyzer LA;
    LA.run(W.entry);


    Loop *LB = findLoopByHeader(LA, W.byName["B"]);
    Loop *LC = findLoopByHeader(LA, W.byName["C"]);
    Loop *LE = findLoopByHeader(LA, W.byName["E"]);
    assert(LB && LC && LE);

    assert(!LB->irreducible);
    assert(!LC->irreducible);
    assert(!LE->irreducible);

    setEq(LB->latches, {"H"});
    setEq(LC->latches, {"D"});
    setEq(LE->latches, {"F"});

    assert(LC->parent == LB && LE->parent == LB);
    assert(hasChild(LB, LC) && hasChild(LB, LE));
    assert(LB->parent == LA.rootLoop);
}

void testLoopsExample3() {
    auto W = buildLoopsExample3();
    LoopAnalyzer LA;
    LA.run(W.entry);

    Loop *LB = findLoopByHeader(LA, W.byName["B"]);
    Loop *LC = findLoopByHeader(LA, W.byName["C"]);
    assert(LB && LC);

    assert(!LB->irreducible);
    assert(LC->irreducible);

    setEq(LB->latches, {"H"});
    setEq(LC->latches, {"G"});

    assert(LC->parent == LB);
    assert(hasChild(LB, LC));
    assert(LB->parent == LA.rootLoop);
}