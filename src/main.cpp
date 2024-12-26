#include "util/logging.h"
#include "vm.h"
#include <core.h>
#include <iostream>
int main(int argc, char *argv[]) {
  LOGGER(DEFAULT)::reporting_level() = logging::INFO;
  std::string file_path("1+1.hex");
  if (argc > 1)
    file_path = argv[1];
  CPPLOG(INFO) << "Loading file '" << file_path << "'";
  auto my_vm = eve_vm();
  auto [start_addr, success] = my_vm.get_arch()->load_file(file_path);
  if (!success) {
    return 1;
  }
  my_vm.reset(start_addr);
  my_vm.start(100);
  return 0;
}
