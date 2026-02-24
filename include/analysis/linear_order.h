#pragma once

#include "analysis/loop_analyzer.h"
#include "analysis/rpo.h"
#include "ir/basic_block.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace analysis {

struct LinearOrder {
    std::vector<ir::BasicBlock *> blocks;
    std::unordered_map<ir::BasicBlock *, int> index;
    std::unordered_map<ir::BasicBlock *, ir::BasicBlock *> loopEnd;

    analysis::RPO rpo;
    analysis::LoopAnalyzer loops;

  private:
    static int loopDepth_(const analysis::Loop *L) {
        int d = 0;
        for (auto *p = L ? L->parent : nullptr; p && p->parent; p = p->parent) {
            ++d;
        }
        return d;
    }

    void rebuildIndex_() {
        index.clear();
        for (int i = 0; i < (int)blocks.size(); ++i) {
            index[blocks[i]] = i;
        }
    }

    void makeLoopContiguous_(const analysis::Loop *L) {
        if (!L || L->irreducible) {
            return;
        }
        auto *H = L->header;
        if (!H)
            return;
        auto itH = index.find(H);
        if (itH == index.end()) {
            return;
        }
        const int hpos = itH->second;

        std::unordered_set<ir::BasicBlock *> inLoop;
        inLoop.reserve(L->blocks.size() * 2 + 1);
        for (auto *b : L->blocks) {
            inLoop.insert(b);
        }

        std::vector<ir::BasicBlock *> moved;
        moved.reserve(inLoop.size());
        for (int i = hpos + 1; i < (int)blocks.size(); ++i) {
            auto *b = blocks[i];
            if (inLoop.count(b)) {
                moved.push_back(b);
            }
        }
        if (moved.empty()) {
            return;
        }

        std::vector<ir::BasicBlock *> filtered;
        filtered.reserve(blocks.size());
        filtered.insert(filtered.end(), blocks.begin(), blocks.begin() + (hpos + 1));
        for (int i = hpos + 1; i < (int)blocks.size(); ++i) {
            auto *b = blocks[i];
            if (!inLoop.count(b)) {
                filtered.push_back(b);
            }
        }

        filtered.insert(filtered.begin() + (hpos + 1), moved.begin(), moved.end());
        blocks.swap(filtered);
        rebuildIndex_();
    }

    void computeLoopEnds_() {
        loopEnd.clear();
        for (auto &up : loops.loops) {
            auto *L = up.get();
            if (!L || L->irreducible || !L->header) {
                continue;
            }
            ir::BasicBlock *end = L->header;
            int best = index[end];
            for (auto *b : L->blocks) {
                auto it = index.find(b);
                if (it != index.end() && it->second > best) {
                    best = it->second;
                    end = b;
                }
            }
            loopEnd[L->header] = end;
        }
    }

  public:
    void run(ir::BasicBlock *entry) {
        blocks.clear();
        index.clear();
        loopEnd.clear();
        if (!entry) {
            return;
        }

        rpo.run(entry);
        blocks = rpo.rpo;
        rebuildIndex_();

        loops.run(entry);

        std::vector<const analysis::Loop *> all;
        all.reserve(loops.loops.size());
        for (auto &up : loops.loops) {
            all.push_back(up.get());
        }

        std::sort(all.begin(), all.end(), [&](const analysis::Loop *a, const analysis::Loop *b) {
            const int da = loopDepth_(a);
            const int db = loopDepth_(b);
            if (da != db) {
                return da > db;
            }
            int ia = (a && a->header && index.count(a->header)) ? index[a->header] : -1;
            int ib = (b && b->header && index.count(b->header)) ? index[b->header] : -1;
            return ia > ib;
        });
        for (auto *L : all) {
            makeLoopContiguous_(L);
        }

        computeLoopEnds_();
    }
};

} // namespace analysis
