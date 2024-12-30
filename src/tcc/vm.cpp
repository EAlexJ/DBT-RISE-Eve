#include "core.h"
#include "iss/tcc/vm_base.h"
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vm.h>

using namespace iss::tcc;
extern "C" {
void print_eve_out(void *iface, int port, int val) {
  CPPLOG(INFO) << "[EXEC] I/O Port " << port << " sent: " << val;
}
}
debugger::target_adapter_if *
eve_vm::accquire_target_adapter(debugger::server_if *srv) {
  return nullptr;
};
eve_vm::eve_vm()
    : vm_base<eve_core>(std::make_unique<eve_core>(), 0, 0),
      local_decoder([]() {
        return std::vector<generic_instruction_descriptor>(
            eve_instr_info.begin(), eve_instr_info.end());
      }()){};
void eve_vm::add_prologue(tu_builder &tu) {
  std::ostringstream os;
  os << "void (*print_eve_out)(void*, int, int)=" << (uintptr_t)&print_eve_out
     << ";\n";
  tu.add_prologue(os.str());
}
continuation_e eve_vm::gen_single_inst_behavior(virt_addr_t &pc_v,
                                                tu_builder &tu) {
  uint8_t opcode;
  auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH,
                             mem_type_e::IMEM, pc_v.val, 1, &opcode);
  assert(read_succ == iss::Ok);
  auto instr_index = local_decoder.decode_instr(opcode);
  auto return_val = continuation_e::ILLEGAL_INSTR;
  switch (instr_index) {
  case op::MOV: {
    uint8_t regS = bit_sub<3, 3>(opcode);
    uint8_t regD = bit_sub<0, 3>(opcode);
    tu("//MOV");
    tu.store(regD,
             tu.ext((tu.add(tu.load(regD, 0), tu.load(regS, 0))), 32, false));
    pc_v = pc_v + 1;
    return_val = continuation_e::CONT;
    break;
  }
  case op::LD: {
    tu("//LD");
    std::array<uint8_t, 2> msb_lsb;
    auto read_succ =
        core.read(address_type::PHYSICAL, access_type::FETCH, mem_type_e::IMEM,
                  pc_v.val + 1, 2, msb_lsb.data());
    assert(read_succ == iss::Ok);
    uint8_t regD = bit_sub<0, 3>(opcode);
    tu.store(regD, tu.read_mem(mem_type_e::DMEM,
                               (msb_lsb.at(0) << 8) | msb_lsb.at(1), 8));
    pc_v = pc_v + 3;
    return_val = continuation_e::CONT;
    break;
  }
  case op::ST: {
    tu("//ST");
    std::array<uint8_t, 2> msb_lsb;
    auto read_succ =
        core.read(address_type::PHYSICAL, access_type::FETCH, mem_type_e::IMEM,
                  pc_v.val + 1, 2, msb_lsb.data());
    assert(read_succ == iss::Ok);
    uint8_t regS = bit_sub<0, 3>(opcode);
    tu.write_mem(mem_type_e::DMEM, (msb_lsb.at(0) << 8) | msb_lsb.at(1),
                 tu.load(regS, 0));
    pc_v = pc_v + 3;
    return_val = continuation_e::CONT;
    break;
  }
  case op::MOVI: {
    tu("//MOVI");
    uint8_t imm = 0;
    auto read_succ =
        core.read(address_type::PHYSICAL, access_type::FETCH, mem_type_e::IMEM,
                  pc_v.val + 1, 1, reinterpret_cast<uint8_t *>(&imm));
    assert(read_succ == iss::Ok);
    uint8_t reg = bit_sub<0, 3>(opcode);
    tu.store(reg, imm);
    pc_v = pc_v + 2;
    return_val = continuation_e::CONT;
    break;
  }
  case op::ADD: {
    tu("//ADD");
    // uint16_t res =
    //     static_cast<uint16_t>(core.reg.A) + static_cast<uint16_t>(core.reg.B);
    // bool CY = res > 0xFF;
    // bool SN = (res & 0x80) != 0;
    // bool ZE = (res == 0);
    // bool OV =
    //     ((static_cast<int8_t>(core.reg.A) > 0 &&
    //       static_cast<int8_t>(core.reg.B) > 0 &&
    //       static_cast<int8_t>(res) < 0) ||
    //      (static_cast<int8_t>(core.reg.A) < 0 &&
    //       static_cast<int8_t>(core.reg.B) < 0 && static_cast<int8_t>(res) > 0));

    // core.reg.A = static_cast<uint8_t>(res);
    // core.reg.CY = CY;
    // core.reg.SN = SN;
    // core.reg.ZE = ZE;
    // core.reg.OV = OV;
    tu.store(reg_e::A, tu.add(tu.load(reg_e::A, 0), tu.load(reg_e::B, 0)));
    pc_v = pc_v + 1;
    return_val = continuation_e::CONT;
    break;
  }
  case op::NOP: {
    tu("//NOP");
    pc_v = pc_v + 1;
    return_val = continuation_e::CONT;
    break;
  }
  case op::GOTOXY: {
    tu("//GOTOXY");
    auto new_pc = tu.add(tu.shl(tu.load(reg_e::X, 0), tu.constant(8, 8)),
                         tu.load(reg_e::Y, 0));
    tu.open_if(tu.icmp(iss::tcc::ICmpInst::ICMP_NE, tu.constant(pc_v.val, 16),
                       new_pc));
    tu.store(reg_e::LAST_BRANCH,
             static_cast<uint8_t>(last_branch_e::UNKNOWN_JUMP));
    tu.open_else();
    tu.store(reg_e::LAST_BRANCH,
             static_cast<uint8_t>(last_branch_e::BRANCH_TO_SELF));
    tu.close_scope();
    tu.store(reg_e::NEXT_PC, new_pc);
    return_val = continuation_e::BRANCH;
    break;
  }
  case op::OUT: {
    tu("//OUT");
    uint8_t dest = 0;
    auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH,
                               mem_type_e::IMEM, pc_v.val + 1, 1, &dest);
    assert(read_succ == iss::Ok);
    tu("print_eve_out(core_ptr, {} , {});", dest, tu.load(reg_e::A, 0));
    pc_v = pc_v + 2;
    return_val = continuation_e::CONT;
    break;
  }
  default: {
    CPPLOG(ERR) << "Illegal Instruction found when jitting, exiting";
    this->core.exit_code = 1;
    return_val = continuation_e::ILLEGAL_INSTR;
    break;
  }
  }
  if (return_val == continuation_e::CONT)
    tu.store(reg_e::NEXT_PC, pc_v.val);
  increment_icount(tu);

  return return_val;
};

void eve_vm::increment_icount(code_builder<eve_core> &tu) {
  tu.store(reg_e::ICOUNT, tu.add(tu.load(reg_e::ICOUNT, 0), tu.constant(1, 8)));
};
