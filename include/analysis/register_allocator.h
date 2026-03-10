#pragma once

#include "analysis/liveness.h"
#include "ir/ir_graph.h"
#include "register_allocator_utils.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace analysis {

class RegisterAllocator {
public:
    RegisterAllocator(int int_regs, int float_regs) : int_regs_(int_regs), float_regs_(float_regs) {
    }

    void run(ir::IRGraph &graph, ir::BasicBlock *entry) {
        locations_.clear();
        edge_moves_.clear();

        next_int_stack_ = 0;
        next_float_stack_ = 0;

        fill_count_ = 0;
        spill_count_ = 0;
        move_count_ = 0;

        liveness_.run(entry);

        linearScanRegisterAllocation_(ir::RegClass::INT);
        linearScanRegisterAllocation_(ir::RegClass::FLOAT);

        std::unordered_map<ir::BasicBlock *, std::vector<ir::Inst *>> original;
        for (auto *bb : liveness_.linearOrder().blocks) {
            original[bb] = bb->allInsts();
        }

        materializePhiMoves_(graph, original);
        materializeSpillFill_(graph, original);
    }

    const LivenessAnalysis &liveness() const {
        return liveness_;
    }

    AllocPos locationOf(ir::SSAValue *v) const {
        auto it = locations_.find(v);
        if (it == locations_.end()) {
            return {};
        }
        return it->second;
    }

    AllocPos locationOf(const ir::Inst *I) const {
        if (!I) {
            return {};
        }
        if (!I->result()) {
            return {};
        }
        return locationOf(I->result());
    }

    const std::unordered_map<ir::SSAValue *, AllocPos> &locations() const {
        return locations_;
    }

    const std::vector<EdgeMoveAction> &edgeMoves() const {
        return edge_moves_;
    }

    int fillCount() const {
        return fill_count_;
    }

    int spillCount() const {
        return spill_count_;
    }

    int moveCount() const {
        return move_count_;
    }

    int usedStackSlots(ir::RegClass cls) const {
        if (cls == ir::RegClass::FLOAT) {
            return next_float_stack_;
        }
        return next_int_stack_;
    }
private:
    struct ActiveInterval {
        const LiveInterval *interval{nullptr};
        ir::SSAValue *value{nullptr};
        int reg{-1};
    };

    int int_regs_{0};
    int float_regs_{0};

    int next_int_stack_{0};
    int next_float_stack_{0};

    int fill_count_{0};
    int spill_count_{0};
    int move_count_{0};

    LivenessAnalysis liveness_;
    std::unordered_map<ir::SSAValue *, AllocPos> locations_;
    std::vector<EdgeMoveAction> edge_moves_;

private:
    static bool isPhi_(const ir::Inst *I) {
        return I && I->opcode() == ir::Opcode::PHI_U64;
    }

    static bool activeEndLess_(const ActiveInterval &a, const ActiveInterval &b) {
        if (a.interval->end() != b.interval->end()) {
            return a.interval->end() < b.interval->end();
        }
        return a.value->id < b.value->id;
    }

    static bool intervalStartLess_(const LiveInterval *a, const LiveInterval *b) {
        if (a->start() != b->start()) {
            return a->start() < b->start();
        }
        if (a->end() != b->end()) {
            return a->end() < b->end();
        }
        return a->value->id < b->value->id;
    }

    int regCount_(ir::RegClass cls) const {
        if (cls == ir::RegClass::FLOAT) {
            return float_regs_;
        }
        return int_regs_;
    }

    int &nextStack_(ir::RegClass cls) {
        if (cls == ir::RegClass::FLOAT) {
            return next_float_stack_;
        }
        return next_int_stack_;
    }

    int scratchReg_(ir::RegClass cls) const {
        if (cls == ir::RegClass::FLOAT) {
            if (float_regs_ == 0) {
                return -1;
            }
            return 0;
        }
        if (int_regs_ == 0) {
            return -1;
        }
        return 0;
    }

    void expireOldIntervals_(const LiveInterval *cur, std::vector<ActiveInterval> &active, std::vector<int> &free_regs) {
        std::sort(active.begin(), active.end(), activeEndLess_);

        for (int i = 0; i < (int)active.size();) {
            const ActiveInterval &j = active[i];

            if (j.interval->end() > cur->start()) {
                return;
            }

            free_regs.push_back(j.reg);
            active.erase(active.begin() + i);
        }
    }

    void spillAtInterval_(const LiveInterval *cur, ir::RegClass cls, std::vector<ActiveInterval> &active) {
        std::sort(active.begin(), active.end(), activeEndLess_);

        ActiveInterval spill = active.back();

        if (spill.interval->end() > cur->end()) {
            locations_[cur->value] = {AllocPos::Kind::REGISTER, cls, spill.reg};
            locations_[spill.value] = {AllocPos::Kind::STACK, cls, nextStack_(cls)++};

            active.pop_back();
            active.push_back({cur, cur->value, spill.reg});
            std::sort(active.begin(), active.end(), activeEndLess_);
        } else {
            locations_[cur->value] = {AllocPos::Kind::STACK, cls, nextStack_(cls)++};
        }
    }

    void linearScanRegisterAllocation_(ir::RegClass cls) {
        std::vector<const LiveInterval *> intervals;
        for (auto &kv : liveness_.intervals()) {
            if (kv.first && kv.first->reg_class == cls) {
                intervals.push_back(&kv.second);
            }
        }

        std::sort(intervals.begin(), intervals.end(), intervalStartLess_);

        std::vector<ActiveInterval> active;
        std::vector<int> free_regs;

        int R = regCount_(cls);
        for (int i = R - 1; i >= 0; --i) {
            free_regs.push_back(i);
        }

        for (const LiveInterval *i : intervals) {
            expireOldIntervals_(i, active, free_regs);

            if (R == 0) {
                locations_[i->value] = {AllocPos::Kind::STACK, cls, nextStack_(cls)++};
                continue;
            }

            if ((int)active.size() == R) {
                spillAtInterval_(i, cls, active);
            } else {
                int reg = free_regs.back();
                free_regs.pop_back();

                locations_[i->value] = {AllocPos::Kind::REGISTER, cls, reg};
                active.push_back({i, i->value, reg});
                std::sort(active.begin(), active.end(), activeEndLess_);
            }
        }
    }

    void materializePhiMoves_(ir::IRGraph &graph,
                              const std::unordered_map<ir::BasicBlock *, std::vector<ir::Inst *>> &original) {
        edge_moves_.clear();
        move_count_ = 0;

        for (auto *bb : liveness_.linearOrder().blocks) {
            for (auto *succ : bb->successors) {
                if (!succ) {
                    continue;
                }

                ir::Inst *anchor = nullptr;
                auto it_orig = original.find(bb);
                if (it_orig != original.end() && !it_orig->second.empty()) {
                    anchor = it_orig->second.back();
                }

                for (auto *I : succ->allInsts()) {
                    if (!isPhi_(I)) {
                        break;
                    }

                    auto *phi = dynamic_cast<ir::PhiInst *>(I);
                    if (!phi) {
                        continue;
                    }

                    AllocPos dst = locationOf(phi->result());

                    for (auto &in : phi->incomings()) {
                        if (in.first != bb || !in.second) {
                            continue;
                        }

                        AllocPos src = locationOf(in.second);

                        if (src == dst) {
                            continue;
                        }

                        edge_moves_.push_back({bb, succ, in.second, src, dst});

                        auto mv = graph.createMove(in.second, dst.toString());
                        if (anchor) {
                            bb->insertBefore(anchor, std::move(mv));
                        } else {
                            bb->addInst(std::move(mv));
                        }

                        ++move_count_;
                    }
                }
            }
        }
    }

    void materializeSpillFill_(ir::IRGraph &graph,
                               const std::unordered_map<ir::BasicBlock *, std::vector<ir::Inst *>> &original) {
        fill_count_ = 0;
        spill_count_ = 0;

        for (auto *bb : liveness_.linearOrder().blocks) {
            auto it = original.find(bb);
            if (it == original.end()) {
                continue;
            }

            for (auto *I : it->second) {
                if (isPhi_(I)) {
                    continue;
                }
                if (I->opcode() == ir::Opcode::MOVE_U64) {
                    continue;
                }
                if (I->opcode() == ir::Opcode::SPILL_U64) {
                    continue;
                }
                if (I->opcode() == ir::Opcode::FILL_U64) {
                    continue;
                }

                auto ops = I->operands();
                for (auto &op : ops) {
                    if (!std::holds_alternative<ir::SSAValue *>(op)) {
                        continue;
                    }

                    auto *v = std::get<ir::SSAValue *>(op);
                    if (!v) {
                        continue;
                    }

                    AllocPos home = locationOf(v);
                    if (home.kind != AllocPos::Kind::STACK) {
                        continue;
                    }

                    auto *tmp = graph.createValue(v->dbg_name + ".fill");
                    tmp->reg_class = v->reg_class;
                    locations_[tmp] = {AllocPos::Kind::REGISTER, v->reg_class, scratchReg_(v->reg_class)};

                    bb->insertBefore(I, graph.createFill(tmp, home.index));
                    I->replaceOperand(v, tmp);
                    ++fill_count_;
                }

                auto *res = I->result();
                if (!res) {
                    continue;
                }

                AllocPos out = locationOf(res);
                if (out.kind == AllocPos::Kind::STACK) {
                    bb->insertAfter(I, graph.createSpill(res, out.index));
                    ++spill_count_;
                }
            }
        }
    }
};

} // namespace analysis