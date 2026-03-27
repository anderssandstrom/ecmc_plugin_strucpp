#pragma once

#include <cstdint>

#include "iec_located.hpp"

#define ECMC_STRUCPP_LOGIC_ABI_VERSION 1

struct ecmcStrucppLogicApi {
  uint32_t abi_version;
  const char* name;
  void* (*create_instance)();
  void (*destroy_instance)(void* instance);
  void (*run_cycle)(void* instance);
  const strucpp::LocatedVar* (*get_located_vars)(void* instance);
  uint32_t (*get_located_var_count)(void* instance);
};

extern "C" {
const ecmcStrucppLogicApi* ecmc_strucpp_logic_get_api();
}
