#ifndef EVE_CORE_H
#define EVE_CORE_H

#include <array>
#include <cstdint>
#include <iss/arch/traits.h>
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

  // core definitions needed to instantiate the vm
  iss::sync_type needed_sync() const { return iss::NO_SYNC; }
};

// the traits struct is needed to instantiate the vm
template <> struct arch::traits<eve_core> {
  enum reg_e { A, B, C, D, X, Y, M1, M2, PC, SP, CY, SN, ZE, OV };
  enum opcode_e {
    MOV,
    MOVI,
    LD,
    ST,
    LDM,
    STM,
    LDCODE,
    LDSP,
    AND,
    OR,
    XOR,
    NOT,
    ADD,
    ADDC,
    SUB,
    SUBC,
    NEG,
    CLR,
    SHL,
    SHLC,
    SHR,
    SHRC,
    INCR,
    DECR,
    CMP,
    ADD16,
    CALL,
    RET,
    PUSH,
    POP,
    BRANCH,
    GOTOXY,
    NOP,
    IN,
    OUT,
    ILLEGAL
  };
  using reg_t = uint8_t;
  using addr_t = uint16_t;
  using code_word_t = uint32_t;
  using virt_addr_t = typed_addr_t<address_type::PHYSICAL>;
  using phys_addr_t = typed_addr_t<address_type::PHYSICAL>;
  static constexpr std::array<uint32_t, 14> reg_bit_widths{
      8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 1, 1, 1, 1};
  static constexpr std::array<uint32_t, 14> reg_byte_offsets{
      0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 12, 13, 14, 15};
  enum sreg_flag_e { FLAGS };
  enum mem_type_e { MEM, IMEM };
};

#endif