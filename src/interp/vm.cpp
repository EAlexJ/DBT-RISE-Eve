
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

void eve_out(int port, int val) {
  CPPLOG(INFO) << "I/O Port " << std::hex << "0x" << port << std::dec
               << " sent: " << val;
}

uint8_t eve_in(){
    int temp;
    while (true) {
        CPPLOG(INFO) << "Enter a number (0-255) to send via I/O Port 0: ";
        std::cin >> temp;
        if (std::cin.eof()) {
            CPPLOG(ERR) << "\nEnd of input detected. Exiting...";
            std::exit(1);
        }
        if (std::cin.fail() || temp < 0 || temp > 255) {
            std::cin.clear(); // Clear error flag
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard invalid input
            CPPLOG(ERR) << "Invalid input. Please try again.";
        } else {
            return static_cast<std::uint8_t>(temp);
        }
    }
}

debugger::target_adapter_if *
eve_vm::accquire_target_adapter(debugger::server_if *srv) {
  return nullptr;
};

eve_vm::eve_vm()
    : iss::interp::vm_base<eve_core>(std::make_unique<eve_core>(), 0, 0),
      local_decoder([&]() {
              std::vector<generic_instruction_descriptor> generic_vector;
              std::transform(
                  eve_instr_info.begin(), eve_instr_info.end(),
                  std::back_inserter(generic_vector),
                  [](const eve_instr_descriptor& eve_info) -> generic_instruction_descriptor {
                      return {eve_info.encoding, eve_info.mask, eve_info.index};
                  }
              );
              return generic_vector;
          }()) {}

eve_vm::virt_addr_t eve_vm::execute_inst(finish_cond_e cond, virt_addr_t start, uint64_t icount_limit) {
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
            //the check for the size works since the order of the index in instrs.h and opcode_e in core.h is guaranteed by CoreDSL generator
            if (instr_index == iss::DECODING_FAIL || (i+1) != eve_instr_info[instr_index].size) 
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
                                uint8_t res_13 = super::template read_mem<uint8_t>(traits::DMEM, imm);
                                if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                                *(REG+reg) = res_13;
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
            case op::PUSH: {
                uint8_t reg = ((bit_sub<0,3>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "push";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint16_t* SP = reinterpret_cast<uint16_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SP]);
                uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]);
                *NEXT_PC = *PC + 1;
                
                {
                    *SP = (uint16_t)((uint32_t)(*SP ) - (uint32_t)(1 ));
                    super::template write_mem<uint8_t>(traits::DMEM, *SP, *(REG+reg));
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                }
                break;
            }
            case op::POP: {
                uint8_t reg = ((bit_sub<0,3>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "pop";
                    this->core.disass_output(*PC, mnemonic);
                }
                
                uint8_t* REG = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::REG0]); 
                uint16_t* SP = reinterpret_cast<uint16_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SP]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res_14 = super::template read_mem<uint8_t>(traits::DMEM, *SP);
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    *(REG+reg) = res_14;
                    *SP = (uint16_t)((uint32_t)(*SP ) + (uint32_t)(1 ));
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
            case op::ADD16: {
                uint16_t imm = ((bit_sub<0,16>(instr)));
                uint8_t regD = ((bit_sub<16,1>(instr)));
                uint8_t regS = ((bit_sub<17,2>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "add16";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* X = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::X]); 
                uint8_t* Y = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::Y]); 
                uint8_t* M1 = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::M1]); 
                uint8_t* M2 = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::M2]); 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
                *NEXT_PC = *PC + 3;
                
                {
                    uint16_t res = imm;
                    if(regS == 0) {
                        res = (uint16_t)((uint32_t)(imm ) + (uint32_t)(((*X<<8)|*Y) ));
                    }
                    else {
                        if(regS == 1) {
                            res = (uint16_t)((uint32_t)(imm ) + (uint32_t)(((*M1<<8)|*M2) ));
                        }
                        else {
                            if(regS == 3) {
                                res = (uint16_t)((uint32_t)(imm ) + (uint32_t)(*A ));
                            }
                        }
                    }
                    if(regD == 0) {
                        *X = bit_sub<8, 15-8+1>(res);
                        *Y = bit_sub<0, 7-0+1>(res);
                    }
                    else {
                        if(regD == 1) {
                            *M1 = bit_sub<8, 15-8+1>(res);
                            *M2 = bit_sub<0, 7-0+1>(res);
                        }
                    }
                }
                break;
            }
            case op::AND: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "and";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res = *A & *B;
                    set_SN_ZE(res);
                    *CY = 0;
                    *OV = 0;
                    *A = res;
                }
                break;
            }
            case op::OR: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "or";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res = *A | *B;
                    set_SN_ZE(res);
                    *CY = 0;
                    *OV = 0;
                    *A = res;
                }
                break;
            }
            case op::XOR: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "xor";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res = *A ^ *B;
                    set_SN_ZE(res);
                    *CY = 0;
                    *OV = 0;
                    *A = res;
                }
                break;
            }
            case op::NOT: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "not";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res = ! *A;
                    set_SN_ZE(res);
                    *CY = 0;
                    *OV = 0;
                    *A = res;
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
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint16_t res = (uint16_t)(*A ) + (uint16_t)(*B );
                    set_SN_ZE((uint8_t)res);
                    *CY = res > 255;
                    *OV = (int8_t)*A > 0 && (int8_t)*B > 0 && (int8_t)res < 0 || (int8_t)*A < 0 && (int8_t)*B < 0 && (int8_t)res > 0;
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::ADDC: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "addc";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint16_t res = (uint16_t)(*A ) + (uint16_t)(*B ) + (uint16_t)(*CY );
                    set_SN_ZE((uint8_t)res);
                    *CY = res > 255;
                    *OV = (int8_t)*A > 0 && (int8_t)*B > 0 && (int8_t)res < 0 || (int8_t)*A < 0 && (int8_t)*B < 0 && (int8_t)res > 0;
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::SUB: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "sub";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    int16_t res = (uint16_t)(*A ) - (uint16_t)(*B );
                    set_SN_ZE((uint8_t)res);
                    *CY = res < 0;
                    *OV = (((*A ^ *B) & 128) && ((*A ^ res) & (int16_t)(128 )));
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::SUBC: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "subc";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    int16_t res = (uint16_t)(*A ) - (uint16_t)(*B ) - (int16_t)(*CY );
                    set_SN_ZE((uint8_t)res);
                    *CY = res < 0;
                    *OV = (((*A ^ *B) & 128) && ((*A ^ res) & (int16_t)(128 )));
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::NEG: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "neg";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    int16_t res = - *A;
                    set_SN_ZE((uint8_t)res);
                    *CY = 0;
                    *OV = (uint8_t)(*A & 128);
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::CLR: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "clr";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* SN = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SN]); 
                uint8_t* ZE = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::ZE]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]); 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
                *NEXT_PC = *PC + 1;
                
                {
                    *SN = 0;
                    *ZE = 1;
                    *CY = 0;
                    *OV = 0;
                    *A = 0;
                }
                break;
            }
            case op::SHL: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "shl";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint16_t res = *A << 1;
                    set_SN_ZE((uint8_t)res);
                    *CY = (uint8_t)(res & 256);
                    *OV = 0;
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::SHRC: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "shrc";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint16_t res = ((*CY<<8)|*A) >> 1;
                    set_SN_ZE((uint8_t)res);
                    *CY = (uint8_t)*A;
                    *OV = 0;
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::SHR: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "shr";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* SN = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SN]); 
                uint8_t* ZE = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::ZE]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res = *A >> 1;
                    *SN = 0;
                    *ZE = res == 0;
                    *CY = (uint8_t)*A;
                    *OV = 0;
                    *A = res;
                }
                break;
            }
            case op::INCR: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "incr";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint16_t res = (uint16_t)(*A ) + (uint16_t)(1 );
                    set_SN_ZE((uint8_t)res);
                    *CY = 0;
                    *OV = 0;
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::DEC: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "dec";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    int16_t res = (uint16_t)(*A ) - (uint16_t)(1 );
                    set_SN_ZE((uint8_t)res);
                    *CY = 0;
                    *OV = 0;
                    *A = (uint8_t)res;
                }
                break;
            }
            case op::LDSP: {
                uint16_t imm = ((bit_sub<0,16>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "ldsp";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint16_t* SP = reinterpret_cast<uint16_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SP]);
                *NEXT_PC = *PC + 3;
                
                {
                                *SP = imm;
                            }
                break;
            }
            case op::LDCODE: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "ldcode";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* X = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::X]); 
                uint8_t* Y = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::Y]); 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint16_t read_addr = (*X<<8)|*Y;
                    uint8_t res_15 = super::template read_mem<uint8_t>(traits::IMEM, read_addr);
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    *A = res_15;
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
                                *NEXT_PC = (*X<<8)|*Y;
                                this->core.reg.last_branch = 1;
                            }
                break;
            }
            case op::CALL: {
                uint16_t addr = ((bit_sub<0,16>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "call";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint16_t* SP = reinterpret_cast<uint16_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SP]);
                *NEXT_PC = *PC + 3;
                
                {
                    uint16_t ret_addr = (uint16_t)((uint32_t)(*PC ) + (uint32_t)(1 ));
                    *SP = (uint16_t)((uint32_t)(*SP ) - (uint32_t)(2 ));
                    super::template write_mem<uint8_t>(traits::DMEM, *SP, (uint8_t)(ret_addr >> 8));
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    super::template write_mem<uint8_t>(traits::DMEM, (uint32_t)(*SP ) + (uint32_t)(1 ), (uint8_t)ret_addr);
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    *NEXT_PC = addr;
                    this->core.reg.last_branch = 1;
                }
                break;
            }
            case op::RET: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "ret";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint16_t* SP = reinterpret_cast<uint16_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SP]);
                *NEXT_PC = *PC + 1;
                
                {
                    uint8_t res_16 = super::template read_mem<uint8_t>(traits::DMEM, *SP);
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    uint8_t msb_addr = res_16;
                    uint8_t res_17 = super::template read_mem<uint8_t>(traits::DMEM, (uint32_t)(*SP ) + (uint32_t)(1 ));
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    uint8_t lsb_addr = res_17;
                    *SP = (uint16_t)((uint32_t)(*SP ) + (uint32_t)(2 ));
                    *NEXT_PC = (msb_addr<<8)|lsb_addr;
                    this->core.reg.last_branch = 1;
                }
                break;
            }
            case op::IN: {
                uint8_t port = ((bit_sub<0,8>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "in";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
                *NEXT_PC = *PC + 2;
                
                {
                                *A = eve_in();
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
                                eve_out(port, *A);
                            }
                break;
            }
            case op::LDM: {
                uint16_t imm = ((bit_sub<0,16>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "ldm";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* M1 = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::M1]); 
                uint8_t* M2 = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::M2]); 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
                *NEXT_PC = *PC + 3;
                
                {
                    uint16_t read_addr = (uint16_t)((uint32_t)(((*M1<<8)|*M2) ) + (uint32_t)(imm ));
                    uint8_t res_18 = super::template read_mem<uint8_t>(traits::DMEM, read_addr);
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                    *A = res_18;
                }
                break;
            }
            case op::STM: {
                uint16_t imm = ((bit_sub<0,16>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "stm";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* M1 = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::M1]); 
                uint8_t* M2 = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::M2]); 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]);
                *NEXT_PC = *PC + 3;
                
                {
                    uint16_t write_addr = (uint16_t)((uint32_t)(((*M1<<8)|*M2) ) + (uint32_t)(imm ));
                    super::template write_mem<uint8_t>(traits::DMEM, write_addr, *A);
                    if(this->core.reg.trap_state>=0x80000000UL) throw memory_access_exception();
                }
                break;
            }
            case op::CMP: {
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "cmp";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* A = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::A]); 
                uint8_t* B = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::B]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]);
                *NEXT_PC = *PC + 1;
                
                {
                    int16_t res = (uint16_t)(*A ) - (uint16_t)(*B );
                    set_SN_ZE((uint8_t)res);
                    *CY = res < 0;
                    *OV = (((*A ^ *B) & 128) && ((*A ^ res) & (int16_t)(128 )));
                }
                break;
            }
            case op::BRANCH: {
                uint16_t addr = ((bit_sub<0,16>(instr)));
                uint8_t cond = ((bit_sub<16,4>(instr)));
                if(this->disass_enabled){
                    
                    //No disass specified, using instruction name
                    std::string mnemonic = "branch";
                    this->core.disass_output(*PC, mnemonic);
                }
                 
                uint8_t* ZE = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::ZE]); 
                uint8_t* SN = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::SN]); 
                uint8_t* OV = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::OV]); 
                uint8_t* CY = reinterpret_cast<uint8_t*>(this->regs_base_ptr+arch::traits<eve_core>::reg_byte_offsets[arch::traits<eve_core>::CY]);
                *NEXT_PC = *PC + 3;
                
                {
                    if(cond == 0) {
                        if(*ZE) {
                            *NEXT_PC = addr;
                            this->core.reg.last_branch = 1;
                        }
                    }
                    else {
                        if(cond == 1) {
                            if(! *ZE) {
                                *NEXT_PC = addr;
                                this->core.reg.last_branch = 1;
                            }
                        }
                        else {
                            if(cond == 2) {
                                if(*SN ^ *OV) {
                                    *NEXT_PC = addr;
                                    this->core.reg.last_branch = 1;
                                }
                            }
                            else {
                                if(cond == 3) {
                                    if((*SN ^ *OV) | *ZE) {
                                        *NEXT_PC = addr;
                                        this->core.reg.last_branch = 1;
                                    }
                                }
                                else {
                                    if(cond == 4) {
                                        if((*SN == *OV) & ! *ZE) {
                                            *NEXT_PC = addr;
                                            this->core.reg.last_branch = 1;
                                        }
                                    }
                                    else {
                                        if(cond == 5) {
                                            if(*SN == *OV) {
                                                *NEXT_PC = addr;
                                                this->core.reg.last_branch = 1;
                                            }
                                        }
                                        else {
                                            if(cond == 6) {
                                                if(*CY) {
                                                    *NEXT_PC = addr;
                                                    this->core.reg.last_branch = 1;
                                                }
                                            }
                                            else {
                                                if(cond == 7) {
                                                    if(*CY | *ZE) {
                                                        *NEXT_PC = addr;
                                                        this->core.reg.last_branch = 1;
                                                    }
                                                }
                                                else {
                                                    if(cond == 8) {
                                                        if(! (*CY | *ZE)) {
                                                            *NEXT_PC = addr;
                                                            this->core.reg.last_branch = 1;
                                                        }
                                                    }
                                                    else {
                                                        if(cond == 9) {
                                                            if(! *CY) {
                                                                *NEXT_PC = addr;
                                                                this->core.reg.last_branch = 1;
                                                            }
                                                        }
                                                        else {
                                                            if(cond == 10) {
                                                                if(! *SN) {
                                                                    *NEXT_PC = addr;
                                                                    this->core.reg.last_branch = 1;
                                                                }
                                                            }
                                                            else {
                                                                if(cond == 11) {
                                                                    if(*SN) {
                                                                        *NEXT_PC = addr;
                                                                        this->core.reg.last_branch = 1;
                                                                    }
                                                                }
                                                                else {
                                                                    if(cond == 12) {
                                                                        if(! *OV) {
                                                                            *NEXT_PC = addr;
                                                                            this->core.reg.last_branch = 1;
                                                                        }
                                                                    }
                                                                    else {
                                                                        if(cond == 13) {
                                                                            if(*OV) {
                                                                                *NEXT_PC = addr;
                                                                                this->core.reg.last_branch = 1;
                                                                            }
                                                                        }
                                                                        else {
                                                                            if(cond == 14) {
                                                                                *NEXT_PC = addr;
                                                                                this->core.reg.last_branch = 1;
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
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