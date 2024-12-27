#include "core.h"
#include "util/logging.h"

#include <fstream>
#include <iostream>

eve_core::eve_core() {}
eve_core::~eve_core() {}

void eve_core::reset(uint64_t addr) {
  auto base_ptr = get_regs_base_ptr();
  for (int i = 0; i < 8; i++)
    *(base_ptr + i) = 0;
  reg.PC = addr;
  reg.SP = 0;
  reg.CY = reg.SN = reg.ZE = reg.OV = false;
}

uint8_t *eve_core::get_regs_base_ptr() {
  return reinterpret_cast<uint8_t *>(&reg);
}

std::pair<uint64_t, bool> eve_core::load_file(std::string name, int type) {
  std::ifstream filestream(name);

  if (filestream.is_open()) {
    unsigned linenumber = 0;
    std::string line;
    while (filestream >> line) {
      try {
        Imem.at(linenumber) = std::stoi(line, nullptr, 16);
      } catch (std::out_of_range) {
        CPPLOG(ERR) << "Access in file '" << name << "' at line " << linenumber
                    << " is out of range";
        return std::make_pair(0, false);
      } catch (std::invalid_argument) {
        CPPLOG(ERR) << "Invalid argument in file '" << name << "' at line "
                    << linenumber;
        return std::make_pair(0, false);
      }
      linenumber++;
    }
    return std::make_pair(0, true);
  }
  CPPLOG(ERR) << "Something went wrong when opening file '" << name << "'";
  return std::make_pair(0, false);
}

status eve_core::read(const address_type type, const access_type access,
                      const uint32_t space, const uint64_t addr,
                      const unsigned length, uint8_t *const data) {
  if (access == iss::access_type::READ) {
    for (int i = 0; i < length; i++)
      *(data + i) = Dmem.at((addr + i) % Dmem.size());
    return iss::Ok;
  } else if (access == iss::access_type::FETCH) {
    for (int i = 0; i < length; i++)
      *(data + i) = Imem.at((addr + i) % Imem.size());
    return iss::Ok;
  }
  return iss::Err;
};
status eve_core::write(const address_type type, const access_type access,
                       const uint32_t space, const uint64_t addr,
                       const unsigned length, const uint8_t *const data) {
  std::copy(data, data + length, Dmem.data() + (addr % Dmem.size()));
  return iss::Ok;
};

iss::arch::traits<eve_core>::phys_addr_t
eve_core::virt2phys(const iss::addr_t &addr) {
  return addr;
};