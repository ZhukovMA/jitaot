#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <unordered_set>

namespace ir {
class BasicBlock;
}

#include "ir/basic_block.h"
#include "ir/opcode.h"

namespace analysis {
using ir::BasicBlock;
struct Loop {
    BasicBlock *header = nullptr;
    std::vector<BasicBlock *> latches; 
    std::vector<BasicBlock *> blocks;  
    bool irreducible = false;

    Loop *parent = nullptr;
    std::vector<Loop *> children;
};

struct LoopAnalyzer {
    
    std::vector<std::unique_ptr<Loop>> loops;             
    std::unordered_map<BasicBlock *, Loop *> loopOfBlock; 
    Loop *rootLoop = nullptr;                             

    
    DominatorTree DT;
    std::unordered_map<BasicBlock *, int> dfsNum;                 
    std::vector<BasicBlock *> preorder;                           
    std::vector<std::pair<BasicBlock *, BasicBlock *>> backEdges; 

    void run(BasicBlock *entry) {
        loops.clear();
        loopOfBlock.clear();
        rootLoop = nullptr;
        backEdges.clear();
        dfsNum.clear();
        preorder.clear();

        
        DT.build(entry);

        
        collectBackEdges(entry);

        
        populateLoops(entry);

        
        buildLoopTree(entry);
    }

    
    void collectBackEdges(BasicBlock *start) {
        enum Color {
            WHITE,
            GRAY,
            BLACK
        };
        std::unordered_map<BasicBlock *, Color> color;

        int time = 0;
        std::function<void(BasicBlock *)> dfs = [&](BasicBlock *b) {
            if (!b)
                return;
            color[b] = GRAY;
            dfsNum[b] = ++time;
            preorder.push_back(b);

            for (auto *s : b->successors) {
                if (!s)
                    continue;
                Color cs = color.count(s) ? color[s] : WHITE;
                if (cs == WHITE) {
                    dfs(s);
                } else if (cs == GRAY) {
                    
                    backEdges.emplace_back(b, s);
                }
            }
            color[b] = BLACK;
        };
        dfs(start);
    }

    
    void populateLoops(BasicBlock *entry) {
        
        std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> srcs;
        for (auto &[n, d] : backEdges)
            srcs[d].push_back(n);

        
        std::vector<BasicBlock *> headers;
        headers.reserve(srcs.size());
        for (auto &kv : srcs)
            headers.push_back(kv.first);
        std::sort(headers.begin(), headers.end(),
                  [&](auto *a, auto *b) { return dfsNum[a] > dfsNum[b]; });

        
        auto dominates = [&](BasicBlock *A, BasicBlock *B) {
            for (auto *cur = B; cur; cur = DT.idom_map[cur])
                if (cur == A)
                    return true;
            return false;
        };

        for (auto *H : headers) {
            auto L = std::make_unique<Loop>();
            L->header = H;

            
            for (auto *src : srcs[H]) {
                L->latches.push_back(src);
                if (!dominates(H, src))
                    L->irreducible = true;
            }

            
            
            std::unordered_set<BasicBlock *> inLoop;
            inLoop.insert(H);

            std::vector<BasicBlock *> stack;
            for (auto *s : srcs[H])
                stack.push_back(s);

            while (!stack.empty()) {
                BasicBlock *X = stack.back();
                stack.pop_back();
                if (!inLoop.insert(X).second)
                    continue; 
                
                if (auto it = loopOfBlock.find(X); it != loopOfBlock.end()) {
                    Loop *inner = it->second;
                    
                    if (inner != L.get() && !isAncestor(inner, L.get())) {
                        inner->parent = L.get();
                        L->children.push_back(inner);
                    }
                    
                }
                
                for (auto *P : X->predecessors) {
                    if (!P || P == H) {
                        inLoop.insert(H);
                        continue;
                    }
                    stack.push_back(P);
                }
            }

            
            L->blocks.assign(inLoop.begin(), inLoop.end());

            
            
            for (auto *b : L->blocks) {
                if (b == H)
                    continue;
                auto it = loopOfBlock.find(b);
                if (it == loopOfBlock.end())
                    loopOfBlock[b] = L.get();
            }

            loops.push_back(std::move(L));
        }
    }

    
    static bool isAncestor(Loop *L1, Loop *L2) {
        for (auto *p = L2 ? L2->parent : nullptr; p; p = p->parent)
            if (p == L1)
                return true;
        return false;
    }

    
    void buildLoopTree(BasicBlock *entry) {
        
        rootLoop = new Loop();
        rootLoop->header = nullptr; 
        
        for (auto &up : loops) {
            if (up->parent == nullptr) {
                up->parent = rootLoop;
                rootLoop->children.push_back(up.get());
            }
        }
    }
};
} 