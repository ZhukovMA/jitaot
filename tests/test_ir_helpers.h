#pragma once
#include "ir/ir_graph.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace testutil {
using namespace ir;

inline BasicBlock *oneBlock(IRGraph &g, const std::string &name = "B") {
    return g.createBlock(name);
}

inline size_t countOp(BasicBlock *bb, Opcode op) {
    size_t c = 0;
    for (auto *I : bb->allInsts())
        if (I->opcode() == op)
            ++c;
    return c;
}

inline uint64_t getRetImm(const BasicBlock *bb) {
    for (auto *I : const_cast<BasicBlock *>(bb)->allInsts()) {
        if (I->opcode() == Opcode::RET_U64) {
            auto ops = I->operands();
            auto *v = std::get<SSAValue *>(ops[0]);
            auto *def = v ? v->def : nullptr;
            auto *mi = def ? dynamic_cast<MoviInst *>(def) : nullptr;
            assert(mi && "ret must point to a movi after folding");
            return mi->imm();
        }
    }
    assert(false && "ret not found");
    return 0;
}

inline SSAValue *getRetValue(const BasicBlock *bb) {
    for (auto *I : const_cast<BasicBlock *>(bb)->allInsts()) {
        if (I->opcode() == Opcode::RET_U64) {
            return std::get<SSAValue *>(I->operands()[0]);
        }
    }
    return nullptr;
}
} // namespace testutil