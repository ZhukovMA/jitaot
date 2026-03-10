#pragma once
#include "ir/inst.h"
#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace ir {

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

    std::vector<Inst *> allInsts() const {
        std::vector<Inst *> out;
        out.reserve(insts.size());
        for (auto &up : const_cast<std::list<std::unique_ptr<Inst>> &>(insts))
            out.push_back(up.get());
        return out;
    }

    void insertBefore(Inst *anchor, std::unique_ptr<Inst> nu) {
        auto it = std::find_if(insts.begin(), insts.end(), [&](auto &p) { return p.get() == anchor; });
        insts.insert(it, std::move(nu));
    }

    void erase(Inst *I) {
        auto it = std::find_if(insts.begin(), insts.end(), [&](auto &p) { return p.get() == I; });
        if (it != insts.end())
            insts.erase(it);
    }


    void insertAfter(Inst *anchor, std::unique_ptr<Inst> nu) {
        auto it = std::find_if(insts.begin(), insts.end(), [&](auto &p) { return p.get() == anchor; });
        if (it == insts.end()) {
            insts.push_back(std::move(nu));
            return;
        }
        ++it;
        insts.insert(it, std::move(nu));
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
} // namespace ir