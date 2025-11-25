#include "analysis/ir_utils.h"
#include "analysis/peephole.h"
#include "ir/ir_graph.h"
#include "test_functions.h"
#include "test_ir_helpers.h"

using namespace ir;
using namespace testutil;
using analysis::getConstU64;

static ir::ShlInst *findSingleShl(const BasicBlock *bb) {
    ShlInst *found = nullptr;
    for (auto *I : const_cast<BasicBlock *>(bb)->allInsts()) {
        if (I->opcode() == Opcode::SHL_U64) {
            assert(found == nullptr && "expected only one SHL");
            found = dynamic_cast<ShlInst *>(I);
        }
    }
    return found;
}

void testPH_MulIdentities() {
    IRGraph g;
    auto *b = oneBlock(g);

    auto *x = g.createValue("x");
    auto *one = g.createValue();
    b->addInst(g.createMovi(one, 1));
    auto *m1 = g.createValue();
    b->addInst(g.createMul(m1, x, one)); // x*1 -> x
    b->addInst(g.createRet(m1));

    opt::Peephole PH(g);
    PH.run(b);

    assert(countOp(b, Opcode::MUL_U64) == 0);
    assert(getRetValue(b) == x);
}

void testPH_MulPow2ToShl() {
    IRGraph g;
    auto *b = oneBlock(g);

    auto *x = g.createValue("x");
    auto *pow = g.createValue();
    b->addInst(g.createMovi(pow, 8)); // 2^3
    auto *m = g.createValue();
    b->addInst(g.createMul(m, x, pow));
    b->addInst(g.createRet(m));

    opt::Peephole PH(g);
    PH.run(b);

    assert(countOp(b, Opcode::MUL_U64) == 0);
    ShlInst *shl = findSingleShl(b);
    assert(shl && "expected a single SHL");
    uint64_t k = ~0ULL;
    assert(getConstU64(std::get<SSAValue *>(shl->operands()[1]), k) && k == 3);
    assert(getRetValue(b)->def == shl);
}

void testPH_AndIdentities() {
    IRGraph g;
    auto *b = oneBlock(g);

    auto *x = g.createValue("x");
    auto *y = x;
    auto *r = g.createValue();
    b->addInst(g.createAnd(r, x, y)); // x & x -> x
    b->addInst(g.createRet(r));

    opt::Peephole PH(g);
    PH.run(b);

    assert(countOp(b, Opcode::AND_U64) == 0);
    assert(getRetValue(b) == x);
}

void testPH_ShlRules() {
    // 1. x << 0 -> x
    {
        IRGraph g;
        auto *b = oneBlock(g);
        auto *x = g.createValue("x");
        auto *k0 = g.createValue();
        b->addInst(g.createMovi(k0, 0));
        auto *s = g.createValue();
        b->addInst(g.createShl(s, x, k0));
        b->addInst(g.createRet(s));

        opt::Peephole PH(g);
        PH.run(b);

        assert(countOp(b, Opcode::SHL_U64) == 0);
        assert(getRetValue(b) == x);
    }
    // 2. 0 << y -> 0
    {
        IRGraph g;
        auto *b = oneBlock(g);
        auto *z = g.createValue();
        b->addInst(g.createMovi(z, 0));
        auto *y = g.createValue("y");
        auto *s = g.createValue();
        b->addInst(g.createShl(s, z, y));
        b->addInst(g.createRet(s));

        opt::Peephole PH(g);
        PH.run(b);

        assert(countOp(b, Opcode::SHL_U64) == 0);
        assert(getRetImm(b) == 0);
    }
    // 3. (x<<c1) << c2 -> x << (c1+c2)
    {
        IRGraph g;
        auto *b = oneBlock(g);
        auto *x = g.createValue("x");
        auto *c1 = g.createValue();
        b->addInst(g.createMovi(c1, 4));
        auto *t = g.createValue();
        b->addInst(g.createShl(t, x, c1)); // x<<4
        auto *c2 = g.createValue();
        b->addInst(g.createMovi(c2, 3));
        auto *s = g.createValue();
        b->addInst(g.createShl(s, t, c2)); // <<3
        b->addInst(g.createRet(s));

        opt::Peephole PH(g);
        PH.run(b);

        ShlInst *shl = findSingleShl(b);
        assert(shl && "one SHL expected");
        uint64_t k = ~0ULL;
        assert(getConstU64(std::get<SSAValue *>(shl->operands()[1]), k) && k == 7);
        assert(getRetValue(b)->def == shl);
    }
}