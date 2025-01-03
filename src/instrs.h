#ifndef INSTRS_H
#define INSTRS_H
#include <iss/arch/traits.h>
#include <core.h>
#include <iss/instruction_decoder.h>

constexpr std::array<iss::generic_instruction_descriptor, 34> eve_instr_info{{
        
        {0b01000000, 0b11000000, iss::arch::traits<eve_core>::opcode_e::MOV},
        {0b100000000000000000000000, 0b111110000000000000000000, iss::arch::traits<eve_core>::opcode_e::LD},
        {0b100010000000000000000000, 0b111110000000000000000000, iss::arch::traits<eve_core>::opcode_e::ST},
        {0b10010000, 0b11111000, iss::arch::traits<eve_core>::opcode_e::PUSH},
        {0b10011000, 0b11111000, iss::arch::traits<eve_core>::opcode_e::POP},
        {0b1010000000000000, 0b1111100000000000, iss::arch::traits<eve_core>::opcode_e::MOVI},
        {0b101010000000000000000000, 0b111110000000000000000000, iss::arch::traits<eve_core>::opcode_e::ADD16},
        {0b10110000, 0b11111111, iss::arch::traits<eve_core>::opcode_e::AND},
        {0b10110001, 0b11111111, iss::arch::traits<eve_core>::opcode_e::OR},
        {0b10110010, 0b11111111, iss::arch::traits<eve_core>::opcode_e::XOR},
        {0b10110011, 0b11111111, iss::arch::traits<eve_core>::opcode_e::NOT},
        {0b10110100, 0b11111111, iss::arch::traits<eve_core>::opcode_e::ADD},
        {0b10110101, 0b11111111, iss::arch::traits<eve_core>::opcode_e::ADDC},
        {0b10110110, 0b11111111, iss::arch::traits<eve_core>::opcode_e::SUB},
        {0b10110111, 0b11111111, iss::arch::traits<eve_core>::opcode_e::SUBC},
        {0b10111000, 0b11111111, iss::arch::traits<eve_core>::opcode_e::NEG},
        {0b10111001, 0b11111111, iss::arch::traits<eve_core>::opcode_e::CLR},
        {0b10111010, 0b11111111, iss::arch::traits<eve_core>::opcode_e::SHL},
        {0b10111011, 0b11111111, iss::arch::traits<eve_core>::opcode_e::SHRC},
        {0b10111101, 0b11111111, iss::arch::traits<eve_core>::opcode_e::SHR},
        {0b10111110, 0b11111111, iss::arch::traits<eve_core>::opcode_e::INCR},
        {0b10111111, 0b11111111, iss::arch::traits<eve_core>::opcode_e::DEC},
        {0b110000000000000000000000, 0b111111110000000000000000, iss::arch::traits<eve_core>::opcode_e::LDSP},
        {0b11000001, 0b11111111, iss::arch::traits<eve_core>::opcode_e::LDCODE},
        {0b11000010, 0b11111111, iss::arch::traits<eve_core>::opcode_e::GOTOXY},
        {0b110000110000000000000000, 0b111111110000000000000000, iss::arch::traits<eve_core>::opcode_e::CALL},
        {0b11000100, 0b11111111, iss::arch::traits<eve_core>::opcode_e::RET},
        {0b1100010100000000, 0b1111111100000000, iss::arch::traits<eve_core>::opcode_e::IN},
        {0b1100011000000000, 0b1111111100000000, iss::arch::traits<eve_core>::opcode_e::OUT},
        {0b110001110000000000000000, 0b111111110000000000000000, iss::arch::traits<eve_core>::opcode_e::LDM},
        {0b110010000000000000000000, 0b111111110000000000000000, iss::arch::traits<eve_core>::opcode_e::STM},
        {0b11001001, 0b11111111, iss::arch::traits<eve_core>::opcode_e::CMP},
        {0b110100000000000000000000, 0b111100000000000000000000, iss::arch::traits<eve_core>::opcode_e::BRANCH},
        {0b00000000, 0b11111111, iss::arch::traits<eve_core>::opcode_e::NOP},
    }};
#endif
