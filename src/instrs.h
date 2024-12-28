#ifndef INSTRS_H
#define INSTRS_H
#include "iss/arch/traits.h"
#include <core.h>
#include <cstdint>
#include <iss/instruction_decoder.h>

using op = arch::traits<eve_core>::opcode_e;
static constexpr std::array<generic_instruction_descriptor, 8> eve_instr_info{{

    // Missing Instructions are:
    // PUSH, POP, CMP, BRANCH, CLR, CAL, RET, LDM, STM, LDCODE, LDSP, AND, OR, XOR, NOT, ADDC, SUB, SUBC, NEG, CLR, SHL, SHLC, SHR, SHRC, INCR, DECR, ADD16, IN
    // value, mask, index
    {0b01000000, 0b11000000, op::MOV},
    {0b10000000, 0b11111000, op::LD},
    {0b10001000, 0b11111000, op::ST},
    {0b10100000, 0b11111000, op::MOVI},
    {0b10110100, 0b11111111, op::ADD},
    {0b11000110, 0b11111111, op::OUT},
    {0b00000000, 0b11111111, op::NOP},
    {0b11000010, 0b11111111, op::GOTOXY},
}};
#endif