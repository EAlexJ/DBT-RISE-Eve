#ifndef EVE_VM_H
#define EVE_VM_H

#include "core.h"
#include "iss/tcc/vm_base.h"
#include "iss/vm_plugin.h"
#include <instrs.h>
#include <iss/instruction_decoder.h>
#include <vector>
using namespace iss::tcc;
using namespace iss::debugger;
struct eve_vm : public vm_base<eve_core> {
  target_adapter_if *accquire_target_adapter(server_if *srv) override;
  continuation_e gen_single_inst_behavior(virt_addr_t &pc_v,
                                          tu_builder &tu) override;
  decoder local_decoder;
  eve_vm();
  void add_prologue(tu_builder &) override;

private:
  void increment_icount(tu_builder &);
};
extern "C" {
extern void print_eve_out(void *, int, int);
}
#endif