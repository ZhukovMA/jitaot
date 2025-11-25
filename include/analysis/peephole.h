#pragma once
#include "analysis/ir_utils.h"
#include "analysis/rpo.h"
#include "ir/ir_graph.h"

namespace opt {

struct Peephole {
    explicit Peephole(ir::IRGraph &g) : g_(g) {
    }

    void run(ir::BasicBlock *entry) {
        analysis::RPO rpo;
        rpo.run(entry);

        bool updated = true;
        while (updated) {
            updated = false;
            for (auto *bb : rpo.rpo) {
                auto insts = bb->allInsts();
                for (auto *I : insts) {
                    if (!I)
                        continue;

                    if (visitMul(bb, I)) {
                        updated = true;
                        continue;
                    }
                    if (visitAnd(bb, I)) {
                        updated = true;
                        continue;
                    }
                    if (visitShl(bb, I)) {
                        updated = true;
                        continue;
                    }
                }
            }
        }
    }

  private:
    ir::IRGraph &g_;

    static void replaceWithConst(ir::IRGraph &g, ir::BasicBlock *bb, ir::Inst *I, uint64_t c) {
        auto *k = analysis::makeConstBefore(bb, I, g, c);
        analysis::replaceAllUsesWith(I->result(), k);
        analysis::eraseInst(bb, I);
    }
    static void replaceWithValue(ir::BasicBlock *bb, ir::Inst *I, ir::SSAValue *v) {
        analysis::replaceAllUsesWith(I->result(), v);
        analysis::eraseInst(bb, I);
    }

    bool visitMul(ir::BasicBlock *bb, ir::Inst *I) {
        using ir::Opcode;
        if (I->opcode() != Opcode::MUL_U64)
            return false;

        auto ops = I->operands();
        auto *x = std::get<ir::SSAValue *>(ops[0]);
        auto *y = std::get<ir::SSAValue *>(ops[1]);

        uint64_t a, b;

        // x * 1 -> x ; 1 * x -> x
        if (analysis::getConstU64(x, a) && a == 1) {
            replaceWithValue(bb, I, y);
            return true;
        }
        if (analysis::getConstU64(y, b) && b == 1) {
            replaceWithValue(bb, I, x);
            return true;
        }

        // x * 0 -> 0 ; 0 * x -> 0
        if (analysis::getConstU64(x, a) && a == 0) {
            replaceWithConst(g_, bb, I, 0);
            return true;
        }
        if (analysis::getConstU64(y, b) && b == 0) {
            replaceWithConst(g_, bb, I, 0);
            return true;
        }

        // x * (2^k) -> shl(x, k)
        if (analysis::getConstU64(y, b) && b && (b & (b - 1)) == 0) {
            unsigned k = (unsigned)__builtin_ctzll(b);
            auto *kVal = analysis::makeConstBefore(bb, I, g_, k);
            auto *res = g_.createValue();
            bb->insertBefore(I, g_.createShl(res, x, kVal));
            replaceWithValue(bb, I, res);
            return true;
        }
        // (2^k) * y -> shl(y, k)
        if (analysis::getConstU64(x, a) && a && (a & (a - 1)) == 0) {
            unsigned k = (unsigned)__builtin_ctzll(a);
            auto *kVal = analysis::makeConstBefore(bb, I, g_, k);
            auto *res = g_.createValue();
            bb->insertBefore(I, g_.createShl(res, y, kVal));
            replaceWithValue(bb, I, res);
            return true;
        }

        return false;
    }

    bool visitAnd(ir::BasicBlock *bb, ir::Inst *I) {
        using ir::Opcode;
        if (I->opcode() != Opcode::AND_U64)
            return false;

        auto ops = I->operands();
        auto *x = std::get<ir::SSAValue *>(ops[0]);
        auto *y = std::get<ir::SSAValue *>(ops[1]);

        uint64_t a, b;

        // x & 0 -> 0 ; 0 & x -> 0
        if (analysis::getConstU64(x, a) && a == 0) {
            replaceWithConst(g_, bb, I, 0);
            return true;
        }
        if (analysis::getConstU64(y, b) && b == 0) {
            replaceWithConst(g_, bb, I, 0);
            return true;
        }

        // x & ~0 -> x ; ~0 & x -> x
        if (analysis::getConstU64(y, b) && b == analysis::allOnes()) {
            replaceWithValue(bb, I, x);
            return true;
        }
        if (analysis::getConstU64(x, a) && a == analysis::allOnes()) {
            replaceWithValue(bb, I, y);
            return true;
        }

        // x & x -> x
        if (x == y) {
            replaceWithValue(bb, I, x);
            return true;
        }

        return false;
    }

    bool visitShl(ir::BasicBlock *bb, ir::Inst *I) {
        using ir::Opcode;
        if (I->opcode() != Opcode::SHL_U64)
            return false;

        auto ops = I->operands();
        auto *x = std::get<ir::SSAValue *>(ops[0]);
        auto *s = std::get<ir::SSAValue *>(ops[1]);

        uint64_t vx, k;

        // x << 0 -> x
        if (analysis::getConstU64(s, k) && k == 0) {
            replaceWithValue(bb, I, x);
            return true;
        }

        // 0 << y -> 0
        if (analysis::getConstU64(x, vx) && vx == 0) {
            replaceWithConst(g_, bb, I, 0);
            return true;
        }

        // (x << c1) << c2 -> x << (c1 + c2)
        if (x && x->def && x->def->opcode() == Opcode::SHL_U64) {
            auto *inner = static_cast<ir::ShlInst *>(x->def);
            auto innerOps = inner->operands();
            auto *innerX = std::get<ir::SSAValue *>(innerOps[0]);
            auto *innerS = std::get<ir::SSAValue *>(innerOps[1]);
            uint64_t c1, c2;
            if (analysis::getConstU64(innerS, c1) && analysis::getConstU64(s, c2)) {
                auto *sum = analysis::makeConstBefore(bb, I, g_, c1 + c2);
                auto *res = g_.createValue();
                bb->insertBefore(I, g_.createShl(res, innerX, sum));
                replaceWithValue(bb, I, res);

                if (inner->result() && inner->result()->users.empty()) {
                    bb->erase(inner);
                }
                return true;
            }
        }
        return false;
    }
};

} // namespace opt