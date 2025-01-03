#ifndef EVE_VM_H
#define EVE_VM_H

#include <core.h>
#include <iss/instruction_decoder.h>
#include <iss/interp/vm_base.h>

struct eve_vm : public iss::interp::vm_base<eve_core> {
    virt_addr_t execute_inst(iss::finish_cond_e cond, virt_addr_t start,
                            uint64_t icount_limit) override;
    iss::debugger::target_adapter_if *
    accquire_target_adapter(iss::debugger::server_if *srv) override;
    iss::decoder local_decoder;
    eve_vm();

    //functions defined in CoreDSL
    using ARCH = eve_core;
    
    void set_SN_ZE(uint8_t res){
        auto* SN = reinterpret_cast<uint8_t*>(this->regs_base_ptr+::iss::arch::traits<ARCH>::reg_byte_offsets[::iss::arch::traits<ARCH>::SN]); 
        auto* ZE = reinterpret_cast<uint8_t*>(this->regs_base_ptr+::iss::arch::traits<ARCH>::reg_byte_offsets[::iss::arch::traits<ARCH>::ZE]); 
        *SN = (res & 128) != 0;
        *ZE = res == 0;
    }
    
};
#endif