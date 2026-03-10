
namespace analysis {

struct AllocPos {
    enum class Kind {
        NONE,
        REGISTER,
        STACK
    };

    Kind kind{Kind::NONE};
    ir::RegClass reg_class{ir::RegClass::INT};
    int index{-1};

    std::string toString() const {
        if (kind == Kind::REGISTER) {
            if (reg_class == ir::RegClass::FLOAT) {
                return "f" + std::to_string(index);
            }
            return "r" + std::to_string(index);
        }
        if (kind == Kind::STACK) {
            return "s" + std::to_string(index);
        }
        return "none";
    }

    bool operator==(const AllocPos &other) const {
        return kind == other.kind && reg_class == other.reg_class && index == other.index;
    }

    bool operator!=(const AllocPos &other) const {
        return !(*this == other);
    }
};

struct EdgeMoveAction {
    ir::BasicBlock *pred{nullptr};
    ir::BasicBlock *succ{nullptr};
    ir::SSAValue *value{nullptr};
    AllocPos from;
    AllocPos to;
};

}