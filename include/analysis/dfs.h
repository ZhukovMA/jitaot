#pragma once
#include <vector>
#include <unordered_set>
#include <functional>

namespace ir {
class BasicBlock;
}

#include "ir/basic_block.h"

namespace analysis {
using ir::BasicBlock;
struct DFS {
    std::vector<BasicBlock *> preorder;

    void run(BasicBlock *start) {
        preorder.clear();
        std::unordered_set<BasicBlock *> vis;
        std::function<void(BasicBlock *)> dfs = [&](BasicBlock *b) {
            if (!b || !vis.insert(b).second)
                return;
            preorder.push_back(b); 
            for (auto *s : b->successors)
                dfs(s);
        };
        dfs(start);
    }
};
} 