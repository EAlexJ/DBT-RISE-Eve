
#include <array>
#include <cassert>
#include <core.h>
#include <cstdint>
#include <instrs.h>
#include <iostream>
#include <iss/interp/vm_base.h>
#include <iss/vm_if.h>
#include <iss/vm_types.h>
#include <util/logging.h>
#include <vm.h>

using namespace iss;
using op = arch::traits<eve_core>::opcode_e;
using super = typename iss::interp::vm_base<eve_core>;
using traits = arch::traits<eve_core>;

struct memory_access_exception : public std::exception {
  memory_access_exception() {}
};

void print_eve_out(int port, int val) {
  CPPLOG(INFO) << "I/O Port " << std::hex << "0x" << port << std::dec
               << " sent: " << val;
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

eve_vm::virt_addr_t eve_vm::execute_inst(finish_cond_e cond, virt_addr_t start,
                                         uint64_t icount_limit) {
  auto &instr_count = get_reg<uint64_t>(reg_e::ICOUNT);
  auto *PC = &get_reg(reg_e::PC);
  auto *NEXT_PC = &get_reg(reg_e::NEXT_PC);
  while (instr_count < icount_limit) {
    uint32_t instr_index = op::ILLEGAL;
    uint32_t instr{0};
    for (size_t i = 0; i < 3; ++i) {
      uint8_t cur_read;
      auto read_succ = core.read(address_type::PHYSICAL, access_type::FETCH, 0,
                                 *PC + i, 1, &cur_read);
      instr |= cur_read;
      instr_index = local_decoder.decode_instr(instr);
      if (instr_index == std::numeric_limits<uint32_t>::max())
        instr = instr << 8;
      else
        break;
    }
    switch (instr_index) {
        case op::MOV: {
            uint8_t regD = ((bit_sub<0,3>(instr)));
            uint8_t regS = ((bit_sub<3,3>(instr)));
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "mov";
                this->core.disass_output(*PC, mnemonic);
            }
            
            uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]);
            *NEXT_PC = *PC + 1;
            
            {
                            *(REG+regD) = *(REG+regS);
                        }
            break;
        }
        case op::LD: {
            uint16_t imm = ((bit_sub<0,16>(instr)));
            uint8_t reg = ((bit_sub<16,3>(instr)));
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "ld";
                this->core.disass_output(*PC, mnemonic);
            }
            
            uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]);
            *NEXT_PC = *PC + 3;
            
            {
                uint8_t res_3 = super::template read_mem<uint8_t>(traits::DMEM, imm);
                if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                *(REG+reg) = res_3;
            }
            break;
        }
        case op::ST: {
            uint16_t imm = ((bit_sub<0,16>(instr)));
            uint8_t reg = ((bit_sub<16,3>(instr)));
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "st";
                this->core.disass_output(*PC, mnemonic);
            }
            
            uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]);
            *NEXT_PC = *PC + 3;
            
            {
                super::template write_mem<uint8_t>(traits::DMEM, imm, *(REG+reg));
                if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
            }
            break;
        }
        case op::MOVI: {
            uint8_t imm = ((bit_sub<0,8>(instr)));
            uint8_t reg = ((bit_sub<8,3>(instr)));
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "movi";
                this->core.disass_output(*PC, mnemonic);
            }
            
            uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]);
            *NEXT_PC = *PC + 2;
            
            {
                *(REG+reg) = imm;
            }
            break;
        }
        case op::ADD: {
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "add";
                this->core.disass_output(*PC, mnemonic);
            }
             
            uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
            uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]);
            uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]); 
            uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
            uint8_t* SN = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SN]); 
            uint8_t* ZE = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::ZE]); 
            uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
            *NEXT_PC = *PC + 1;
            
            {
                uint16_t res = (uint16_t)(*A ) + (uint16_t)(*B );
                uint16_t my_res = (uint16_t)(*(REG+0) ) + (uint16_t)(*(REG+1) );
                *CY = res > 255;
                *SN = (res & (uint16_t)(128 )) != 0;
                *ZE = (res == 0);
                *OV = (int8_t)*A > 0 && (int8_t)*B > 0 && (int8_t)res < 0 || (int8_t)*A < 0 && (int8_t)*B < 0 && (int8_t)res > 0;
                *A = (uint8_t)res;
            }
            break;
        }
        case op::NOP: {
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "nop";
                this->core.disass_output(*PC, mnemonic);
            }
            
            *NEXT_PC = *PC + 1;
            
            {
            }
            break;
        }
        case op::GOTOXY: {
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "gotoxy";
                this->core.disass_output(*PC, mnemonic);
            }
             
            uint8_t* X = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::X]); 
            uint8_t* Y = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::Y]);
            *NEXT_PC = *PC + 1;
            
            {
                *NEXT_PC = ((uint16_t)*X << 8) | (uint16_t)(*Y );
                this->core.reg.last_branch = 1;
            }
            break;
        }
        case op::OUT: {
            uint8_t port = ((bit_sub<0,8>(instr)));
            if(this->disass_enabled){
                
                //No disass specified, using instruction name
                std::string mnemonic = "out";
                this->core.disass_output(*PC, mnemonic);
            }
             
            uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
            *NEXT_PC = *PC + 2;
            
            {
                print_eve_out(port, *A);
            }
            break;
        }
    default: {
      CPPLOG(ERR) << "Illegal Instruction, stopping";
      throw simulation_stopped(1);
      break;
    }
    }
    *PC = *NEXT_PC;
    instr_count++;
  }
  throw simulation_stopped(0);
};