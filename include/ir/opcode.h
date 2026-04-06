#pragma once

namespace ir {
enum class Opcode {
    MOVI_U64,
    PARAMETER,
    U32TOU64,
    CMP_U64,
    JA_U64,
    MUL_U64,
    ADDI_U64,
    CALL_STATIC_U64,
    JMP,
    RET_U64,
    PHI_U64,
    AND_U64,
    SHL_U64,
    MOVE_U64,
    SPILL_U64,
    FILL_U64
};
}