#ifndef INSTRS_H
#define INSTRS_H
#include <iss/arch/traits.h>
#include <core.h>
#include <iss/instruction_decoder.h>

using op = iss::arch::traits<eve_core>::opcode_e;
static constexpr std::array<iss::generic_instruction_descriptor, 8> eve_instr_info{{
        
        {0b01000000, 0b11000000, op::MOV},
        {0b100000000000000000000000, 0b111110000000000000000000, op::LD},
        {0b100010000000000000000000, 0b111110000000000000000000, op::ST},
        {0b1010000000000000, 0b1111100000000000, op::MOVI},
        {0b10110100, 0b11111111, op::ADD},
        {0b00000000, 0b11111111, op::NOP},
        {0b11000010, 0b11111111, op::GOTOXY},
        {0b1100011000000000, 0b1111111100000000, op::OUT},
    }};
#endif
