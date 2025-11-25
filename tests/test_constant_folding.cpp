#include "analysis/constant_folding.h"
#include "analysis/ir_utils.h"
#include "test_functions.h"
#include "test_ir_helpers.h"

using namespace ir;
using namespace analysis;
using namespace testutil;

void testCF_MulShl_chain() {
    IRGraph g;
    auto *b = oneBlock(g);

    auto *v2 = g.createValue();
    b->addInst(g.createMovi(v2, 2));
    auto *v8 = g.createValue();
    b->addInst(g.createMovi(v8, 8));
    auto *vm = g.createValue();
    b->addInst(g.createMul(vm, v2, v8)); // 2*8=16
    auto *v1 = g.createValue();
    b->addInst(g.createMovi(v1, 1));
    auto *vs = g.createValue();
    b->addInst(g.createShl(vs, vm, v1)); // 16<<1=32
    b->addInst(g.createRet(vs));

    ConstantFolding CF;
    bool any = CF.run(g, b);
    assert(any);

    assert(countOp(b, Opcode::MUL_U64) == 0);
    assert(countOp(b, Opcode::SHL_U64) == 0);
    assert(getRetImm(b) == 32);
}

void testCF_And_chain() {
    IRGraph g;
    auto *b = oneBlock(g);

    auto *a = g.createValue();
    b->addInst(g.createMovi(a, 0xF0F0));
    auto *c = g.createValue();
    b->addInst(g.createMovi(c, 0x0FF0));
    auto *r = g.createValue();
    b->addInst(g.createAnd(r, a, c)); // -> 0x00F0
    b->addInst(g.createRet(r));

    ConstantFolding CF;
    bool any = CF.run(g, b);
    assert(any);

    assert(countOp(b, Opcode::AND_U64) == 0);
    assert(getRetImm(b) == 0x00F0);
}

void testCF_ShlClamp() {
    IRGraph g;
    auto *b = oneBlock(g);

    auto *one = g.createValue();
    b->addInst(g.createMovi(one, 1));
    auto *s = g.createValue();
    b->addInst(g.createMovi(s, 70)); // >=64
    auto *r = g.createValue();
    b->addInst(g.createShl(r, one, s)); // -> 0
    b->addInst(g.createRet(r));

    ConstantFolding CF;
    bool any = CF.run(g, b);
    assert(any);

    assert(countOp(b, Opcode::SHL_U64) == 0);
    assert(getRetImm(b) == 0);
}