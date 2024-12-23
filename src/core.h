#ifndef EVE_H
#define EVE_H
#include <array>
#include <cstdint>
#include <iss/arch_if.h>
#include <iss/vm_types.h>

using namespace iss;
struct eve_core : public arch_if {

#pragma pack(push, 1)
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
#pragma pack(pop)

  std::array<uint8_t, 0xfff> Imem;
  std::array<uint8_t, 0xffff> Dmem;

  eve_core();
  ~eve_core();
  void reset(uint64_t address = 0) override;
  uint8_t *get_regs_base_ptr() override;
  std::pair<uint64_t, bool> load_file(std::string name, int type = -1) override;
  status read(const address_type type, const access_type access,
              const uint32_t space, const uint64_t addr, const unsigned length,
              uint8_t *const data) override;
  status write(const address_type type, const access_type access,
               const uint32_t space, const uint64_t addr, const unsigned length,
               const uint8_t *const data) override;
};
#endif