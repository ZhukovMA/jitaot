#include "ir.h"
#include <algorithm>
#include <cassert>

int main() {
    IRGraph graph;
    graph.setSignature("u64", "fact", {{"u32", "a0"}});

    auto *entry = graph.createBlock("entry");
    auto *loop_header = graph.createBlock("loop");
    auto *body = graph.createBlock("body");
    auto *done = graph.createBlock("done");

    SSAValue *a0 = graph.createArg("u32", "a0");

    SSAValue *v0_0 = graph.createValue();
    SSAValue *v1_0 = graph.createValue();
    SSAValue *v2_0 = graph.createValue();
    SSAValue *v0_1 = graph.createValue();
    SSAValue *v1_1 = graph.createValue();
    SSAValue *v0_2 = graph.createValue();
    SSAValue *v1_2 = graph.createValue();

    entry->addInst(graph.createMovi(v0_0, 1ULL));
    entry->addInst(graph.createMovi(v1_0, 2ULL));
    entry->addInst(graph.createCast(v2_0, a0));
    entry->addSuccessor(loop_header);

    {
        std::vector<std::pair<BasicBlock *, SSAValue *>> phi_v0 = {
            {entry, v0_0},
            {body, v0_2}};
        loop_header->addInst(graph.createPhi(v0_1, phi_v0));
    }
    {
        std::vector<std::pair<BasicBlock *, SSAValue *>> phi_v1 = {
            {entry, v1_0},
            {body, v1_2}};
        loop_header->addInst(graph.createPhi(v1_1, phi_v1));
    }

    loop_header->addInst(graph.createCmp(v1_1, v2_0));
    loop_header->addInst(graph.createJa(done));
    loop_header->addSuccessor(done);
    loop_header->addSuccessor(body);

    body->addInst(graph.createMul(v0_2, v0_1, v1_1));
    body->addInst(graph.createAddi(v1_2, v1_1, 1ULL));
    body->addInst(graph.createJmp(loop_header));
    body->addSuccessor(loop_header);

    done->addInst(graph.createRet(v0_1));

    graph.print();

    std::map<std::string, std::set<Opcode>> required = {
        {"entry", {Opcode::MOVI_U64, Opcode::U32TOU64}},
        {"loop", {Opcode::PHI_U64, Opcode::CMP_U64, Opcode::JA_U64}},
        {"body", {Opcode::MUL_U64, Opcode::ADDI_U64, Opcode::JMP}},
        {"done", {Opcode::RET_U64}}};
    assert(graph.checkNecessaryInsts(required) && "necessary insts error");

    std::map<std::string, std::vector<std::string>> expected_cf = {
        {"entry", {"loop"}},
        {"loop", {"done", "body"}},
        {"body", {"loop"}},
        {"done", {}}};
    assert(graph.checkControlFlow(expected_cf) && "control flow error");

    assert(graph.checkDataFlow() && "Data flow error");

    std::cout << "OK." << std::endl;
    return 0;
}