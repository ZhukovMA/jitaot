#pragma once
#include <memory>
#include <string>
#include <vector>
#include <list>

#include "ir/inst.h"

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
} 
