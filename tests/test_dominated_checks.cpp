#include "analysis/dominated_checks.h"
#include "test_functions.h"
#include "test_ir_helpers.h"
#include <cassert>

using namespace ir;
using namespace analysis;
using namespace testutil;

namespace {

void testNullCheckSameBlock() {
    IRGraph g;
    auto *entry = g.createBlock("entry");
    auto *obj = g.createArg("ref", "obj");
    auto *retv = g.createValue("retv");

    entry->addInst(g.createNullCheck(obj));
    entry->addInst(g.createNullCheck(obj));
    entry->addInst(g.createMovi(retv, 0));
    entry->addInst(g.createRet(retv));

    DominatedChecksElimination pass;
    bool changed = pass.run(g, entry);

    assert(changed);
    assert(countOp(entry, Opcode::NULL_CHECK) == 1);
    assert(g.checkDataFlow());
}

void testNullCheckDominatedBlock() {
    IRGraph g;
    auto *entry = g.createBlock("entry");
    auto *mid = g.createBlock("mid");
    auto *exit = g.createBlock("exit");
    auto *obj = g.createArg("ref", "obj");
    auto *retv = g.createValue("retv");

    entry->addInst(g.createNullCheck(obj));
    entry->addSuccessor(mid);
    mid->addInst(g.createNullCheck(obj));
    mid->addSuccessor(exit);
    exit->addInst(g.createMovi(retv, 1));
    exit->addInst(g.createRet(retv));

    DominatedChecksElimination pass;
    bool changed = pass.run(g, entry);

    assert(changed);
    assert(countOp(entry, Opcode::NULL_CHECK) == 1);
    assert(countOp(mid, Opcode::NULL_CHECK) == 0);
    assert(g.checkDataFlow());
}

void testNullCheckNotDominatedAtMerge() {
    IRGraph g;
    auto *entry = g.createBlock("entry");
    auto *left = g.createBlock("left");
    auto *right = g.createBlock("right");
    auto *merge = g.createBlock("merge");
    auto *obj = g.createArg("ref", "obj");
    auto *retv = g.createValue("retv");

    entry->addSuccessor(left);
    entry->addSuccessor(right);
    left->addInst(g.createNullCheck(obj));
    left->addSuccessor(merge);
    right->addSuccessor(merge);
    merge->addInst(g.createNullCheck(obj));
    merge->addInst(g.createMovi(retv, 2));
    merge->addInst(g.createRet(retv));

    DominatedChecksElimination pass;
    bool changed = pass.run(g, entry);

    assert(!changed);
    assert(countOp(left, Opcode::NULL_CHECK) == 1);
    assert(countOp(merge, Opcode::NULL_CHECK) == 1);
    assert(g.checkDataFlow());
}

void testBoundsCheckExactDuplicate() {
    IRGraph g;
    auto *entry = g.createBlock("entry");
    auto *idx = g.createArg("u64", "idx");
    auto *len = g.createArg("u64", "len");
    auto *retv = g.createValue("retv");

    entry->addInst(g.createBoundsCheck(idx, len));
    entry->addInst(g.createBoundsCheck(idx, len));
    entry->addInst(g.createMovi(retv, 0));
    entry->addInst(g.createRet(retv));

    DominatedChecksElimination pass;
    bool changed = pass.run(g, entry);

    assert(changed);
    assert(countOp(entry, Opcode::BOUNDS_CHECK) == 1);
    assert(g.checkDataFlow());
}

void testBoundsCheckDifferentLengthStays() {
    IRGraph g;
    auto *entry = g.createBlock("entry");
    auto *idx = g.createArg("u64", "idx");
    auto *len0 = g.createArg("u64", "len0");
    auto *len1 = g.createArg("u64", "len1");
    auto *retv = g.createValue("retv");

    entry->addInst(g.createBoundsCheck(idx, len0));
    entry->addInst(g.createBoundsCheck(idx, len1));
    entry->addInst(g.createMovi(retv, 0));
    entry->addInst(g.createRet(retv));

    DominatedChecksElimination pass;
    bool changed = pass.run(g, entry);

    assert(!changed);
    assert(countOp(entry, Opcode::BOUNDS_CHECK) == 2);
    assert(g.checkDataFlow());
}

} // namespace

void testDominatedNullChecks() {
    testNullCheckSameBlock();
    testNullCheckDominatedBlock();
    testNullCheckNotDominatedAtMerge();
}

void testDominatedBoundsChecks() {
    testBoundsCheckExactDuplicate();
    testBoundsCheckDifferentLengthStays();
}
