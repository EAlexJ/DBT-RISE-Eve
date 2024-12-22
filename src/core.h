#ifndef EVE_H
#define EVE_H
#include <cstdint>
#include <iss/arch_if.h>

struct eve_core : public iss::arch_if {
  struct eve_regs {
    uint8_t A = 0;
    uint8_t B = 0;
    uint8_t C = 0;
    uint8_t D = 0;
    uint8_t X = 0;
    uint8_t Y = 0;
    uint8_t M1 = 0;
    uint8_t M2 = 0;
    uint16_t PC = 0;
    uint16_t SP = 0;
    bool CY = 0;
    bool SN = 0;
    bool ZE = 0;
    bool OV = 0;
  } reg;
};
#endif