#pragma once
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