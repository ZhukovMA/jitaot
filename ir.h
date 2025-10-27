#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

struct SSAValue;
class Inst;
class BasicBlock;

enum class Opcode {
    MOVI_U64,
    U32TOU64,
    CMP_U64,
    JA_U64,
    MUL_U64,
    ADDI_U64,
    JMP,
    RET_U64,
    PHI_U64
};

using Operand = std::variant<SSAValue *, uint64_t, BasicBlock *>;

struct SSAValue {
    uint32_t id{};
    Inst *def{nullptr};
    std::vector<Inst *> users;
    bool is_arg{false};
    std::string dbg_name;

    void addUser(Inst *I) {
        users.push_back(I);
    }
};

static std::string fmtVal(const SSAValue *v) {
    if (!v)
        return "<null>";
    if (v->is_arg && !v->dbg_name.empty())
        return v->dbg_name;
    std::ostringstream oss;
    if (!v->dbg_name.empty()) {
        oss << v->dbg_name;
    } else {
        oss << "v" << v->id;
    }
    return oss.str();
}

class Inst {
  public:
    virtual ~Inst() = default;
    virtual Opcode opcode() const = 0;
    virtual SSAValue *result() const {
        return nullptr;
    }
    virtual std::vector<Operand> operands() const = 0;
    virtual std::string toString() const = 0;
};

static std::string bbName(const BasicBlock *bb);

class MoviInst : public Inst {
    SSAValue *res_;
    uint64_t imm_;

  public:
    MoviInst(SSAValue *res, uint64_t imm) : res_(res), imm_(imm) {
        if (res_)
            res_->def = this;
    }
    Opcode opcode() const override {
        return Opcode::MOVI_U64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Operand> operands() const override {
        return {Operand{imm_}};
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "movi.u64    " << fmtVal(res_) << ", " << imm_;
        return oss.str();
    }
};

class CastInst : public Inst {
    SSAValue *res_;
    SSAValue *src_;

  public:
    CastInst(SSAValue *res, SSAValue *src) : res_(res), src_(src) {
        if (res_)
            res_->def = this;
        if (src_)
            src_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::U32TOU64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Operand> operands() const override {
        return {Operand{src_}};
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "u32tou64    " << fmtVal(res_) << ", " << fmtVal(src_);
        return oss.str();
    }
};

class CmpInst : public Inst {
    SSAValue *left_;
    SSAValue *right_;

  public:
    CmpInst(SSAValue *left, SSAValue *right) : left_(left), right_(right) {
        if (left_)
            left_->addUser(this);
        if (right_)
            right_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::CMP_U64;
    }
    std::vector<Operand> operands() const override {
        return {Operand{left_}, Operand{right_}};
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "cmp.u64     " << fmtVal(left_) << ", " << fmtVal(right_);
        return oss.str();
    }
};

class JaInst : public Inst {
    BasicBlock *target_;

  public:
    explicit JaInst(BasicBlock *target) : target_(target) {
    }
    Opcode opcode() const override {
        return Opcode::JA_U64;
    }
    std::vector<Operand> operands() const override {
        return {Operand{target_}};
    }
    std::string toString() const override {
        return "ja          " + bbName(target_);
    }
};

class MulInst : public Inst {
    SSAValue *res_;
    SSAValue *left_;
    SSAValue *right_;

  public:
    MulInst(SSAValue *res, SSAValue *left, SSAValue *right)
        : res_(res), left_(left), right_(right) {
        if (res_)
            res_->def = this;
        if (left_)
            left_->addUser(this);
        if (right_)
            right_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::MUL_U64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Operand> operands() const override {
        return {Operand{left_}, Operand{right_}};
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "mul.u64     " << fmtVal(res_) << ", " << fmtVal(left_) << ", " << fmtVal(right_);
        return oss.str();
    }
};

class AddiInst : public Inst {
    SSAValue *res_;
    SSAValue *src_;
    uint64_t imm_;

  public:
    AddiInst(SSAValue *res, SSAValue *src, uint64_t imm)
        : res_(res), src_(src), imm_(imm) {
        if (res_)
            res_->def = this;
        if (src_)
            src_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::ADDI_U64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Operand> operands() const override {
        return {Operand{src_}, Operand{imm_}};
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "addi.u64    " << fmtVal(res_) << ", " << fmtVal(src_) << ", " << imm_;
        return oss.str();
    }
};

class JmpInst : public Inst {
    BasicBlock *target_;

  public:
    explicit JmpInst(BasicBlock *target) : target_(target) {
    }
    Opcode opcode() const override {
        return Opcode::JMP;
    }
    std::vector<Operand> operands() const override {
        return {Operand{target_}};
    }
    std::string toString() const override {
        return "jmp         " + bbName(target_);
    }
};

class RetInst : public Inst {
    SSAValue *src_;

  public:
    explicit RetInst(SSAValue *src) : src_(src) {
        if (src_)
            src_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::RET_U64;
    }
    std::vector<Operand> operands() const override {
        return {Operand{src_}};
    }
    std::string toString() const override {
        return "ret.u64     " + fmtVal(src_);
    }
};

class PhiInst : public Inst {
    SSAValue *res_;
    std::vector<std::pair<BasicBlock *, SSAValue *>> sources_;

  public:
    PhiInst(SSAValue *res, std::vector<std::pair<BasicBlock *, SSAValue *>> sources)
        : res_(res), sources_(std::move(sources)) {
        if (res_)
            res_->def = this;
        for (auto &[bb, val] : sources_) {
            (void)bb;
            if (val)
                val->addUser(this);
        }
    }
    Opcode opcode() const override {
        return Opcode::PHI_U64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Operand> operands() const override {
        std::vector<Operand> ops;
        ops.reserve(sources_.size());
        for (auto &p : sources_)
            ops.push_back(Operand{p.second});
        return ops;
    }
    const auto &incomings() const {
        return sources_;
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "phi.u64     " << fmtVal(res_) << " = ";
        for (size_t i = 0; i < sources_.size(); ++i) {
            if (i > 0)
                oss << ", ";
            oss << bbName(sources_[i].first) << ": " << fmtVal(sources_[i].second);
        }
        return oss.str();
    }
};

class BasicBlock {
  public:
    std::string label;
    std::list<std::unique_ptr<Inst>> insts;
    std::vector<BasicBlock *> successors;
    std::vector<BasicBlock *> predecessors;

    explicit BasicBlock(std::string lbl = "") : label(std::move(lbl)) {
    }

    void addInst(std::unique_ptr<Inst> inst) {
        insts.push_back(std::move(inst));
    }

    void addSuccessor(BasicBlock *succ) {
        successors.push_back(succ);
        succ->predecessors.push_back(this);
    }

    std::string toString() const {
        std::string s;
        for (const auto &inst : insts) {
            s += "    " + inst->toString() + "\n";
        }
        return s;
    }
};

static std::string bbName(const BasicBlock *bb) {
    return bb ? bb->label : "<nullbb>";
}

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