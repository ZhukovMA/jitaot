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
struct RPO {
    std::vector<BasicBlock *> rpo;

    void run(BasicBlock *start) {
        rpo.clear();
        std::vector<BasicBlock *> post;
        std::unordered_set<BasicBlock *> vis;
        std::function<void(BasicBlock *)> dfs = [&](BasicBlock *b) {
            if (!b || !vis.insert(b).second)
                return;
            for (auto *s : b->successors)
                dfs(s);
            post.push_back(b); 
        };
        dfs(start);
        rpo.assign(post.rbegin(), post.rend()); 
    }
};
} 