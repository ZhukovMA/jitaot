#pragma once
#include "analysis/ir_utils.h"
#include "ir/ir_graph.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace analysis {

struct Inliner {
    bool inlineStaticCall(ir::IRGraph &caller_graph, ir::BasicBlock *caller_block, ir::CallStaticInst *call,
                          const ir::IRGraph &callee_graph, ir::BasicBlock *callee_entry) {
        if (!caller_block || !call || !callee_entry) {
            return false;
        }

        auto moved = splitCallerBlock(caller_graph, caller_block, call);
        auto *call_cont_block = moved;
        std::map<ir::SSAValue *, ir::SSAValue *> value_map;
        std::map<ir::BasicBlock *, ir::BasicBlock *> block_map;
        std::vector<std::pair<ir::BasicBlock *, ir::SSAValue *>> returns;

        if (!mapParams(value_map, call, callee_graph, callee_entry)) {
            return false;
        }

        moveEntryConstants(caller_graph, caller_block, call, value_map, callee_entry);
        cloneBlocks(caller_graph, callee_graph, callee_entry, block_map);
        cloneInstructions(caller_graph, callee_entry, value_map, block_map, returns);
        cloneEdges(callee_entry, block_map);
        connectBlocks(caller_block, call_cont_block, callee_entry, block_map, returns);
        updateReturns(caller_graph, caller_block, call_cont_block, call, returns);
        eraseInst(caller_block, call);

        return true;
    }

    bool run(ir::IRGraph &caller_graph, ir::BasicBlock *caller_block, ir::CallStaticInst *call,
             const ir::IRGraph &callee_graph, ir::BasicBlock *callee_entry) {
        return inlineStaticCall(caller_graph, caller_block, call, callee_graph, callee_entry);
    }

  private:
    static ir::BasicBlock *splitCallerBlock(ir::IRGraph &g, ir::BasicBlock *call_block, ir::Inst *call) {
        auto *cont = g.createBlock(call_block->label + ".split2");

        auto it = std::find_if(call_block->insts.begin(), call_block->insts.end(), [&](auto &p) { return p.get() == call; });
        if (it == call_block->insts.end()) {
            return cont;
        }

        auto tail = it;
        ++tail;
        while (tail != call_block->insts.end()) {
            auto cur = tail++;
            cont->insts.splice(cont->insts.end(), call_block->insts, cur);
        }

        auto old_succs = call_block->successors;
        for (auto *succ : old_succs) {
            call_block->removeSuccessor(succ);
            cont->addSuccessor(succ);
            for (auto *I : succ->allInsts()) {
                if (auto *phi = dynamic_cast<ir::PhiInst *>(I)) {
                    phi->replaceIncomingBlock(call_block, cont);
                }
            }
        }
        return cont;
    }

    static bool mapParams(std::map<ir::SSAValue *, ir::SSAValue *> &value_map,
                          ir::CallStaticInst *call,
                          const ir::IRGraph &callee_graph,
                          ir::BasicBlock *callee_entry) {
        const auto &args = call->args();

        for (size_t i = 0; i < callee_graph.func_args_.size() && i < args.size(); ++i) {

            auto *formal = callee_graph.func_args_[i].val;
            if (formal) {
                value_map[formal] = args[i];
            }
        }

        for (auto *I : callee_entry->allInsts()) {
            auto *param = dynamic_cast<ir::ParameterInst *>(I);

            if (!param) {
                continue;
            }

            if (param->index() >= args.size()) {
                return false;
            }
            value_map[param->result()] = args[param->index()];
        }
        return true;
    }

    static void moveEntryConstants(ir::IRGraph &caller_graph, ir::BasicBlock *call_block, ir::Inst *call,
                                   std::map<ir::SSAValue *, ir::SSAValue *> &value_map,
                                   ir::BasicBlock *callee_entry) {
        for (auto *I : callee_entry->allInsts()) {
            if (I->opcode() != ir::Opcode::MOVI_U64) {
                continue;
            }

            auto *mov = dynamic_cast<ir::MoviInst *>(I);

            if (!mov || !mov->result()) {
                continue;
            }

            auto *new_val = caller_graph.createValue(mov->result()->dbg_name);
            call_block->insertBefore(call, caller_graph.createMovi(new_val, mov->imm()));
            value_map[mov->result()] = new_val;
        }
    }

    static void cloneBlocks(ir::IRGraph &caller_graph, const ir::IRGraph &callee_graph, ir::BasicBlock *callee_entry,
                            std::map<ir::BasicBlock *, ir::BasicBlock *> &block_map) {
        for (auto *bb : callee_graph.allBlocks()) {

            if (bb == callee_entry) {
                continue;
            }

            auto *clone = caller_graph.createBlock(bb->label + ".inl");
            block_map[bb] = clone;
        }
    }

    static ir::SSAValue *mapValue(ir::SSAValue *v, std::map<ir::SSAValue *, ir::SSAValue *> &value_map) {
        auto it = value_map.find(v);
        return it == value_map.end() ? v : it->second;
    }

    static std::unique_ptr<ir::Inst> cloneInst(ir::IRGraph &g, ir::Inst *I,
                                               std::map<ir::SSAValue *, ir::SSAValue *> &value_map,
                                               const std::map<ir::BasicBlock *, ir::BasicBlock *> &block_map) {
        using namespace ir;
        switch (I->opcode()) {
        case Opcode::MOVI_U64: {
            auto *orig = dynamic_cast<MoviInst *>(I);
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;
            return g.createMovi(res, orig->imm());
        }
        case Opcode::MUL_U64: {
            auto *orig = dynamic_cast<MulInst *>(I);
            auto ops = orig->operands();
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;
            return g.createMul(res, mapValue(std::get<SSAValue *>(ops[0]), value_map), mapValue(std::get<SSAValue *>(ops[1]), value_map));
        }
        case Opcode::ADDI_U64: {
            auto *orig = dynamic_cast<AddiInst *>(I);
            auto ops = orig->operands();
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;
            return g.createAddi(res, mapValue(std::get<SSAValue *>(ops[0]), value_map), std::get<uint64_t>(ops[1]));
        }
        case Opcode::AND_U64: {
            auto *orig = dynamic_cast<AndInst *>(I);

            auto ops = orig->operands();
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;

            return g.createAnd(res, mapValue(std::get<SSAValue *>(ops[0]), value_map), mapValue(std::get<SSAValue *>(ops[1]), value_map));
        }
        case Opcode::SHL_U64: {

            auto *orig = dynamic_cast<ShlInst *>(I);
            auto ops = orig->operands();
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;

            return g.createShl(res, mapValue(std::get<SSAValue *>(ops[0]), value_map), mapValue(std::get<SSAValue *>(ops[1]), value_map));
        }
        case Opcode::U32TOU64: {
            auto *orig = dynamic_cast<CastInst *>(I);

            auto ops = orig->operands();
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;

            return g.createCast(res, mapValue(std::get<SSAValue *>(ops[0]), value_map));
        }
        case Opcode::CMP_U64: {
            auto *orig = dynamic_cast<CmpInst *>(I);
            auto ops = orig->operands();

            return g.createCmp(mapValue(std::get<SSAValue *>(ops[0]), value_map), mapValue(std::get<SSAValue *>(ops[1]), value_map));
        }

        case Opcode::JMP: {
            auto *orig = dynamic_cast<JmpInst *>(I);
            return g.createJmp(block_map.at(orig->target()));
        }

        case Opcode::JA_U64: {
            auto *orig = dynamic_cast<JaInst *>(I);

            return g.createJa(block_map.at(orig->target()));
        }
        case Opcode::PHI_U64: {
            auto *orig = dynamic_cast<PhiInst *>(I);
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;

            std::vector<std::pair<BasicBlock *, SSAValue *>> sources;

            for (auto &src : orig->incomings()) {
                sources.push_back({block_map.at(src.first), mapValue(src.second, value_map)});
            }
            return g.createPhi(res, std::move(sources));
        }
        case Opcode::MOVE_U64: {
            auto *orig = dynamic_cast<MoveInst *>(I);
            auto ops = orig->operands();
            return g.createMove(mapValue(std::get<SSAValue *>(ops[0]), value_map), std::get<std::string>(ops[1]));
        }
        case Opcode::SPILL_U64: {
            auto *orig = dynamic_cast<SpillInst *>(I);
            auto ops = orig->operands();
            return g.createSpill(mapValue(std::get<SSAValue *>(ops[0]), value_map), std::get<uint64_t>(ops[1]));
        }
        case Opcode::FILL_U64: {
            auto *orig = dynamic_cast<FillInst *>(I);
            auto *res = g.createValue(orig->result() ? orig->result()->dbg_name : "");
            value_map[orig->result()] = res;
            return g.createFill(res, std::get<uint64_t>(orig->operands()[0]));
        }
        default:
            break;
        }

        return nullptr;
    }

    static void cloneInstructions(ir::IRGraph &caller_graph, ir::BasicBlock *callee_entry,
                                  std::map<ir::SSAValue *, ir::SSAValue *> &value_map,
                                  const std::map<ir::BasicBlock *, ir::BasicBlock *> &block_map,
                                  std::vector<std::pair<ir::BasicBlock *, ir::SSAValue *>> &returns) {
        for (auto &kv : block_map) {
            auto *orig_bb = kv.first;
            auto *clone_bb = kv.second;
            (void)callee_entry;

            for (auto *I : orig_bb->allInsts()) {

                if (I->opcode() == ir::Opcode::RET_U64) {
                    auto *ret = dynamic_cast<ir::RetInst *>(I);
                    returns.push_back({clone_bb, mapValue(ret->value(), value_map)});
                    continue;
                }
                auto cloned = cloneInst(caller_graph, I, value_map, block_map);

                if (cloned) {
                    clone_bb->addInst(std::move(cloned));
                }
            }
        }
    }

    static void cloneEdges(ir::BasicBlock *callee_entry, const std::map<ir::BasicBlock *, ir::BasicBlock *> &block_map) {
        for (auto &kv : block_map) {
            auto *orig_bb = kv.first;
            auto *clone_bb = kv.second;
            for (auto *succ : orig_bb->successors) {
                if (succ == callee_entry) {

                    continue;
                }

                auto it = block_map.find(succ);

                if (it != block_map.end()) {
                    clone_bb->addSuccessor(it->second);
                }
            }
        }
    }

    static void connectBlocks(ir::BasicBlock *caller_block,
                              ir::BasicBlock *call_cont_block,
                              ir::BasicBlock *callee_entry,
                              const std::map<ir::BasicBlock *, ir::BasicBlock *> &block_map,
                              const std::vector<std::pair<ir::BasicBlock *, ir::SSAValue *>> &returns) {
        for (auto *succ : callee_entry->successors) {
            auto it = block_map.find(succ);

            if (it != block_map.end()) {
                caller_block->addSuccessor(it->second);
            }
        }

        for (auto &ret : returns) {
            ret.first->addSuccessor(call_cont_block);
        }
    }

    static void updateReturns(ir::IRGraph &caller_graph, ir::BasicBlock *caller_block, ir::BasicBlock *call_cont_block,
                              ir::CallStaticInst *call,
                              const std::vector<std::pair<ir::BasicBlock *, ir::SSAValue *>> &returns) {
        if (!call->result() || returns.empty()) {
            return;
        }

        ir::SSAValue *replacement = nullptr;

        if (returns.size() == 1) {
            replacement = returns.front().second;
        } else {
            auto *phi_res = caller_graph.createValue(call->result()->dbg_name);
            std::vector<std::pair<ir::BasicBlock *, ir::SSAValue *>> sources = returns;
            auto phi = caller_graph.createPhi(phi_res, std::move(sources));

            auto *anchor = call_cont_block->insts.empty() ? nullptr : call_cont_block->insts.front().get();
            if (anchor) {
                call_cont_block->insertBefore(anchor, std::move(phi));
            } else {
                call_cont_block->addInst(std::move(phi));
            }

            replacement = phi_res;
        }

        replaceAllUsesWith(call->result(), replacement);
    }
};

inline bool inlineStaticCall(ir::IRGraph &caller_graph,
                             ir::BasicBlock *caller_block,
                             ir::CallStaticInst *call,
                             const ir::IRGraph &callee_graph,
                             ir::BasicBlock *callee_entry) {
    Inliner inliner;
    return inliner.inlineStaticCall(caller_graph, caller_block, call, callee_graph, callee_entry);
}

} // namespace analysis
