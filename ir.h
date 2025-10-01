#pragma once
#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <map>
#include <iostream>
#include <cstdint>
#include <set>
#include <algorithm>
#include <iomanip>  
#include <sstream>  

using Value = std::variant<std::string, uint64_t>;

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

class Inst {
public:
    virtual ~Inst() = default;
    virtual Opcode opcode() const = 0;
    virtual std::string result() const { return ""; }
    virtual std::vector<Value> operands() const = 0;
    virtual std::string toString() const = 0;
};

class MoviInst : public Inst {
    std::string res_;
    uint64_t imm_;
public:
    MoviInst(std::string res, uint64_t imm) : res_(std::move(res)), imm_(imm) {}
    Opcode opcode() const override { return Opcode::MOVI_U64; }
    std::string result() const override { return res_; }
    std::vector<Value> operands() const override { return {Value{imm_}}; }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "movi.u64    " << res_ << ", " << imm_;
        return oss.str();
    }
};

class CastInst : public Inst {
    std::string res_, src_;
public:
    CastInst(std::string res, std::string src) : res_(std::move(res)), src_(std::move(src)) {}
    Opcode opcode() const override { return Opcode::U32TOU64; }
    std::string result() const override { return res_; }
    std::vector<Value> operands() const override { return {Value{src_}}; }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "u32tou64    " << res_ << ", " << src_;
        return oss.str();
    }
};

class CmpInst : public Inst {
    std::string left_, right_;
public:
    CmpInst(std::string left, std::string right) : left_(std::move(left)), right_(std::move(right)) {}
    Opcode opcode() const override { return Opcode::CMP_U64; }
    std::vector<Value> operands() const override { return {Value{left_}, Value{right_}}; }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "cmp.u64     " << left_ << ", " << right_;
        return oss.str();
    }
};

class JaInst : public Inst {
    std::string label_;
public:
    JaInst(std::string label) : label_(std::move(label)) {}
    Opcode opcode() const override { return Opcode::JA_U64; }
    std::vector<Value> operands() const override { return {Value{label_}}; }
    std::string toString() const override { return "ja          " + label_; }
};

class MulInst : public Inst {
    std::string res_, left_, right_;
public:
    MulInst(std::string res, std::string left, std::string right)
        : res_(std::move(res)), left_(std::move(left)), right_(std::move(right)) {}
    Opcode opcode() const override { return Opcode::MUL_U64; }
    std::string result() const override { return res_; }
    std::vector<Value> operands() const override { return {Value{left_}, Value{right_}}; }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "mul.u64     " << res_ << ", " << left_ << ", " << right_;
        return oss.str();
    }
};

class AddiInst : public Inst {
    std::string res_, src_;
    uint64_t imm_;
public:
    AddiInst(std::string res, std::string src, uint64_t imm)
        : res_(std::move(res)), src_(std::move(src)), imm_(imm) {}
    Opcode opcode() const override { return Opcode::ADDI_U64; }
    std::string result() const override { return res_; }
    std::vector<Value> operands() const override { return {Value{src_}, Value{imm_}}; }
    std::string toString() const override {
        std::ostringstream oss;
        oss << "addi.u64    " << res_ << ", " << src_ << ", " << imm_;
        return oss.str();
    }
};

class JmpInst : public Inst {
    std::string label_;
public:
    JmpInst(std::string label) : label_(std::move(label)) {}
    Opcode opcode() const override { return Opcode::JMP; }
    std::vector<Value> operands() const override { return {Value{label_}}; }
    std::string toString() const override { return "jmp         " + label_; }
};

class RetInst : public Inst {
    std::string src_;
public:
    RetInst(std::string src) : src_(std::move(src)) {}
    Opcode opcode() const override { return Opcode::RET_U64; }
    std::vector<Value> operands() const override { return {Value{src_}}; }
    std::string toString() const override { return "ret.u64     " + src_; }
};

class PhiInst : public Inst {
    std::string res_;
    std::vector<std::pair<std::string, Value>> sources_;
public:
    PhiInst(std::string res, std::vector<std::pair<std::string, Value>> sources)
        : res_(std::move(res)), sources_(std::move(sources)) {}
    Opcode opcode() const override { return Opcode::PHI_U64; }
    std::string result() const override { return res_; }
    std::vector<Value> operands() const override {
        std::vector<Value> ops;
        for (auto& p : sources_) ops.push_back(p.second);
        return ops;
    }
    std::string toString() const override {
        std::string s = "phi.u64     " + res_ + " = ";
        for (size_t i = 0; i < sources_.size(); ++i) {
            if (i > 0) s += ", ";
            s += sources_[i].first + ": " + std::get<std::string>(sources_[i].second);
        }
        return s;
    }
};

class BasicBlock {
public:
    std::string label;
    std::vector<std::unique_ptr<Inst>> insts;
    std::vector<BasicBlock*> successors;
    std::vector<BasicBlock*> predecessors;

    BasicBlock(std::string lbl = "") : label(std::move(lbl)) {}

    void addInst(std::unique_ptr<Inst> inst) {
        insts.push_back(std::move(inst));
    }

    void addSuccessor(BasicBlock* succ) {
        successors.push_back(succ);
        succ->predecessors.push_back(this);
    }

    std::string toString() const {
        std::string s;
        for (const auto& inst : insts) {
            s += "    " + inst->toString() + "\n";
        }
        return s;
    }
};

class IRGraph {
    std::map<std::string, BasicBlock*> labelToBlock;
    std::vector<std::unique_ptr<BasicBlock>> blocks;

public:

    struct Arg {
        std::string type;   
        std::string name;   
    };

    std::string func_ret_ = "u64";
    std::string func_name_ = "fact";
    std::vector<Arg> func_args_ = { {"u32", "a0"} };

    BasicBlock* createBlock(const std::string& lbl = "") {
        auto bb = std::make_unique<BasicBlock>(lbl);
        auto* ptr = bb.get();
        if (!lbl.empty()) labelToBlock[lbl] = ptr;
        blocks.push_back(std::move(bb));
        return ptr;
    }

    BasicBlock* getBlock(const std::string& lbl) const {
        auto it = labelToBlock.find(lbl);
        return it != labelToBlock.end() ? it->second : nullptr;
    }

    std::unique_ptr<Inst> createMovi(std::string res, uint64_t imm) {
        return std::make_unique<MoviInst>(std::move(res), imm);
    }

    std::unique_ptr<Inst> createCast(std::string res, std::string src) {
        return std::make_unique<CastInst>(std::move(res), std::move(src));
    }

    std::unique_ptr<Inst> createCmp(std::string left, std::string right) {
        return std::make_unique<CmpInst>(std::move(left), std::move(right));
    }

    std::unique_ptr<Inst> createJa(std::string label) {
        return std::make_unique<JaInst>(std::move(label));
    }

    std::unique_ptr<Inst> createMul(std::string res, std::string left, std::string right) {
        return std::make_unique<MulInst>(std::move(res), std::move(left), std::move(right));
    }

    std::unique_ptr<Inst> createAddi(std::string res, std::string src, uint64_t imm) {
        return std::make_unique<AddiInst>(std::move(res), std::move(src), imm);
    }

    std::unique_ptr<Inst> createJmp(std::string label) {
        return std::make_unique<JmpInst>(std::move(label));
    }

    std::unique_ptr<Inst> createRet(std::string src) {
        return std::make_unique<RetInst>(std::move(src));
    }

    std::unique_ptr<Inst> createPhi(std::string res, std::vector<std::pair<std::string, Value>> sources) {
        return std::make_unique<PhiInst>(std::move(res), std::move(sources));
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
            if (i + 1 < func_args_.size()) std::cout << ", ";
        }
        std::cout << "):\n";
        for (const auto& bb : blocks) {
            if (!bb->label.empty() && bb->label != "entry" && bb->label != "body") {
                std::cout << bb->label << ":\n";
            }
            std::cout << bb->toString();
        }
    }

    bool checkControlFlow(const std::map<std::string, std::vector<std::string>>& expected) const {
        for (const auto& bb : blocks) {
            std::vector<std::string> actual;
            for (auto* succ : bb->successors) actual.push_back(succ->label);
            std::sort(actual.begin(), actual.end());
            auto it = expected.find(bb->label);
            if (it == expected.end()) return false;
            auto exp = it->second;
            std::sort(exp.begin(), exp.end());
            if (actual != exp) return false;
        }
        return true;
    }

    bool checkDataFlow() const {
        for (const auto& bb : blocks) {
            std::set<std::string> defs, uses;
            for (const auto& inst : bb->insts) {
                if (!inst->result().empty()) {
                    defs.insert(inst->result());  
                }
                for (const auto& op : inst->operands()) {
                    if (std::holds_alternative<std::string>(op)) {
                        uses.insert(std::get<std::string>(op));  
                    }
                }
            }

            size_t versioned = std::count_if(defs.begin(), defs.end(), [](const std::string& d) {
                return d.find(".") != std::string::npos;
            });
            if (defs.size() != versioned) return false;

            std::set<std::string> local_uses;
            for (const auto& u : uses) {
                size_t dot_pos = u.find(".");
                if (dot_pos == std::string::npos && u != "a0") {
                    local_uses.insert(u);
                }
            }

            for (const auto& d : defs) {
                if (local_uses.count(d)) return false;  
            }
        }
        return true;
    }

    bool checkNecessaryInsts(const std::map<std::string, std::set<Opcode>>& required) const {
        for (const auto& [lbl, ops] : required) {
            auto* bb = getBlock(lbl);
            if (!bb) return false;
            std::set<Opcode> actual;
            for (const auto& inst : bb->insts) actual.insert(inst->opcode());
            if (actual != ops) return false;
        }
        return true;
    }
};
