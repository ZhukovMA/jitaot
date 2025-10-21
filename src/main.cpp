#include "analysis.h"
#include "parser.h"
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;
    string path = argv[1];
    string err;
    auto parsed = parseByteCode(path, &err);
    if (!parsed) {
        cerr << err << "\n";
        return 1;
    }
    Graph P = std::move(*parsed);
    buildBlocks(P);

    DomInfo D = computeDominators(P);
    printDominatorTree(P, D);

    if (!runUnitTests(P, D, &err)) {
        cerr << "TEST FAILED: " << err << "\n";
        return 2;
    }
    cout << "TEST OK\n";

    placePhi(P, D);
    int nextId = 0;
    rename2SSA(P, D, P.entry, nextId, {});
    phi2Instructions(P);
    buildUseDefLinks(P);
    dumpIR(P);

    return 0;
}