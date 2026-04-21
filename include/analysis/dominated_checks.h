#pragma once

#include "analysis/dominator_tree.h"
#include "analysis/ir_utils.h"
#include "analysis/rpo.h"
#include "ir/ir_graph.h"
#include <unordered_map>
#include <unordered_set>

namespace analysis {

struct DominatedChecksElimination {
    bool run(ir::IRGraph &g, ir::BasicBlock *entry) {
        (void)g;
        if (!entry) {
            return false;
        }

        RPO rpo;
        rpo.run(entry);

        DominatorTree dt;
        dt.build(entry);

        std::unordered_map<ir::Inst *, ir::BasicBlock *> inst_to_block;
        for (auto *bb : rpo.rpo) {
            for (auto *I : bb->allInsts()) {
                inst_to_block[I] = bb;
            }
        }

        auto dominatesBlock = [&](ir::BasicBlock *a, ir::BasicBlock *b) {
            if (a == b) {
                return true;
            }
            for (auto *cur = b; cur; ) {
                auto it = dt.idom_map.find(cur);
                if (cur == a) {
                    return true;
                }
                if (it == dt.idom_map.end()) {
                    break;
                }
                cur = it->second;
            }
            return false;
        };

        auto dominatesInst = [&](ir::Inst *a, ir::Inst *b) {
            auto ita = inst_to_block.find(a);
            auto itb = inst_to_block.find(b);
            if (ita == inst_to_block.end() || itb == inst_to_block.end()) {
                return false;
            }
            auto *bb_a = ita->second;
            auto *bb_b = itb->second;
            if (bb_a != bb_b) {
                return dominatesBlock(bb_a, bb_b);
            }
            for (auto *I : bb_a->allInsts()) {
                if (I == a) {
                    return true;
                }
                if (I == b) {
                    return false;
                }
            }
            return false;
        };

        std::unordered_set<ir::Inst *> to_remove;

        for (auto *bb : rpo.rpo) {
            (void)bb;
            for (auto *I : bb->allInsts()) {
                if (to_remove.count(I)) {
                    continue;
                }

                switch (I->opcode()) {
                case ir::Opcode::NULL_CHECK: {
                    auto *check = dynamic_cast<ir::NullCheckInst *>(I);
                    if (!check || !check->ref()) {
                        break;
                    }
                    auto users = check->ref()->users;
                    for (auto *U : users) {
                        if (U == I || to_remove.count(U) || U->opcode() != ir::Opcode::NULL_CHECK) {
                            continue;
                        }
                        auto *other = dynamic_cast<ir::NullCheckInst *>(U);
                        if (!other || other->ref() != check->ref()) {
                            continue;
                        }
                        if (dominatesInst(I, U)) {
                            to_remove.insert(U);
                        }
                    }
                    break;
                }
                case ir::Opcode::BOUNDS_CHECK: {
                    auto *check = dynamic_cast<ir::BoundsCheckInst *>(I);
                    if (!check || !check->index() || !check->length()) {
                        break;
                    }
                    auto users = check->index()->users;
                    for (auto *U : users) {
                        if (U == I || to_remove.count(U) || U->opcode() != ir::Opcode::BOUNDS_CHECK) {
                            continue;
                        }
                        auto *other = dynamic_cast<ir::BoundsCheckInst *>(U);
                        if (!other) {
                            continue;
                        }
                        if (other->index() != check->index() || other->length() != check->length()) {
                            continue;
                        }
                        if (dominatesInst(I, U)) {
                            to_remove.insert(U);
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }

        bool changed = false;
        for (auto *bb : rpo.rpo) {
            auto insts = bb->allInsts();
            for (auto *I : insts) {
                if (!to_remove.count(I)) {
                    continue;
                }
                eraseInst(bb, I);
                inst_to_block.erase(I);
                changed = true;
            }
        }
        return changed;
    }
};

} // namespace analysis
