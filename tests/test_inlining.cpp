#include "analysis/inlining.h"
#include "ir/ir_graph.h"
#include "test_functions.h"
#include <cassert>
#include <iostream>

using namespace ir;

static size_t countOpcode(const IRGraph &g, Opcode op) {
    size_t n = 0;
    for (auto *bb : g.allBlocks()) {
        for (auto *I : bb->allInsts()) {
            if (I->opcode() == op) {
                ++n;
            }
        }
    }

    return n;
}

void testInlining() {
    IRGraph caller;
    caller.setSignature("u64", "caller", {{"u64", "x"}, {"u64", "y"}});

    auto *caller_entry = caller.createBlock("entry");
    auto *x = caller.createArg("u64", "x");
    auto *y = caller.createArg("u64", "y");
    auto *call_res = caller.createValue("call_res");
    auto *after = caller.createValue("after");

    caller_entry->addInst(caller.createCallStatic(call_res, {x, y}, "callee"));
    caller_entry->addInst(caller.createAddi(after, call_res, 1));
    caller_entry->addInst(caller.createRet(after));

    IRGraph callee;
    callee.setSignature("u64", "callee", {{"u64", "a0"}, {"u64", "a1"}});
    auto *c_entry = callee.createBlock("c.entry");
    auto *c_r1 = callee.createBlock("c.r1");
    auto *c_r2 = callee.createBlock("c.r2");
    auto *c_r3 = callee.createBlock("c.r3");

    auto *p0 = callee.createValue("p0");
    auto *p1 = callee.createValue("p1");
    auto *k1 = callee.createValue("k1");
    auto *k2 = callee.createValue("k2");
    auto *k3 = callee.createValue("k3");
    auto *r1 = callee.createValue("r1");
    auto *r2 = callee.createValue("r2");
    auto *r3 = callee.createValue("r3");

    c_entry->addInst(callee.createParameter(p0, 0));
    c_entry->addInst(callee.createParameter(p1, 1));
    c_entry->addInst(callee.createMovi(k1, 10));
    c_entry->addInst(callee.createMovi(k2, 20));
    c_entry->addInst(callee.createMovi(k3, 30));
    c_entry->addSuccessor(c_r1);
    c_entry->addSuccessor(c_r2);
    c_entry->addSuccessor(c_r3);

    c_r1->addInst(callee.createAddi(r1, p0, 10));
    c_r1->addInst(callee.createRet(r1));

    c_r2->addInst(callee.createMul(r2, p1, k2));
    c_r2->addInst(callee.createRet(r2));

    c_r3->addInst(callee.createMul(r3, p0, k3));
    c_r3->addInst(callee.createRet(r3));

    auto *call_inst = dynamic_cast<CallStaticInst *>(caller_entry->allInsts().front());
    assert(call_inst != nullptr);

    analysis::Inliner inliner;
    bool ok = inliner.inlineStaticCall(caller, caller_entry, call_inst, callee, c_entry);
    assert(ok);

    assert(countOpcode(caller, Opcode::CALL_STATIC_U64) == 0);
    assert(countOpcode(caller, Opcode::PHI_U64) == 1);
    assert(countOpcode(caller, Opcode::MOVI_U64) >= 3);
    assert(caller_entry->successors.size() == 3);
    assert(caller.checkDataFlow());

    std::cout << "Inlining test OK.\n";
}
