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

struct Inst {
    Opcode op{};
    std::string res;                                  
    std::vector<Value> ops;                           
    std::vector<std::pair<std::string, Value>> phi;   

    Opcode opcode() const { return op; }
    std::string result() const { return res; }
    
    std::vector<Value> operands() const {
        if (op != Opcode::PHI_U64) return ops;
        std::vector<Value> v; v.reserve(phi.size());
        for (auto &p : phi) v.push_back(p.second);
        return v;
    }

    static std::string valToStr(const Value& v) {
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        std::ostringstream oss; oss << std::get<uint64_t>(v); return oss.str();
    }

    std::string toString() const {
        std::ostringstream oss;
        auto sp = "    "; 

        switch (op) {
            case Opcode::MOVI_U64:
                oss << "movi.u64" << sp << res << ", " << valToStr(ops.at(0)); break;
            case Opcode::U32TOU64:
                oss << "u32tou64" << sp << res << ", " << valToStr(ops.at(0)); break;
            case Opcode::CMP_U64:
                oss << "cmp.u64"  << sp << valToStr(ops.at(0)) << ", " << valToStr(ops.at(1)); break;
            case Opcode::JA_U64:
                oss << "ja"        << sp << valToStr(ops.at(0)); break;
            case Opcode::MUL_U64:
                oss << "mul.u64"  << sp << res << ", " << valToStr(ops.at(0)) << ", " << valToStr(ops.at(1)); break;
            case Opcode::ADDI_U64:
                oss << "addi.u64" << sp << res << ", " << valToStr(ops.at(0)) << ", " << valToStr(ops.at(1)); break;
            case Opcode::JMP:
                oss << "jmp"       << sp << valToStr(ops.at(0)); break;
            case Opcode::RET_U64:
                oss << "ret.u64"   << sp << valToStr(ops.at(0)); break;
            case Opcode::PHI_U64: {
                oss << "phi.u64"   << sp << res << " = ";
                for (size_t i = 0; i < phi.size(); ++i) {
                    if (i) oss << ", ";
                    oss << phi[i].first << ": " << valToStr(phi[i].second);
                }
                break;
            }
        }
        return oss.str();
    }
    
    static Inst Movi(std::string r, uint64_t imm) {
        return {Opcode::MOVI_U64, std::move(r), {Value{imm}}, {}};
    }
    static Inst Cast(std::string r, std::string src) {
        return {Opcode::U32TOU64, std::move(r), {Value{std::move(src)}}, {}};
    }
    static Inst Cmp(std::string l, std::string r) {
        return {Opcode::CMP_U64,  "", {Value{std::move(l)}, Value{std::move(r)}}, {}};
    }
    static Inst Ja(std::string label) {
        return {Opcode::JA_U64,   "", {Value{std::move(label)}}, {}};
    }
    static Inst Mul(std::string r, std::string l, std::string rr) {
        return {Opcode::MUL_U64,  std::move(r), {Value{std::move(l)}, Value{std::move(rr)}}, {}};
    }
    static Inst Addi(std::string r, std::string src, uint64_t imm) {
        return {Opcode::ADDI_U64, std::move(r), {Value{std::move(src)}, Value{imm}}, {}};
    }
    static Inst Jmp(std::string label) {
        return {Opcode::JMP,      "", {Value{std::move(label)}}, {}};
    }
    static Inst Ret(std::string src) {
        return {Opcode::RET_U64,  "", {Value{std::move(src)}}, {}};
    }
    static Inst Phi(std::string r, std::vector<std::pair<std::string, Value>> sources) {
        return {Opcode::PHI_U64,  std::move(r), {}, std::move(sources)};
    }
};

class BasicBlock {
public:
    std::string label;
    std::vector<Inst> insts;
    std::vector<BasicBlock*> successors;
    std::vector<BasicBlock*> predecessors;

    explicit BasicBlock(std::string lbl = "") : label(std::move(lbl)) {}

    void addInst(const Inst& i) { insts.push_back(i); }

    void addSuccessor(BasicBlock* succ) {
        successors.push_back(succ);
        succ->predecessors.push_back(this);
    }

    std::string toString() const {
        std::string s;
        for (const auto& inst : insts) s += "    " + inst.toString() + "\n";
        return s;
    }
};

class IRGraph {
    std::map<std::string, BasicBlock*> labelToBlock;
    std::vector<std::unique_ptr<BasicBlock>> blocks;

public:
    struct Arg { std::string type; std::string name; };

    std::string func_ret_  = "u64";
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
    
    Inst createMovi(std::string res, uint64_t imm) { return Inst::Movi(std::move(res), imm); }
    Inst createCast(std::string res, std::string src){ return Inst::Cast(std::move(res), std::move(src)); }
    Inst createCmp(std::string l, std::string r)     { return Inst::Cmp(std::move(l), std::move(r)); }
    Inst createJa(std::string label)                 { return Inst::Ja(std::move(label)); }
    Inst createMul(std::string r, std::string l, std::string rr){ return Inst::Mul(std::move(r), std::move(l), std::move(rr)); }
    Inst createAddi(std::string r, std::string s, uint64_t imm) { return Inst::Addi(std::move(r), std::move(s), imm); }
    Inst createJmp(std::string label)                { return Inst::Jmp(std::move(label)); }
    Inst createRet(std::string src)                  { return Inst::Ret(std::move(src)); }
    Inst createPhi(std::string r, std::vector<std::pair<std::string, Value>> srcs) {
        return Inst::Phi(std::move(r), std::move(srcs));
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
                if (!inst.result().empty()) defs.insert(inst.result());

                for (const auto& op : inst.operands()) {
                    if (std::get_if<std::string>(&op)) {
                        uses.insert(std::get<std::string>(op));
                    }
                }
            }

            size_t versioned = std::count_if(defs.begin(), defs.end(),
                [](const std::string& d){ return d.find(".") != std::string::npos; });
            if (defs.size() != versioned) return false;

            std::set<std::string> local_uses;
            for (const auto& u : uses) {
                size_t dot_pos = u.find(".");
                if (dot_pos == std::string::npos && u != "a0") local_uses.insert(u);
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
            for (const auto& inst : bb->insts) actual.insert(inst.opcode());
            if (actual != ops) return false;
        }
        return true;
    }
};