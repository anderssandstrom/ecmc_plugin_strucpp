#pragma once

#include <cstdint>

#include "iec_located.hpp"

#define ECMC_STRUCPP_LOGIC_ABI_VERSION 2

enum ecmcStrucppExportType : uint32_t {
  ECMC_STRUCPP_EXPORT_BOOL = 1,
  ECMC_STRUCPP_EXPORT_S8 = 2,
  ECMC_STRUCPP_EXPORT_U8 = 3,
  ECMC_STRUCPP_EXPORT_S16 = 4,
  ECMC_STRUCPP_EXPORT_U16 = 5,
  ECMC_STRUCPP_EXPORT_S32 = 6,
  ECMC_STRUCPP_EXPORT_U32 = 7,
  ECMC_STRUCPP_EXPORT_F32 = 8,
  ECMC_STRUCPP_EXPORT_F64 = 9,
};

struct ecmcStrucppExportedVar {
  const char* name;
  void* data;
  uint32_t type;
  uint32_t writable;
};

struct ecmcStrucppLogicApi {
  uint32_t abi_version;
  const char* name;
  void* (*create_instance)();
  void (*destroy_instance)(void* instance);
  void (*run_cycle)(void* instance);
  const strucpp::LocatedVar* (*get_located_vars)(void* instance);
  uint32_t (*get_located_var_count)(void* instance);
  const ecmcStrucppExportedVar* (*get_exported_vars)(void* instance);
  uint32_t (*get_exported_var_count)(void* instance);
};

extern "C" {
const ecmcStrucppLogicApi* ecmc_strucpp_logic_get_api();
}
