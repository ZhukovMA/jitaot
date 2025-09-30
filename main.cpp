#include "ir.h"
#include <cassert>
#include <algorithm>

int main() {
    IRGraph graph;
    graph.setSignature("u64", "fact", { {"u32","a0"} });

    auto* entry = graph.createBlock("entry");
    auto* loop_header = graph.createBlock("loop");
    auto* body = graph.createBlock("body");
    auto* done = graph.createBlock("done");

    entry->addInst(graph.createMovi("v0.0", 1ULL));
    entry->addInst(graph.createMovi("v1.0", 2ULL));
    entry->addInst(graph.createCast("v2.0", "a0"));
    entry->addSuccessor(loop_header);

    std::vector<std::pair<std::string, Value>> phi_v0 = {
        {"entry", Value{std::string{"v0.0"}}},
        {"body", Value{std::string{"v0.2"}}}
    };
    loop_header->addInst(graph.createPhi("v0.1", phi_v0));

    std::vector<std::pair<std::string, Value>> phi_v1 = {
        {"entry", Value{std::string{"v1.0"}}},
        {"body", Value{std::string{"v1.2"}}}
    };
    loop_header->addInst(graph.createPhi("v1.1", phi_v1));

    loop_header->addInst(graph.createCmp("v1.1", "v2.0"));
    loop_header->addInst(graph.createJa("done"));
    loop_header->addSuccessor(done);  
    loop_header->addSuccessor(body); 

    body->addInst(graph.createMul("v0.2", "v0.1", "v1.1"));
    body->addInst(graph.createAddi("v1.2", "v1.1", 1ULL));
    body->addInst(graph.createJmp("loop"));
    body->addSuccessor(loop_header);

    done->addInst(graph.createRet("v0.1"));

    graph.print();

    std::map<std::string, std::set<Opcode>> required = {
        {"entry", {Opcode::MOVI_U64, Opcode::U32TOU64}},
        {"loop", {Opcode::PHI_U64, Opcode::CMP_U64, Opcode::JA_U64}},
        {"body", {Opcode::MUL_U64, Opcode::ADDI_U64, Opcode::JMP}},
        {"done", {Opcode::RET_U64}}
    };
    assert(graph.checkNecessaryInsts(required) && "necessary insts error");

    std::map<std::string, std::vector<std::string>> expected_cf = {
        {"entry", {"loop"}},
        {"loop", {"done", "body"}},
        {"body", {"loop"}},
        {"done", {}}
    };
    assert(graph.checkControlFlow(expected_cf) && "control flow error");

    assert(graph.checkDataFlow() && "Data flow error");

    std::cout << "OK." << std::endl;
    return 0;
}