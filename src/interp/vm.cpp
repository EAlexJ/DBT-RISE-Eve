#include "vm.h"
#include "core.h"
#include "iss/interp/vm_base.h"
#include "iss/vm_if.h"
#include "iss/vm_types.h"
#include "util/ities.h"
#include "util/logging.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <ratio>
#include <sys/types.h>

using op = arch::traits<eve_core>::opcode_e;

target_adapter_if *eve_vm::accquire_target_adapter(server_if *srv) {
  return nullptr;
};
eve_vm::eve_vm()
    : vm_base<eve_core>(std::make_unique<eve_core>(), 0, 0),
      local_decoder([]() {
        return std::vector<generic_instruction_descriptor>(
            eve_instr_info.begin(), eve_instr_info.end());
      }()){};

eve_vm::virt_addr_t eve_vm::execute_inst(finish_cond_e cond, virt_addr_t start,
                                         uint64_t icount_limit) {
  auto &instr_count = get_reg<uint64_t>(reg_e::ICOUNT);
  while (instr_count < icount_limit) {
    uint8_t opcode = op::ILLEGAL; // incase decoding goes wrong
    auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                               core.reg.PC, 1, &opcode);
    assert(read_succ == iss::Ok);
    auto instr_index = local_decoder.decode_instr(opcode);
    switch (instr_index) {
    case op::MOV: {
      uint8_t regS = bit_sub<3, 3>(opcode);
      uint8_t regD = bit_sub<0, 3>(opcode);
      this->get_reg(regD) = this->get_reg(regS);
      core.reg.PC += 1;
      break;
    }
    case op::LD: {
      std::array<uint8_t, 2> msb_lsb;
      auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                                 core.reg.PC + 1, 2, msb_lsb.data());
      assert(read_succ == iss::Ok);
      uint8_t regD = bit_sub<0, 3>(opcode);
      auto write_succ = core.read(address_type::PHYSICAL, access_type::READ, 0,
                                  (msb_lsb.at(0) << 8) | msb_lsb.at(1), 1,
                                  &(this->get_reg(regD)));
      assert(write_succ == iss::Ok);
      core.reg.PC += 3;
      break;
    }
    case op::ST: {
      std::array<uint8_t, 2> msb_lsb;
      auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                                 core.reg.PC + 1, 2, msb_lsb.data());
      assert(read_succ == iss::Ok);
      uint8_t regS = bit_sub<0, 3>(opcode);
      auto write_succ = core.write(address_type::PHYSICAL, access_type::READ, 0,
                                   (msb_lsb.at(0) << 8) | msb_lsb.at(1), 1,
                                   &(this->get_reg(regS)));
      assert(write_succ == iss::Ok);
      core.reg.PC += 3;
      break;
    }
    case op::MOVI: {
      uint8_t imm = 0;
      auto read_succ =
          core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                    core.reg.PC + 1, 1, reinterpret_cast<uint8_t *>(&imm));
      assert(read_succ == iss::Ok);
      uint8_t reg = bit_sub<0, 3>(opcode);
      this->get_reg(reg) = imm;
      core.reg.PC += 2;
      break;
    }
    case op::ADD: {
      uint16_t res =
          static_cast<uint16_t>(core.reg.A) + static_cast<uint16_t>(core.reg.B);
      bool CY = res > 0xFF;
      bool SN = (res & 0x80) != 0;
      bool ZE = (res == 0);
      bool OV = ((static_cast<int8_t>(core.reg.A) > 0 &&
                  static_cast<int8_t>(core.reg.B) > 0 &&
                  static_cast<int8_t>(res) < 0) ||
                 (static_cast<int8_t>(core.reg.A) < 0 &&
                  static_cast<int8_t>(core.reg.B) < 0 &&
                  static_cast<int8_t>(res) > 0));

      core.reg.A = static_cast<uint8_t>(res);
      core.reg.CY = CY;
      core.reg.SN = SN;
      core.reg.ZE = ZE;
      core.reg.OV = OV;
      core.reg.PC += 1;
      break;
    }
    case op::NOP: {
      core.reg.PC += 1;
      break;
    }
    case op::GOTOXY: {
      uint16_t new_pc = (core.reg.X << 8) + core.reg.Y;
      if (static_cast<bool>(cond & finish_cond_e::JUMP_TO_SELF) &&
          new_pc == core.reg.PC) {
        CPPLOG(INFO) << "Jump to self, exiting";
        throw simulation_stopped(0);
      }
      core.reg.PC = new_pc;
      break;
    }
    case op::OUT: {
      uint8_t dest = 0;
      auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                                 core.reg.PC + 1, 1, &dest);
      assert(read_succ == iss::Ok);
      CPPLOG(INFO) << "I/O Port " << std::hex << "0x" << (unsigned)dest
                   << std::dec << " sent: " << (unsigned)core.reg.A;
      core.reg.PC += 2;
      break;
    }
    default: {
      CPPLOG(ERR) << "Illegal Instruction, stopping";
      throw simulation_stopped(1);
      break;
    }
    }
    instr_count++;
  }
  throw simulation_stopped(0);
};