#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ir {
class Inst; 
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


using Value = std::variant<SSAValue *, std::string, uint64_t>;
} 