#pragma once
#include "analysis/ir_utils.h"
#include "analysis/rpo.h"
#include "ir/ir_graph.h"

namespace analysis {

struct ConstantFolding {
    bool run(ir::IRGraph &g, ir::BasicBlock *entry) {
        RPO rpo;
        rpo.run(entry);
        bool any = false, changed = true;
        while (changed) {
            changed = false;
            for (auto *bb : rpo.rpo) {
                auto insts = bb->allInsts();
                for (auto *I : insts) {
                    if (!I) {
                        continue;
                    }

                    if (foldMul(g, bb, I)) {
                        changed = true;
                        continue;
                    }
                    if (foldAnd(g, bb, I)) {
                        changed = true;
                        continue;
                    }
                    if (foldShl(g, bb, I)) {
                        changed = true;
                        continue;
                    }
                }
            }
            any |= changed;
        }
        return any;
    }

  private:
    static void replaceWithConst(ir::IRGraph &g, ir::BasicBlock *bb, ir::Inst *I, uint64_t c) {
        auto *k = makeConstBefore(bb, I, g, c);
        replaceAllUsesWith(I->result(), k);
        eraseInst(bb, I);
    }
    static void replaceWithValue(ir::BasicBlock *bb, ir::Inst *I, ir::SSAValue *v) {
        replaceAllUsesWith(I->result(), v);
        eraseInst(bb, I);
    }

    bool foldMul(ir::IRGraph &g, ir::BasicBlock *bb, ir::Inst *I) {
        using ir::Opcode;
        using ir::SSAValue;
        if (I->opcode() != Opcode::MUL_U64)
            return false;
        auto ops = I->operands();
        auto *x = std::get<SSAValue *>(ops[0]);
        auto *y = std::get<SSAValue *>(ops[1]);
        uint64_t a, b;
        if (getConstU64(x, a) && a == 0) {
            replaceWithConst(g, bb, I, 0);
            return true;
        }
        if (getConstU64(y, b) && b == 0) {
            replaceWithConst(g, bb, I, 0);
            return true;
        }
        if (getConstU64(x, a) && a == 1) {
            replaceWithValue(bb, I, y);
            return true;
        }
        if (getConstU64(y, b) && b == 1) {
            replaceWithValue(bb, I, x);
            return true;
        }
        if (getConstU64(x, a) && getConstU64(y, b)) {
            replaceWithConst(g, bb, I, (uint64_t)(a * b));
            return true;
        }
        return false;
    }

    bool foldAnd(ir::IRGraph &g, ir::BasicBlock *bb, ir::Inst *I) {
        using ir::Opcode;
        using ir::SSAValue;
        if (I->opcode() != Opcode::AND_U64)
            return false;
        auto ops = I->operands();
        auto *x = std::get<SSAValue *>(ops[0]);
        auto *y = std::get<SSAValue *>(ops[1]);
        uint64_t a, b;
        if (getConstU64(x, a) && a == 0) {
            replaceWithConst(g, bb, I, 0);
            return true;
        }
        if (getConstU64(y, b) && b == 0) {
            replaceWithConst(g, bb, I, 0);
            return true;
        }
        if (getConstU64(y, b) && b == allOnes()) {
            replaceWithValue(bb, I, x);
            return true;
        }
        if (getConstU64(x, a) && a == allOnes()) {
            replaceWithValue(bb, I, y);
            return true;
        }
        if (x == y) {
            replaceWithValue(bb, I, x);
            return true;
        }
        if (getConstU64(x, a) && getConstU64(y, b)) {
            replaceWithConst(g, bb, I, (uint64_t)(a & b));
            return true;
        }
        return false;
    }

    bool foldShl(ir::IRGraph &g, ir::BasicBlock *bb, ir::Inst *I) {
        using ir::Opcode;
        using ir::SSAValue;
        if (I->opcode() != Opcode::SHL_U64)
            return false;
        auto ops = I->operands();
        auto *x = std::get<SSAValue *>(ops[0]);
        auto *s = std::get<SSAValue *>(ops[1]);
        uint64_t vx, k;
        if (getConstU64(x, vx) && vx == 0) {
            replaceWithConst(g, bb, I, 0);
            return true;
        }
        if (getConstU64(s, k) && k == 0) {
            replaceWithValue(bb, I, x);
            return true;
        }
        if (getConstU64(x, vx) && getConstU64(s, k)) {
            uint64_t amt = (k >= 64) ? 64 : k;
            uint64_t res = (amt == 64) ? 0ULL : (vx << amt);
            replaceWithConst(g, bb, I, res);
            return true;
        }
        return false;
    }
};

} // namespace analysis