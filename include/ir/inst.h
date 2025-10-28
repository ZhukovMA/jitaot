#pragma once
#include "ir/opcode.h"
#include "ir/value.h"
#include <memory>
#include <string>
#include <vector>
#include <sstream>

namespace ir {

class BasicBlock; 


static inline std::string bbName(const BasicBlock *b) {
    
    return b ? std::string("<bb>") : std::string("<null>");
}
static inline std::string fmtVal(const SSAValue *v) {
    if (!v)
        return "<null>";
    if (!v->dbg_name.empty())
        return v->dbg_name;
    return "v" + std::to_string(v->id);
}

class Inst {
  public:
    virtual ~Inst() = default;
    virtual Opcode opcode() const = 0;
    
    virtual SSAValue *result() const {
        return nullptr;
    }
    
    virtual std::vector<Value> operands() const = 0;
    virtual std::string toString() const = 0;
};

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
    std::vector<Value> operands() const override {
        return {Value{imm_}};
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
    std::vector<Value> operands() const override {
        return {Value{src_}};
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
    std::vector<Value> operands() const override {
        return {Value{left_}, Value{right_}};
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
    std::vector<Value> operands() const override {
        return {Value{bbName(target_)}};
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
    std::vector<Value> operands() const override {
        return {Value{left_}, Value{right_}};
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
    std::vector<Value> operands() const override {
        return {Value{src_}, Value{imm_}};
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
    std::vector<Value> operands() const override {
        return {Value{bbName(target_)}};
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
    std::vector<Value> operands() const override {
        return {Value{src_}};
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
    std::vector<Value> operands() const override {
        std::vector<Value> ops;
        ops.reserve(sources_.size());
        for (auto &p : sources_)
            ops.push_back(Value{p.second});
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

} 