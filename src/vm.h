#ifndef EVE_VM_H
#define EVE_VM_H

#include "core.h"
#include "iss/interp/vm_base.h"
#include "iss/vm_plugin.h"
#include <eve.h>
#include <iss/instruction_decoder.h>
#include <memory>
#include <vector>
using namespace iss::interp;
using namespace iss::debugger;
struct eve_vm : public vm_base<eve_core> {
  virt_addr_t execute_inst(finish_cond_e cond, virt_addr_t start,
                           uint64_t icount_limit) override;
  target_adapter_if *accquire_target_adapter(server_if *srv) override;
  decoder local_decoder;
  eve_vm();
};
#endif