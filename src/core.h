
#ifndef EVE_CORE_H
#define EVE_CORE_H

#include <array>
#include <cstdint>
#include <iss/arch/traits.h>
#include <iss/arch_if.h>
#include <iss/vm_types.h>

using namespace iss;
struct eve_core;
template <> struct arch::traits<eve_core> {
  enum reg_e {
    REG0, REG1, REG2, REG3, REG4, REG5, REG6, REG7, PC, NEXT_PC, SP, FLAGS0, FLAGS1, FLAGS2, FLAGS3,
    ICOUNT,
    last_branch,
    trap_state,
    NUM_REGS,
    A = REG0, B = REG1, C = REG2, D = REG3, X = REG4, Y = REG5, M1 = REG6, M2 = REG7, CY = FLAGS0, SN = FLAGS1, ZE = FLAGS2, OV = FLAGS3
  };
  enum opcode_e {
    MOV = 0,
    LD = 1,
    ST = 2,
    MOVI = 3,
    ADD = 4,
    NOP = 5,
    GOTOXY = 6,
    OUT = 7,
    ILLEGAL
  };
    using reg_t = uint8_t;
    using addr_t = uint16_t;
    using code_word_t = uint32_t;
    using virt_addr_t = typed_addr_t<address_type::PHYSICAL>;
    using phys_addr_t = typed_addr_t<address_type::PHYSICAL>;
    static constexpr std::array<const uint32_t, 18> reg_bit_widths{
        {8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 8, 8, 8, 8, 64, 32, 32}};
    static constexpr std::array<const uint32_t, 18> reg_byte_offsets{
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 15, 16, 17, 18, 26, 30}};
    enum sreg_flag_e { FLAGS };
        enum mem_type_e { IMEM, DMEM, MEM = IMEM};
};

struct eve_core : public arch_if {
#pragma pack(push, 1)
  struct eve_regs { 
    uint8_t REG0 = 0; 
    uint8_t REG1 = 0; 
    uint8_t REG2 = 0; 
    uint8_t REG3 = 0; 
    uint8_t REG4 = 0; 
    uint8_t REG5 = 0; 
    uint8_t REG6 = 0; 
    uint8_t REG7 = 0; 
    uint16_t PC = 0; 
    uint16_t NEXT_PC = 0; 
    uint16_t SP = 0; 
    uint8_t FLAGS0 = 0; 
    uint8_t FLAGS1 = 0; 
    uint8_t FLAGS2 = 0; 
    uint8_t FLAGS3 = 0;
    uint64_t ICOUNT;
    uint8_t last_branch;
    uint32_t trap_state;
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

  iss::sync_type needed_sync() const { return iss::NO_SYNC; }
  inline bool should_stop() { return exit_code; };
  inline uint64_t stop_code() { return exit_code; };
  uint64_t exit_code = 0;
  iss::arch::traits<eve_core>::phys_addr_t virt2phys(const iss::addr_t &addr);
};
#endif