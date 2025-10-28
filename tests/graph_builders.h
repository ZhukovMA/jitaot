#pragma once
#include "analysis/dfs.h"
#include "analysis/dominator_tree.h"
#include "analysis/loop_analyzer.h"
#include "analysis/rpo.h"
#include "ir/ir_graph.h"
#include <map>
#include <string>

struct BuiltCFG {
    ir::IRGraph g;
    ir::BasicBlock *entry{};
    std::map<std::string, ir::BasicBlock *> byName;
};
inline ir::BasicBlock *BB(BuiltCFG &W, const std::string &name) {
    auto *b = W.g.createBlock(name);
    W.byName[name] = b;
    return b;
}
inline void EDGE(ir::BasicBlock *u, ir::BasicBlock *v) {
    u->addSuccessor(v);
}