#include <boost/program_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <core.h>
#include <iostream>
#include <string>
#include <util/logging.h>
#include <vm.h>

namespace po = boost::program_options;

int main(int argc, char *argv[]) {
  std::string filename;
  int icount = 0;
  bool dump = false;
  try {
    // Define and configure options
    po::options_description mandatory("Mandatory options");
    mandatory.add_options()("file,f",
                            po::value<std::string>(&filename)->required(),
                            "File to process");

    po::options_description optional("Optional options");
    optional.add_options()(
        "icount", po::value<int>(&icount)->default_value(100),
        "Instruction count")("dump", po::bool_switch(&dump),
                             "Toggle dumping of Intermediate Representation");

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
  CPPLOG(INFO) << "Loading file '" << filename << "'";
  auto my_vm = eve_vm();
  auto [start_addr, success] = my_vm.get_arch()->load_file(filename);
  if (!success) {
    return 1;
  }
  my_vm.reset(start_addr);
  my_vm.start(icount, dump);
  return 0;
}
