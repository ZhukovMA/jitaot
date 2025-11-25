#pragma once
#include "ir/basic_block.h"
#include "ir/inst.h"
#include "ir/ir_graph.h"
#include <algorithm>
#include <cstdint>
#include <variant>

namespace analysis {

inline bool getConstU64(ir::SSAValue *v, uint64_t &out) {
    if (!v || !v->def)
        return false;
    if (v->def->opcode() != ir::Opcode::MOVI_U64)
        return false;
    if (auto *mi = dynamic_cast<ir::MoviInst *>(v->def)) {
        out = mi->imm();
        return true;
    }
    return false;
}

inline void replaceAllUsesWith(ir::SSAValue *from, ir::SSAValue *to) {
    if (!from || !to || from == to)
        return;
    auto users = from->users;
    for (auto *I : users)
        I->replaceOperand(from, to);
    from->users.clear();
}

inline ir::SSAValue *makeConstBefore(ir::BasicBlock *bb, ir::Inst *anchor, ir::IRGraph &g, uint64_t cst) {
    auto *v = g.createValue();
    bb->insertBefore(anchor, g.createMovi(v, cst));
    return v;
}

inline void eraseInst(ir::BasicBlock *bb, ir::Inst *I) {

    for (auto &op : I->operands()) {
        if (std::holds_alternative<ir::SSAValue *>(op)) {
            auto *v = std::get<ir::SSAValue *>(op);
            auto &u = v->users;
            u.erase(std::remove(u.begin(), u.end(), I), u.end());
        }
    }
    if (auto *r = I->result())
        r->def = nullptr;
    bb->erase(I);
}

inline uint64_t allOnes() {
    return ~0ULL;
}

} // namespace analysis