#pragma once
#include "ir/basic_block.h"
#include "ir/inst.h"
#include <map>
#include <memory>
#include <vector>
#include <algorithm>
#include <set>
#include <iostream>

namespace ir {
struct Arg {
    std::string type;
    std::string name;
};

class IRGraph {
    std::map<std::string, BasicBlock *> labelToBlock;
    std::vector<std::unique_ptr<BasicBlock>> blocks;

    uint32_t next_val_id_ = 0;
    std::vector<std::unique_ptr<SSAValue>> all_values_;

  public:
    struct Arg {
        std::string type;
        std::string name;
        SSAValue *val{nullptr};
    };

    std::string func_ret_ = "u64";
    std::string func_name_ = "fact";
    std::vector<Arg> func_args_ = {{"u32", "a0"}};

    SSAValue *createValue(std::string dbg = "") {
        auto v = std::make_unique<SSAValue>();
        v->id = next_val_id_++;
        v->dbg_name = std::move(dbg);
        auto *ptr = v.get();
        all_values_.push_back(std::move(v));
        return ptr;
    }

    SSAValue *createArg(const std::string &type, const std::string &name) {
        SSAValue *v = createValue(name);
        v->is_arg = true;

        for (auto &a : func_args_) {
            if (a.name == name && a.type == type) {
                a.val = v;
                break;
            }
        }
        return v;
    }

    BasicBlock *createBlock(const std::string &lbl = "") {
        auto bb = std::make_unique<BasicBlock>(lbl);
        auto *ptr = bb.get();
        if (!lbl.empty())
            labelToBlock[lbl] = ptr;
        blocks.push_back(std::move(bb));
        return ptr;
    }

    BasicBlock *getBlock(const std::string &lbl) const {
        auto it = labelToBlock.find(lbl);
        return it != labelToBlock.end() ? it->second : nullptr;
    }

    std::unique_ptr<Inst> createMovi(SSAValue *res, uint64_t imm) {
        return std::make_unique<MoviInst>(res, imm);
    }
    std::unique_ptr<Inst> createCast(SSAValue *res, SSAValue *src) {
        return std::make_unique<CastInst>(res, src);
    }
    std::unique_ptr<Inst> createCmp(SSAValue *left, SSAValue *right) {
        return std::make_unique<CmpInst>(left, right);
    }
    std::unique_ptr<Inst> createJa(BasicBlock *target) {
        return std::make_unique<JaInst>(target);
    }
    std::unique_ptr<Inst> createMul(SSAValue *res, SSAValue *left, SSAValue *right) {
        return std::make_unique<MulInst>(res, left, right);
    }
    std::unique_ptr<Inst> createAddi(SSAValue *res, SSAValue *src, uint64_t imm) {
        return std::make_unique<AddiInst>(res, src, imm);
    }
    std::unique_ptr<Inst> createJmp(BasicBlock *target) {
        return std::make_unique<JmpInst>(target);
    }
    std::unique_ptr<Inst> createRet(SSAValue *src) {
        return std::make_unique<RetInst>(src);
    }
    std::unique_ptr<Inst> createPhi(SSAValue *res, std::vector<std::pair<BasicBlock *, SSAValue *>> sources) {
        return std::make_unique<PhiInst>(res, std::move(sources));
    }

    void setSignature(std::string ret, std::string name, std::vector<Arg> args) {
        func_ret_ = std::move(ret);
        func_name_ = std::move(name);
        func_args_ = std::move(args);
    }

    void print() const {
        std::cout << func_ret_ << " " << func_name_ << "(";
        for (size_t i = 0; i < func_args_.size(); ++i) {
            std::cout << func_args_[i].type << " " << func_args_[i].name;
            if (i + 1 < func_args_.size())
                std::cout << ", ";
        }
        std::cout << "):\n";
        for (const auto &bb : blocks) {
            if (!bb->label.empty() && bb->label != "entry" && bb->label != "body") {
                std::cout << bb->label << ":\n";
            }
            std::cout << bb->toString();
        }
    }

    bool checkControlFlow(const std::map<std::string, std::vector<std::string>> &expected) const {
        for (const auto &bb : blocks) {
            std::vector<std::string> actual;
            for (auto *succ : bb->successors)
                actual.push_back(succ->label);
            std::sort(actual.begin(), actual.end());
            auto it = expected.find(bb->label);
            if (it == expected.end())
                return false;
            auto exp = it->second;
            std::sort(exp.begin(), exp.end());
            if (actual != exp)
                return false;
        }
        return true;
    }

    bool checkDataFlow() const {
        for (const auto &bb : blocks) {
            for (const auto &up : bb->insts) {
                const Inst *I = up.get();

                if (auto *r = I->result()) {
                    if (r->def != I)
                        return false;
                }

                for (const auto &op : I->operands()) {
                    if (std::holds_alternative<SSAValue *>(op)) {
                        auto *v = std::get<SSAValue *>(op);
                        if (!v)
                            return false;
                        if (!v->is_arg && v->def == nullptr)
                            return false;

                        auto &users = v->users;
                        if (std::find(users.begin(), users.end(), I) == users.end())
                            return false;
                    }
                }

                if (I->opcode() == Opcode::PHI_U64) {
                    auto *P = dynamic_cast<const PhiInst *>(I);
                    if (!P)
                        return false;
                    for (auto &in : P->incomings()) {
                        auto *pred = in.first;
                        if (!pred)
                            return false;
                        if (std::find(bb->predecessors.begin(), bb->predecessors.end(), pred) == bb->predecessors.end()) {
                            return false;
                        }
                        auto *val = in.second;
                        if (!val)
                            return false;
                        if (!val->is_arg && val->def == nullptr)
                            return false;
                    }
                }
            }
        }
        return true;
    }

    bool checkNecessaryInsts(const std::map<std::string, std::set<Opcode>> &required) const {
        for (const auto &[lbl, ops] : required) {
            auto *bb = getBlock(lbl);
            if (!bb)
                return false;
            std::set<Opcode> actual;
            for (const auto &inst : bb->insts)
                actual.insert(inst->opcode());
            if (actual != ops)
                return false;
        }
        return true;
    }
};
} 