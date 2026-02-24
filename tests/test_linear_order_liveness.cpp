#include "analysis/dominator_tree.h"
#include "analysis/linear_order.h"
#include "analysis/liveness.h"
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

static void assertLoopContiguous(const analysis::LinearOrder &ord, const analysis::LoopAnalyzer &loops) {
    for (auto &up : loops.loops) {
        auto *L = up.get();
        assert(L && !L->irreducible);
        assert(L->header);

        int mn = ord.index.at(L->header);
        int mx = mn;
        for (auto *b : L->blocks) {
            int idx = ord.index.at(b);
            mn = std::min(mn, idx);
            mx = std::max(mx, idx);
        }
        assert(mn == ord.index.at(L->header));
        assert((mx - mn + 1) == (int)L->blocks.size());
    }
}

static void assertDominatorsBefore(const analysis::LinearOrder &ord, ir::BasicBlock *entry) {
    analysis::DominatorTree DT;
    DT.build(entry);
    for (auto *b : ord.blocks) {
        if (b == entry) {
            continue;
        }
        assert(DT.idom_map.count(b));
        auto *idom = DT.idom_map[b];
        assert(idom);
        assert(ord.index.at(idom) < ord.index.at(b));
    }
}

struct Graph1 {
    ir::IRGraph g;
    ir::BasicBlock *entry;
    ir::BasicBlock *header;
    ir::BasicBlock *body;
    ir::BasicBlock *exit;
    ir::Inst *inst_movi_res;
    ir::Inst *inst_movi_i;
    ir::Inst *inst_movi_limit;
    ir::Inst *inst_cmp;
    ir::Inst *inst_mul;
};

static Graph1 buildGraph1() {
    Graph1 G;
    G.entry = G.g.createBlock("entry");
    G.header = G.g.createBlock("h");
    G.body = G.g.createBlock("b");
    G.exit = G.g.createBlock("exit");

    G.entry->addSuccessor(G.header);
    G.header->addSuccessor(G.exit);
    G.header->addSuccessor(G.body);
    G.body->addSuccessor(G.header);

    auto *res0 = G.g.createValue("res0");
    auto *i0 = G.g.createValue("i0");
    auto *limit = G.g.createValue("limit");

    G.inst_movi_res = append(G.entry, G.g.createMovi(res0, 1));
    G.inst_movi_i = append(G.entry, G.g.createMovi(i0, 1));
    G.inst_movi_limit = append(G.entry, G.g.createMovi(limit, 10));
    G.entry->addInst(G.g.createJmp(G.header));

    auto *res1 = G.g.createValue("res1");
    auto *i1 = G.g.createValue("i1");
    auto *res2 = G.g.createValue("res2");
    auto *i2 = G.g.createValue("i2");

    G.header->addInst(G.g.createPhi(res1, {{G.entry, res0}, {G.body, res2}}));
    G.header->addInst(G.g.createPhi(i1, {{G.entry, i0}, {G.body, i2}}));
    G.inst_cmp = append(G.header, G.g.createCmp(i1, limit));
    G.header->addInst(G.g.createJa(G.exit));
    G.header->addInst(G.g.createJmp(G.body));

    G.inst_mul = append(G.body, G.g.createMul(res2, res1, i1));
    G.body->addInst(G.g.createAddi(i2, i1, 1));
    G.body->addInst(G.g.createJmp(G.header));

    G.exit->addInst(G.g.createRet(res1));

    assert(G.g.checkDataFlow());
    return G;
}

struct Graph2 {
    ir::IRGraph g;
    ir::BasicBlock *entry;
    ir::BasicBlock *header;
    ir::BasicBlock *ifblk;
    ir::BasicBlock *thenb;
    ir::BasicBlock *elseb;
    ir::BasicBlock *latch;
    ir::BasicBlock *exit;
    ir::Inst *inst_cmp_loop;
    ir::Inst *inst_then_add;
    ir::Inst *inst_else_add;
};

static Graph2 buildGraph2() {
    Graph2 G;
    G.entry = G.g.createBlock("entry");
    G.header = G.g.createBlock("h");
    G.ifblk = G.g.createBlock("if");
    G.thenb = G.g.createBlock("then");
    G.elseb = G.g.createBlock("else");
    G.latch = G.g.createBlock("latch");
    G.exit = G.g.createBlock("exit");

    G.entry->addSuccessor(G.header);
    G.header->addSuccessor(G.exit);
    G.header->addSuccessor(G.ifblk);
    G.ifblk->addSuccessor(G.thenb);
    G.ifblk->addSuccessor(G.elseb);
    G.thenb->addSuccessor(G.latch);
    G.elseb->addSuccessor(G.latch);
    G.latch->addSuccessor(G.header);

    auto *N = G.g.createValue("N");
    auto *i0 = G.g.createValue("i0");
    auto *sum0 = G.g.createValue("sum0");

    G.entry->addInst(G.g.createMovi(N, 7));
    G.entry->addInst(G.g.createMovi(i0, 0));
    G.entry->addInst(G.g.createMovi(sum0, 0));
    G.entry->addInst(G.g.createJmp(G.header));

    auto *i = G.g.createValue("i");
    auto *sum = G.g.createValue("sum");
    auto *i1 = G.g.createValue("i1");
    auto *sum1 = G.g.createValue("sum1");
    auto *sum_then = G.g.createValue("sum_then");
    auto *sum_else = G.g.createValue("sum_else");

    G.header->addInst(G.g.createPhi(i, {{G.entry, i0}, {G.latch, i1}}));
    G.header->addInst(G.g.createPhi(sum, {{G.entry, sum0}, {G.latch, sum1}}));
    G.inst_cmp_loop = append(G.header, G.g.createCmp(i, N));
    G.header->addInst(G.g.createJa(G.exit));
    G.header->addInst(G.g.createJmp(G.ifblk));

    auto *cond = G.g.createValue("cond");
    G.ifblk->addInst(G.g.createCmp(sum, N));
    G.ifblk->addInst(G.g.createJa(G.thenb));
    G.ifblk->addInst(G.g.createJmp(G.elseb));
    (void)cond;

    G.inst_then_add = append(G.thenb, G.g.createAddi(sum_then, sum, 1));
    G.thenb->addInst(G.g.createJmp(G.latch));

    G.inst_else_add = append(G.elseb, G.g.createAddi(sum_else, sum, 2));
    G.elseb->addInst(G.g.createJmp(G.latch));

    G.latch->addInst(G.g.createPhi(sum1, {{G.thenb, sum_then}, {G.elseb, sum_else}}));
    G.latch->addInst(G.g.createAddi(i1, i, 1));
    G.latch->addInst(G.g.createJmp(G.header));

    G.exit->addInst(G.g.createRet(sum));

    assert(G.g.checkDataFlow());
    return G;
}

struct Graph3 {
    ir::IRGraph g;
    ir::BasicBlock *entry;
    ir::BasicBlock *outerH;
    ir::BasicBlock *innerH;
    ir::BasicBlock *innerThen;
    ir::BasicBlock *innerElse;
    ir::BasicBlock *innerLatch;
    ir::BasicBlock *innerExit;
    ir::BasicBlock *outerLatch;
    ir::BasicBlock *exit;
    ir::Inst *inst_outer_cmp;
    ir::Inst *inst_inner_cmp;
};

static Graph3 buildGraph3() {
    Graph3 G;
    G.entry = G.g.createBlock("entry");
    G.outerH = G.g.createBlock("outerH");
    G.innerH = G.g.createBlock("innerH");
    G.innerThen = G.g.createBlock("innerThen");
    G.innerElse = G.g.createBlock("innerElse");
    G.innerLatch = G.g.createBlock("innerLatch");
    G.innerExit = G.g.createBlock("innerExit");
    G.outerLatch = G.g.createBlock("outerLatch");
    G.exit = G.g.createBlock("exit");

    G.entry->addSuccessor(G.outerH);
    G.outerH->addSuccessor(G.exit);
    G.outerH->addSuccessor(G.innerH);
    G.innerH->addSuccessor(G.innerExit);
    G.innerH->addSuccessor(G.innerThen);
    G.innerH->addSuccessor(G.innerElse);
    G.innerThen->addSuccessor(G.innerLatch);
    G.innerElse->addSuccessor(G.innerLatch);
    G.innerLatch->addSuccessor(G.innerH);
    G.innerExit->addSuccessor(G.outerLatch);
    G.outerLatch->addSuccessor(G.outerH);

    auto *N = G.g.createValue("N");
    auto *M = G.g.createValue("M");
    auto *i0 = G.g.createValue("i0");
    auto *acc0 = G.g.createValue("acc0");

    G.entry->addInst(G.g.createMovi(N, 3));
    G.entry->addInst(G.g.createMovi(M, 2));
    G.entry->addInst(G.g.createMovi(i0, 0));
    G.entry->addInst(G.g.createMovi(acc0, 0));
    G.entry->addInst(G.g.createJmp(G.outerH));

    auto *i = G.g.createValue("i");
    auto *acc = G.g.createValue("acc");
    auto *i1 = G.g.createValue("i1");
    auto *acc2 = G.g.createValue("acc2");
    auto *j0 = G.g.createValue("j0");

    G.outerH->addInst(G.g.createPhi(i, {{G.entry, i0}, {G.outerLatch, i1}}));
    G.outerH->addInst(G.g.createPhi(acc, {{G.entry, acc0}, {G.outerLatch, acc2}}));
    G.inst_outer_cmp = append(G.outerH, G.g.createCmp(i, N));
    G.outerH->addInst(G.g.createJa(G.exit));
    G.outerH->addInst(G.g.createMovi(j0, 0));
    G.outerH->addInst(G.g.createJmp(G.innerH));

    auto *j = G.g.createValue("j");
    auto *j1 = G.g.createValue("j1");
    auto *acc1 = G.g.createValue("acc1");
    auto *acc1n = G.g.createValue("acc1n");
    auto *acc_then = G.g.createValue("acc_then");
    auto *acc_else = G.g.createValue("acc_else");

    G.innerH->addInst(G.g.createPhi(j, {{G.outerH, j0}, {G.innerLatch, j1}}));
    G.innerH->addInst(G.g.createPhi(acc1, {{G.outerH, acc}, {G.innerLatch, acc1n}}));
    G.inst_inner_cmp = append(G.innerH, G.g.createCmp(j, M));
    G.innerH->addInst(G.g.createJa(G.innerExit));
    G.innerH->addInst(G.g.createCmp(j, i));
    G.innerH->addInst(G.g.createJa(G.innerThen));
    G.innerH->addInst(G.g.createJmp(G.innerElse));

    G.innerThen->addInst(G.g.createAddi(acc_then, acc1, 10));
    G.innerThen->addInst(G.g.createJmp(G.innerLatch));

    G.innerElse->addInst(G.g.createAddi(acc_else, acc1, 1));
    G.innerElse->addInst(G.g.createJmp(G.innerLatch));

    G.innerLatch->addInst(G.g.createPhi(acc1n, {{G.innerThen, acc_then}, {G.innerElse, acc_else}}));
    G.innerLatch->addInst(G.g.createAddi(j1, j, 1));
    G.innerLatch->addInst(G.g.createJmp(G.innerH));

    G.innerExit->addInst(G.g.createCast(acc2, acc1));
    G.innerExit->addInst(G.g.createJmp(G.outerLatch));

    G.outerLatch->addInst(G.g.createAddi(i1, i, 1));
    G.outerLatch->addInst(G.g.createJmp(G.outerH));

    G.exit->addInst(G.g.createRet(acc));

    assert(G.g.checkDataFlow());
    return G;
}

} // namespace

void testLinearOrderAndLiveness() {
    {
        auto G = buildGraph1();
        analysis::LinearOrder ord;
        ord.run(G.entry);
        assertDominatorsBefore(ord, G.entry);
        assertLoopContiguous(ord, ord.loops);

        analysis::LivenessAnalysis LA;
        LA.run(G.entry);

        auto bpEntry = LA.blockPosition(G.entry);
        auto bpBody = LA.blockPosition(G.body);
        auto bpHeader = LA.blockPosition(G.header);
        auto loopEnd = LA.linearOrder().loopEnd.at(G.header);
        auto bpLoopEnd = LA.blockPosition(loopEnd);

        auto *limit = G.inst_movi_limit->result();
        auto *liLimit = LA.getInterval(limit);
        assert(liLimit);
        assert(liLimit->covers(bpHeader.from));
        assert(liLimit->covers(bpLoopEnd.to - 1));

        auto *res0 = G.inst_movi_res->result();
        auto *liRes0 = LA.getInterval(res0);
        assert(liRes0);
        assert(liRes0->end() == bpEntry.to);

        auto *res2 = G.inst_mul->result();
        auto *liRes2 = LA.getInterval(res2);
        assert(liRes2);
        assert(liRes2->end() == bpBody.to);

        auto liveAtCmp = LA.liveValuesAt(G.inst_cmp);
        assert(std::find(liveAtCmp.begin(), liveAtCmp.end(), limit) != liveAtCmp.end());
        auto *i1 = std::get<ir::SSAValue *>(G.inst_cmp->operands()[0]);
        assert(std::find(liveAtCmp.begin(), liveAtCmp.end(), i1) != liveAtCmp.end());

        (void)bpHeader;
        (void)bpLoopEnd;
    }

    {
        auto G = buildGraph2();
        analysis::LinearOrder ord;
        ord.run(G.entry);
        assertDominatorsBefore(ord, G.entry);
        assertLoopContiguous(ord, ord.loops);

        analysis::LivenessAnalysis LA;
        LA.run(G.entry);

        auto bpThen = LA.blockPosition(G.thenb);
        auto bpElse = LA.blockPosition(G.elseb);
        auto *liThen = LA.getInterval(G.inst_then_add);
        auto *liElse = LA.getInterval(G.inst_else_add);
        assert(liThen && liElse);
        assert(liThen->end() == bpThen.to);
        assert(liElse->end() == bpElse.to);

        auto liveAtLoopCmp = LA.liveValuesAt(G.inst_cmp_loop);
        auto *i = std::get<ir::SSAValue *>(G.inst_cmp_loop->operands()[0]);
        auto *N = std::get<ir::SSAValue *>(G.inst_cmp_loop->operands()[1]);
        assert(std::find(liveAtLoopCmp.begin(), liveAtLoopCmp.end(), i) != liveAtLoopCmp.end());
        assert(std::find(liveAtLoopCmp.begin(), liveAtLoopCmp.end(), N) != liveAtLoopCmp.end());
    }

    {
        auto G = buildGraph3();
        analysis::LinearOrder ord;
        ord.run(G.entry);
        assertDominatorsBefore(ord, G.entry);
        assertLoopContiguous(ord, ord.loops);

        analysis::LivenessAnalysis LA;
        LA.run(G.entry);

        auto liveAtOuter = LA.liveValuesAt(G.inst_outer_cmp);
        auto *i = std::get<ir::SSAValue *>(G.inst_outer_cmp->operands()[0]);
        auto *N = std::get<ir::SSAValue *>(G.inst_outer_cmp->operands()[1]);
        assert(std::find(liveAtOuter.begin(), liveAtOuter.end(), i) != liveAtOuter.end());
        assert(std::find(liveAtOuter.begin(), liveAtOuter.end(), N) != liveAtOuter.end());

        auto liveAtInner = LA.liveValuesAt(G.inst_inner_cmp);
        auto *j = std::get<ir::SSAValue *>(G.inst_inner_cmp->operands()[0]);
        auto *M = std::get<ir::SSAValue *>(G.inst_inner_cmp->operands()[1]);
        assert(std::find(liveAtInner.begin(), liveAtInner.end(), j) != liveAtInner.end());
        assert(std::find(liveAtInner.begin(), liveAtInner.end(), M) != liveAtInner.end());
    }
}
