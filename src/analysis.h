#pragma once
#include "ir.h"
#include <string>
#include <unordered_set>
#include <vector>

struct DomInfo {
    std::vector<std::unordered_set<int>> dom;
    std::vector<int> idom;
    std::vector<std::vector<int>> kids;
    std::vector<std::unordered_set<int>> DF;
};

DomInfo computeDominators(const Graph &P);
void placePhi(Graph &P, const DomInfo &D);
struct RS {
    std::unordered_map<std::string, std::vector<std::string>> st;
    int nextId = 0;
    std::string toSSAName(const std::string &b);
    std::string top(const std::string &b);
    void push(const std::string &b, const std::string &n);
    void pop(const std::string &b);
};
void rename2SSA(Graph &P, const DomInfo &D, int b, int &nextId, std::unordered_map<std::string, std::string> cur);
void phi2Instructions(Graph &P);
void buildUseDefLinks(Graph &P);
void dumpIR(const Graph &P);

bool runUnitTests(const Graph &P, const DomInfo &D, std::string *outErr);
void printDominatorTree(const Graph &P, const DomInfo &D);
