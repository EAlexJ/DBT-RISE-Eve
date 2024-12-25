#include "util/logging.h"
#include "vm.h"
#include <core.h>
#include <iostream>
int main() {
  LOGGER(DEFAULT)::reporting_level() = logging::INFO;
  auto my_vm = eve_vm();
  auto [atart_addr, success] =
      my_vm.get_arch()->load_file("/root/DBT-RISE-EVE/1+1.hex");
  if (!success) {
    return 1;
  }
  my_vm.start(100);
  return 0;
}
