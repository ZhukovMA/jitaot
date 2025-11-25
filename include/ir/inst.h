#pragma once
#include "ir/opcode.h"
#include "ir/value.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

    virtual void replaceOperand(SSAValue *, SSAValue *) {
    }
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

    uint64_t imm() const {
        return imm_;
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

    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (src_ == from) {
            src_ = to;
            to->addUser(this);
        }
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

    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (left_ == from) {
            left_ = to;
            to->addUser(this);
        }
        if (right_ == from) {
            right_ = to;
            to->addUser(this);
        }
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
    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (left_ == from) {
            left_ = to;
            to->addUser(this);
        }
        if (right_ == from) {
            right_ = to;
            to->addUser(this);
        }
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

    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (src_ == from) {
            src_ = to;
            to->addUser(this);
        }
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

    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (src_ == from) {
            src_ = to;
            to->addUser(this);
        }
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

    void replaceOperand(SSAValue *from, SSAValue *to) override {
        for (auto &kv : sources_) {
            if (kv.second == from) {
                kv.second = to;
                to->addUser(this);
            }
        }
    }
};

class AndInst : public Inst {
    SSAValue *res_;
    SSAValue *x_;
    SSAValue *y_;

  public:
    AndInst(SSAValue *r, SSAValue *x, SSAValue *y) : res_(r), x_(x), y_(y) {
        if (res_)
            res_->def = this;
        if (x_)
            x_->addUser(this);
        if (y_)
            y_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::AND_U64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Value> operands() const override {
        return {Value{x_}, Value{y_}};
    }
    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (x_ == from) {
            x_ = to;
            to->addUser(this);
        }
        if (y_ == from) {
            y_ = to;
            to->addUser(this);
        }
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "and.u64     " << fmtVal(res_) << ", " << fmtVal(x_) << ", " << fmtVal(y_);
        return oss.str();
    }
};

class ShlInst : public Inst {
    SSAValue *res_;
    SSAValue *x_;
    SSAValue *s_;

  public:
    ShlInst(SSAValue *r, SSAValue *x, SSAValue *s) : res_(r), x_(x), s_(s) {
        if (res_)
            res_->def = this;
        if (x_)
            x_->addUser(this);
        if (s_)
            s_->addUser(this);
    }
    Opcode opcode() const override {
        return Opcode::SHL_U64;
    }
    SSAValue *result() const override {
        return res_;
    }
    std::vector<Value> operands() const override {
        return {Value{x_}, Value{s_}};
    }
    void replaceOperand(SSAValue *from, SSAValue *to) override {
        if (x_ == from) {
            x_ = to;
            to->addUser(this);
        }
        if (s_ == from) {
            s_ = to;
            to->addUser(this);
        }
    }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "shl.u64     " << fmtVal(res_) << ", " << fmtVal(x_) << ", " << fmtVal(s_);
        return oss.str();
    }
};

} // namespace ir