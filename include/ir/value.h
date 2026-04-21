#include <algorithm>
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ir {
class Inst; 

enum class RegClass {
    INT,
    FLOAT
};

struct SSAValue {
    uint32_t id{};
    Inst *def{nullptr};
    std::vector<Inst *> users;
    bool is_arg{false};
    std::string dbg_name;
    RegClass reg_class{RegClass::INT};

    void addUser(Inst *I) {
        users.push_back(I);
    }

    void removeUser(Inst *I) {
        users.erase(std::remove(users.begin(), users.end(), I), users.end());
    }
};


using Value = std::variant<SSAValue *, std::string, uint64_t>;
} 