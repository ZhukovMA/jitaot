#pragma once
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct Block;

struct Inst {
    int pc = 0;
    std::string text;
    std::string op;

    std::string def;
    std::vector<std::string> useNames;

    int target_pc = -1;
    bool isCondBr = false;
    bool isUncondBr = false;
    bool isRet = false;

    Inst *prev = nullptr;
    Inst *next = nullptr;
    Block *parent = nullptr;

    std::vector<Inst *> inputs;
    std::vector<Inst *> users;

    bool isTerminator() const {
        return isCondBr || isUncondBr || isRet;
    }
};

struct Block {
    int startPC = -1;
    int endPC = -1;

    Inst *head = nullptr;
    Inst *tail = nullptr;

    std::vector<int> predecessors, successors;

    std::map<std::string, std::vector<std::string>> phiArgs;
    std::map<std::string, std::string> phiDef;

    void append(Inst *I) {
        I->parent = this;
        I->prev = tail;
        I->next = nullptr;
        if (tail)
            tail->next = I;
        else
            head = I;
        tail = I;
    }
    void prepend(Inst *I) {
        I->parent = this;
        I->prev = nullptr;
        I->next = head;
        if (head)
            head->prev = I;
        else
            tail = I;
        head = I;
    }
};

struct Graph {
    std::vector<std::unique_ptr<Inst>> instructions;
    std::vector<Inst *> orderInstructions;
    std::vector<Block> blocks;
    std::unordered_map<int, int> startInstPC2blockIdx;
    int entry = -1;

    Inst *makeInst() {
        instructions.emplace_back(std::make_unique<Inst>());
        return instructions.back().get();
    }
};

static inline std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
static inline std::string lower(std::string s) {
    for (auto &c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}
static inline bool isRegister(const std::string &s) {
    return !s.empty() && s[0] == 'v';
}
