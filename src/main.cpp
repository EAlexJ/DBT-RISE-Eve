#include <boost/program_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <core.h>
#include <iostream>
#include <iss/log_categories.h>
#include <string>
#include <util/logging.h>
#include <vm.h>

namespace po = boost::program_options;

int main(int argc, char *argv[]) {
  std::string filename;
  int icount = 0;
  bool disass = false;
  try {
    // Define and configure options
    po::options_description mandatory("Mandatory options");
    mandatory.add_options()("file,f",
                            po::value<std::string>(&filename)->required(),
                            "File to process");

    po::options_description optional("Optional options");
    optional.add_options()("icount",
                           po::value<int>(&icount)->default_value(100),
                           "Instruction count")(
        "disass", po::bool_switch(&disass), "Toggle disassembly output");

    po::options_description all("Allowed options");
    all.add(mandatory).add(optional).add_options()("help",
                                                   "Show this help message");

    // Parse command-line arguments
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, all), vm);

    // Handle help option
    if (vm.count("help")) {
      std::cout << all << "\n";
      return 0;
    }

    // Notify for mandatory options
    po::notify(vm);
  } catch (const po::error &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
  LOGGER(DEFAULT)::reporting_level() = logging::INFO;
  LOGGER(DEFAULT)::print_time() = false;
  CPPLOG(INFO) << "Loading file '" << filename << "'";
  auto my_vm = eve_vm();
  auto [start_addr, success] = my_vm.get_arch()->load_file(filename);
  if (!success) {
    return 1;
  }
  if (disass) {
    my_vm.setDisassEnabled(disass);
    LOGGER(disass)::reporting_level() = logging::INFO;
    LOGGER(disass)::print_time() = false;
  }
  my_vm.reset(start_addr);
  my_vm.start(icount, true);
  return 0;
}
