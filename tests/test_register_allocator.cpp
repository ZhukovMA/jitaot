#include "analysis/register_allocator.h"
#include "ir/ir_graph.h"
#include "test_functions.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <unordered_set>

namespace {

static ir::Inst *append(ir::BasicBlock *bb, std::unique_ptr<ir::Inst> inst) {
    auto *ptr = inst.get();
    bb->addInst(std::move(inst));
    return ptr;
}

static int countOpcode(const std::vector<ir::BasicBlock *> &blocks, ir::Opcode op) {
    int n = 0;
    for (auto *bb : blocks) {
        for (auto *I : bb->allInsts()) {
            if (I->opcode() == op) {
                ++n;
            }
        }
    }
    return n;
}

static void assertAllIntervalsAllocated(const analysis::RegisterAllocator &ra) {
    for (auto &kv : ra.liveness().intervals()) {
        auto loc = ra.locationOf(kv.first);
        assert(loc.kind != analysis::AllocPos::Kind::NONE);
        if (loc.kind == analysis::AllocPos::Kind::REGISTER) {
            assert(loc.index >= 0);
        }
        if (loc.kind == analysis::AllocPos::Kind::STACK) {
            assert(loc.index >= 0);
        }
    }
}

static void assertRegisterPressureLimited(const analysis::RegisterAllocator &ra, int regs) {
    for (auto *bb : ra.liveness().linearOrder().blocks) {
        for (auto *I : bb->allInsts()) {
            int pos = ra.liveness().positionOf(I);
            if (pos < 0) {
                continue;
            }
            int used = 0;
            for (auto &kv : ra.liveness().intervals()) {
                if (!kv.second.covers(pos)) {
                    continue;
                }
                auto loc = ra.locationOf(kv.first);
                if (loc.kind == analysis::AllocPos::Kind::REGISTER && loc.reg_class == ir::RegClass::INT) {
                    ++used;
                }
            }
            assert(used <= regs);
        }
    }
}

struct GraphA {
    ir::IRGraph g;
    ir::BasicBlock *entry{};
    ir::BasicBlock *head{};
    ir::BasicBlock *body{};
    ir::BasicBlock *exit{};
};

static GraphA buildGraphA() {
    GraphA W;
    W.entry = W.g.createBlock("entry");
    W.head = W.g.createBlock("head");
    W.body = W.g.createBlock("body");
    W.exit = W.g.createBlock("exit");

    W.entry->addSuccessor(W.head);
    W.head->addSuccessor(W.exit);
    W.head->addSuccessor(W.body);
    W.body->addSuccessor(W.head);

    auto *acc0 = W.g.createValue("acc0");
    auto *i0 = W.g.createValue("i0");
    auto *limit = W.g.createValue("limit");
    W.entry->addInst(W.g.createMovi(acc0, 1));
    W.entry->addInst(W.g.createMovi(i0, 1));
    W.entry->addInst(W.g.createMovi(limit, 8));
    W.entry->addInst(W.g.createJmp(W.head));

    auto *acc1 = W.g.createValue("acc1");
    auto *i1 = W.g.createValue("i1");
    auto *acc2 = W.g.createValue("acc2");
    auto *i2 = W.g.createValue("i2");

    W.head->addInst(W.g.createPhi(acc1, {{W.entry, acc0}, {W.body, acc2}}));
    W.head->addInst(W.g.createPhi(i1, {{W.entry, i0}, {W.body, i2}}));
    W.head->addInst(W.g.createCmp(i1, limit));
    W.head->addInst(W.g.createJa(W.exit));
    W.head->addInst(W.g.createJmp(W.body));

    W.body->addInst(W.g.createMul(acc2, acc1, i1));
    W.body->addInst(W.g.createAddi(i2, i1, 1));
    W.body->addInst(W.g.createJmp(W.head));

    W.exit->addInst(W.g.createRet(acc1));
    assert(W.g.checkDataFlow());
    return W;
}

struct GraphB {
    ir::IRGraph g;
    ir::BasicBlock *entry{};
    ir::BasicBlock *head{};
    ir::BasicBlock *ifb{};
    ir::BasicBlock *thenb{};
    ir::BasicBlock *elseb{};
    ir::BasicBlock *latch{};
    ir::BasicBlock *exit{};
};

static GraphB buildGraphB() {
    GraphB W;
    W.entry = W.g.createBlock("entry");
    W.head = W.g.createBlock("head");
    W.ifb = W.g.createBlock("if");
    W.thenb = W.g.createBlock("then");
    W.elseb = W.g.createBlock("else");
    W.latch = W.g.createBlock("latch");
    W.exit = W.g.createBlock("exit");

    W.entry->addSuccessor(W.head);
    W.head->addSuccessor(W.exit);
    W.head->addSuccessor(W.ifb);
    W.ifb->addSuccessor(W.thenb);
    W.ifb->addSuccessor(W.elseb);
    W.thenb->addSuccessor(W.latch);
    W.elseb->addSuccessor(W.latch);
    W.latch->addSuccessor(W.head);

    auto *n = W.g.createValue("n");
    auto *i0 = W.g.createValue("i0");
    auto *sum0 = W.g.createValue("sum0");
    W.entry->addInst(W.g.createMovi(n, 6));
    W.entry->addInst(W.g.createMovi(i0, 0));
    W.entry->addInst(W.g.createMovi(sum0, 0));
    W.entry->addInst(W.g.createJmp(W.head));

    auto *i = W.g.createValue("i");
    auto *sum = W.g.createValue("sum");
    auto *i1 = W.g.createValue("i1");
    auto *sum1 = W.g.createValue("sum1");
    auto *sum_then = W.g.createValue("sum_then");
    auto *sum_else = W.g.createValue("sum_else");

    W.head->addInst(W.g.createPhi(i, {{W.entry, i0}, {W.latch, i1}}));
    W.head->addInst(W.g.createPhi(sum, {{W.entry, sum0}, {W.latch, sum1}}));
    W.head->addInst(W.g.createCmp(i, n));
    W.head->addInst(W.g.createJa(W.exit));
    W.head->addInst(W.g.createJmp(W.ifb));

    W.ifb->addInst(W.g.createCmp(sum, n));
    W.ifb->addInst(W.g.createJa(W.thenb));
    W.ifb->addInst(W.g.createJmp(W.elseb));

    W.thenb->addInst(W.g.createAddi(sum_then, sum, 1));
    W.thenb->addInst(W.g.createJmp(W.latch));

    W.elseb->addInst(W.g.createAddi(sum_else, sum, 2));
    W.elseb->addInst(W.g.createJmp(W.latch));

    W.latch->addInst(W.g.createPhi(sum1, {{W.thenb, sum_then}, {W.elseb, sum_else}}));
    W.latch->addInst(W.g.createAddi(i1, i, 1));
    W.latch->addInst(W.g.createJmp(W.head));

    W.exit->addInst(W.g.createRet(sum));
    assert(W.g.checkDataFlow());
    return W;
}

struct GraphC {
    ir::IRGraph g;
    ir::BasicBlock *entry{};
    ir::BasicBlock *outerH{};
    ir::BasicBlock *innerH{};
    ir::BasicBlock *innerThen{};
    ir::BasicBlock *innerElse{};
    ir::BasicBlock *innerLatch{};
    ir::BasicBlock *innerExit{};
    ir::BasicBlock *outerLatch{};
    ir::BasicBlock *exit{};
};

static GraphC buildGraphC() {
    GraphC W;
    W.entry = W.g.createBlock("entry");
    W.outerH = W.g.createBlock("outerH");
    W.innerH = W.g.createBlock("innerH");
    W.innerThen = W.g.createBlock("innerThen");
    W.innerElse = W.g.createBlock("innerElse");
    W.innerLatch = W.g.createBlock("innerLatch");
    W.innerExit = W.g.createBlock("innerExit");
    W.outerLatch = W.g.createBlock("outerLatch");
    W.exit = W.g.createBlock("exit");

    W.entry->addSuccessor(W.outerH);
    W.outerH->addSuccessor(W.exit);
    W.outerH->addSuccessor(W.innerH);
    W.innerH->addSuccessor(W.innerExit);
    W.innerH->addSuccessor(W.innerThen);
    W.innerH->addSuccessor(W.innerElse);
    W.innerThen->addSuccessor(W.innerLatch);
    W.innerElse->addSuccessor(W.innerLatch);
    W.innerLatch->addSuccessor(W.innerH);
    W.innerExit->addSuccessor(W.outerLatch);
    W.outerLatch->addSuccessor(W.outerH);

    auto *n = W.g.createValue("n");
    auto *m = W.g.createValue("m");
    auto *i0 = W.g.createValue("i0");
    auto *acc0 = W.g.createValue("acc0");
    W.entry->addInst(W.g.createMovi(n, 3));
    W.entry->addInst(W.g.createMovi(m, 2));
    W.entry->addInst(W.g.createMovi(i0, 0));
    W.entry->addInst(W.g.createMovi(acc0, 0));
    W.entry->addInst(W.g.createJmp(W.outerH));

    auto *i = W.g.createValue("i");
    auto *acc = W.g.createValue("acc");
    auto *i1 = W.g.createValue("i1");
    auto *acc2 = W.g.createValue("acc2");
    auto *j0 = W.g.createValue("j0");

    W.outerH->addInst(W.g.createPhi(i, {{W.entry, i0}, {W.outerLatch, i1}}));
    W.outerH->addInst(W.g.createPhi(acc, {{W.entry, acc0}, {W.outerLatch, acc2}}));
    W.outerH->addInst(W.g.createCmp(i, n));
    W.outerH->addInst(W.g.createJa(W.exit));
    W.outerH->addInst(W.g.createMovi(j0, 0));
    W.outerH->addInst(W.g.createJmp(W.innerH));

    auto *j = W.g.createValue("j");
    auto *j1 = W.g.createValue("j1");
    auto *acc1 = W.g.createValue("acc1");
    auto *acc1n = W.g.createValue("acc1n");
    auto *acc_then = W.g.createValue("acc_then");
    auto *acc_else = W.g.createValue("acc_else");

    W.innerH->addInst(W.g.createPhi(j, {{W.outerH, j0}, {W.innerLatch, j1}}));
    W.innerH->addInst(W.g.createPhi(acc1, {{W.outerH, acc}, {W.innerLatch, acc1n}}));
    W.innerH->addInst(W.g.createCmp(j, m));
    W.innerH->addInst(W.g.createJa(W.innerExit));
    W.innerH->addInst(W.g.createCmp(j, i));
    W.innerH->addInst(W.g.createJa(W.innerThen));
    W.innerH->addInst(W.g.createJmp(W.innerElse));

    W.innerThen->addInst(W.g.createAddi(acc_then, acc1, 10));
    W.innerThen->addInst(W.g.createJmp(W.innerLatch));

    W.innerElse->addInst(W.g.createAddi(acc_else, acc1, 1));
    W.innerElse->addInst(W.g.createJmp(W.innerLatch));

    W.innerLatch->addInst(W.g.createPhi(acc1n, {{W.innerThen, acc_then}, {W.innerElse, acc_else}}));
    W.innerLatch->addInst(W.g.createAddi(j1, j, 1));
    W.innerLatch->addInst(W.g.createJmp(W.innerH));

    W.innerExit->addInst(W.g.createCast(acc2, acc1));
    W.innerExit->addInst(W.g.createJmp(W.outerLatch));

    W.outerLatch->addInst(W.g.createAddi(i1, i, 1));
    W.outerLatch->addInst(W.g.createJmp(W.outerH));

    W.exit->addInst(W.g.createRet(acc));
    assert(W.g.checkDataFlow());
    return W;
}

static void testAllocatorGraphA() {
    auto W = buildGraphA();
    analysis::RegisterAllocator ra(1, 0);
    ra.run(W.g, W.entry);

    assertAllIntervalsAllocated(ra);
    assertRegisterPressureLimited(ra, 1);
    assert(ra.usedStackSlots(ir::RegClass::INT) > 0);
    assert(ra.fillCount() > 0);
    assert(ra.spillCount() > 0);
    assert(ra.moveCount() > 0);
    assert(W.g.checkDataFlow());

    auto blocks = ra.liveness().linearOrder().blocks;
    assert(countOpcode(blocks, ir::Opcode::FILL_U64) == ra.fillCount());
    assert(countOpcode(blocks, ir::Opcode::SPILL_U64) == ra.spillCount());
    assert(countOpcode(blocks, ir::Opcode::MOVE_U64) == ra.moveCount());
}

static void testAllocatorGraphB() {
    auto W = buildGraphB();
    analysis::RegisterAllocator ra(2, 0);
    ra.run(W.g, W.entry);

    assertAllIntervalsAllocated(ra);
    assertRegisterPressureLimited(ra, 2);
    assert(ra.usedStackSlots(ir::RegClass::INT) > 0);
    assert(ra.fillCount() > 0);
    assert(ra.moveCount() > 0);
    assert(W.g.checkDataFlow());

    auto blocks = ra.liveness().linearOrder().blocks;
    assert(countOpcode(blocks, ir::Opcode::FILL_U64) > 0);
    assert(countOpcode(blocks, ir::Opcode::MOVE_U64) > 0);
}

static void testAllocatorGraphC() {
    auto W = buildGraphC();
    analysis::RegisterAllocator ra(2, 1);
    ra.run(W.g, W.entry);

    assertAllIntervalsAllocated(ra);
    assertRegisterPressureLimited(ra, 2);
    assert(ra.usedStackSlots(ir::RegClass::INT) > 0);
    assert(ra.fillCount() > 0);
    assert(ra.spillCount() > 0);
    assert(ra.moveCount() > 0);
    assert(W.g.checkDataFlow());

    int edge_moves = 0;
    for (auto &mv : ra.edgeMoves()) {
        if (mv.pred && mv.succ && mv.from != mv.to) {
            ++edge_moves;
        }
    }
    assert(edge_moves > 0);
}

} // namespace

void testRegisterAllocator() {
    testAllocatorGraphA();
    testAllocatorGraphB();
    testAllocatorGraphC();
}