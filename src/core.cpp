#include "iss/log_categories.h"
#include <core.h>
#include <filesystem>
#include <fmt/format.h>
#include <iss/arch/traits.h>
#include <iss/vm_types.h>
#include <util/logging.h>

#include <cassert>
#include <fstream>
#include <iostream>

using namespace iss;

eve_core::eve_core() {}
eve_core::~eve_core() {}
using traits = iss::arch::traits<eve_core>;
void eve_core::reset(uint64_t addr) {
  auto base_ptr = get_regs_base_ptr();
  for (int i = 0; i < 8; i++)
    *(base_ptr + i) = 0;
  *(base_ptr + traits::reg_byte_offsets[traits::PC]) = addr;
  *(base_ptr + traits::reg_byte_offsets[traits::SP]) = 0;
  *(base_ptr + traits::reg_byte_offsets[traits::CY]) = false;
  *(base_ptr + traits::reg_byte_offsets[traits::SN]) = false;
  *(base_ptr + traits::reg_byte_offsets[traits::ZE]) = false;
  *(base_ptr + traits::reg_byte_offsets[traits::OV]) = false;
}

uint8_t *eve_core::get_regs_base_ptr() {
  return reinterpret_cast<uint8_t *>(&reg);
}

std::pair<uint64_t, bool> eve_core::load_file(std::string name, int type) {
  std::ifstream filestream(name);
  try {
    if (filestream.is_open() && std::filesystem::file_size(name) != 0) {
      unsigned linenumber = 0;
      std::string line;
      while (filestream >> line) {
        try {
          Imem.at(linenumber) = std::stoi(line, nullptr, 16);
        } catch (std::out_of_range) {
          CPPLOG(ERR) << "Access in file '" << name << "' at line "
                      << linenumber << " is out of range";
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

  } catch (std::filesystem::filesystem_error &e) {
    CPPLOG(ERR) << e.what();
    return std::make_pair(0, false);
  } catch (...) {
  }
  CPPLOG(ERR) << "Something went wrong when opening file '" << name << "'";
  return std::make_pair(0, false);
}

status eve_core::read(const address_type type, const access_type access,
                      const uint32_t space, const uint64_t addr,
                      const unsigned length, uint8_t *const data) {
  if (space == arch::traits<eve_core>::mem_type_e::DMEM) {
    for (int i = 0; i < length; i++)
      *(data + i) = Dmem.at((addr + i) % Dmem.size());
    return iss::Ok;
  } else if (space == arch::traits<eve_core>::mem_type_e::IMEM) {
    for (int i = 0; i < length; i++)
      *(data + i) = Imem.at((addr + i) % Imem.size());
    return iss::Ok;
  }
  return iss::Err;
};
status eve_core::write(const address_type type, const access_type access,
                       const uint32_t space, const uint64_t addr,
                       const unsigned length, const uint8_t *const data) {
  assert(space == arch::traits<eve_core>::mem_type_e::DMEM &&
         "Can only write to Dmem");
  std::copy(data, data + length, Dmem.data() + (addr % Dmem.size()));
  return iss::Ok;
};

iss::arch::traits<eve_core>::phys_addr_t
eve_core::virt2phys(const iss::addr_t &addr) {
  return addr;
};

void eve_core::disass_output(uint64_t pc, const std::string instr) {
  NSCLOG(INFO, logging::disass) << fmt::format("0x{:08x}    {:40} [i:0x{:x}]",
                                               pc, instr, this->reg.ICOUNT);
};