#include "vm.h"
#include "core.h"
#include "eve.h"
#include "iss/vm_if.h"
#include "iss/vm_types.h"
#include "util/ities.h"
#include "util/logging.h"
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
  int instr_count = 0;
  while (instr_count < icount_limit) {
    uint8_t opcode = 0; // incase decoding goes wrong this defaults to nop
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
      bool CY = (static_cast<uint16_t>(core.reg.A) +
                 static_cast<uint16_t>(core.reg.B)) > 0xFF;
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
    case op::OUT: {
      uint8_t dest = 0;
      auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                                 core.reg.PC + 1, 1, &dest);
      assert(read_succ == iss::Ok);
      // Why does printing payload print an empty char if gdb says its a val?
      // Why do I need to cast it wider than the type actually is in order for it to work?
      auto payload = this->get_reg_val<reg_t>(dest);
      CPPLOG(INFO) << "OUT sent: " << (uint16_t)core.reg.A << " to I/O Port "
                   << std::hex << "0x" << (uint16_t)dest << std::dec << " "
                   << payload;
      core.reg.PC += 2;
      break;
    }
    /*
    case op::LD : {
        CPPLOG(INFO) << "Instruction not yet implemented";
        break;
      }
    case op::ST: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::PUSH: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::POP: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::CMP: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::BRANCH: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::CLR: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::CALL: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::RET: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
    case op::GOTOXY: {
      CPPLOG(INFO) << "Instruction not yet implemented";
      break;
    }
  */
    default: {
      CPPLOG(ERR) << "Unknown Instruction";
      throw simulation_stopped(1);
      break;
    }
    }
    instr_count++;
  }
  throw simulation_stopped(0);
};