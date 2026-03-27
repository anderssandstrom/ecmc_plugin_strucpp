/*************************************************************************\
* Copyright (c) 2026 Paul Scherrer Institute
* ecmc is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*
*  ecmcPluginStrucpp.cpp
*
\*************************************************************************/

#define ECMC_IS_PLUGIN
#define ECMC_PLUGIN_VERSION 1

#include "ecmcStrucppBridge.hpp"
#include "ecmcStrucppLogicIface.hpp"

#include "ecmcAsynDataItem.h"
#include "ecmcAsynPortDriver.h"
#include "ecmcDataItem.h"
#include "ecmcPluginClient.h"
#include "ecmcPluginDefs.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct ItemBindingSpec {
  size_t offset {};
  std::string item_name;
  size_t bytes {};
};

struct BoundItemBinding {
  size_t offset {};
  ecmcStrucppIoImageSpan item;
  ecmcDataItemInfo info {};
  size_t bytes {};
};

struct LocatedAddressKey {
  strucpp::LocatedArea area {strucpp::LocatedArea::Input};
  strucpp::LocatedSize size {strucpp::LocatedSize::Bit};
  uint16_t byte_index {};
  uint8_t bit_index {};
};

struct AddressMappingSpec {
  LocatedAddressKey address;
  std::string item_name;
};

struct BoundAddressMapping {
  LocatedAddressKey address;
  ecmcStrucppIoImageSpan item;
  ecmcDataItemInfo info {};
};

struct PluginConfig {
  std::string logic_lib;
  std::string mapping_file;
  std::string asyn_port_name;
  std::string input_item;
  std::string output_item;
  std::vector<ItemBindingSpec> input_bindings;
  std::vector<ItemBindingSpec> output_bindings;
  size_t memory_bytes {256};
};

struct LogicRuntime {
  void* dl_handle {};
  const ecmcStrucppLogicApi* api {};
  void* instance {};
};

struct ExportedParamBinding {
  std::string name;
  ecmcAsynDataItem* param {};
  uint32_t type {};
  uint32_t writable {};
};

static int alreadyLoaded = 0;
PluginConfig g_config {};
ecmcStrucppIoImages g_images {};
std::vector<uint8_t> g_input_image;
std::vector<uint8_t> g_output_image;
std::vector<uint8_t> g_memory_image;
std::vector<BoundItemBinding> g_bound_input_bindings;
std::vector<BoundItemBinding> g_bound_output_bindings;
std::vector<AddressMappingSpec> g_mapping_specs;
std::vector<BoundAddressMapping> g_bound_mappings;
LogicRuntime g_logic {};
ecmcStrucppCompiledCopyPlan g_copy_plan {};
std::vector<ExportedParamBinding> g_exported_params;
ecmcAsynPortDriver* g_asyn_port = nullptr;

constexpr double kPlcAreaInput = 0.0;
constexpr double kPlcAreaOutput = 1.0;
constexpr double kPlcAreaMemory = 2.0;
constexpr const char* kDefaultAsynPortName = "PLUGIN.STRUCPP0";

double plcNaN() {
  return std::numeric_limits<double>::quiet_NaN();
}

void logError(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "[ecmc_plugin_strucpp] ERROR: ");
  std::vfprintf(stderr, fmt, args);
  std::fprintf(stderr, "\n");
  va_end(args);
}

void logInfo(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stdout, "[ecmc_plugin_strucpp] ");
  std::vfprintf(stdout, fmt, args);
  std::fprintf(stdout, "\n");
  va_end(args);
}

ecmcStrucppIoImageSpan* plcAreaImage(double area) {
  if (area == kPlcAreaInput) {
    return &g_images.input;
  }
  if (area == kPlcAreaOutput) {
    return &g_images.output;
  }
  if (area == kPlcAreaMemory) {
    return &g_images.memory;
  }
  return nullptr;
}

bool plcCheckByteRange(const ecmcStrucppIoImageSpan* span,
                       size_t offset,
                       size_t width) {
  return span && span->data && width > 0 && offset <= span->size &&
         width <= (span->size - offset);
}

bool plcCheckBitRange(const ecmcStrucppIoImageSpan* span,
                      size_t byte_offset,
                      size_t bit_index) {
  return span && span->data && byte_offset < span->size && bit_index < 8;
}

template <typename T>
double plcGetScalar(double area, double offset) {
  ecmcStrucppIoImageSpan* const span = plcAreaImage(area);
  if (!span || offset < 0.0) {
    return plcNaN();
  }

  const size_t byte_offset = static_cast<size_t>(offset);
  if (!plcCheckByteRange(span, byte_offset, sizeof(T))) {
    return plcNaN();
  }

  T value {};
  std::memcpy(&value, span->data + byte_offset, sizeof(T));
  return static_cast<double>(value);
}

template <typename T>
double plcSetScalar(double area, double offset, double value) {
  ecmcStrucppIoImageSpan* const span = plcAreaImage(area);
  if (!span || offset < 0.0) {
    return plcNaN();
  }

  const size_t byte_offset = static_cast<size_t>(offset);
  if (!plcCheckByteRange(span, byte_offset, sizeof(T))) {
    return plcNaN();
  }

  const T typed_value = static_cast<T>(value);
  std::memcpy(span->data + byte_offset, &typed_value, sizeof(T));
  return static_cast<double>(typed_value);
}

double plcGetBit(double area, double byte_offset, double bit_index) {
  ecmcStrucppIoImageSpan* const span = plcAreaImage(area);
  if (!span || byte_offset < 0.0 || bit_index < 0.0) {
    return plcNaN();
  }

  const size_t byte_index = static_cast<size_t>(byte_offset);
  const size_t bit = static_cast<size_t>(bit_index);
  if (!plcCheckBitRange(span, byte_index, bit)) {
    return plcNaN();
  }

  return ((span->data[byte_index] >> bit) & 0x1u) ? 1.0 : 0.0;
}

double plcSetBit(double area, double byte_offset, double bit_index, double value) {
  ecmcStrucppIoImageSpan* const span = plcAreaImage(area);
  if (!span || byte_offset < 0.0 || bit_index < 0.0) {
    return plcNaN();
  }

  const size_t byte_index = static_cast<size_t>(byte_offset);
  const size_t bit = static_cast<size_t>(bit_index);
  if (!plcCheckBitRange(span, byte_index, bit)) {
    return plcNaN();
  }

  const uint8_t mask = static_cast<uint8_t>(1u << bit);
  if (value != 0.0) {
    span->data[byte_index] = static_cast<uint8_t>(span->data[byte_index] | mask);
  } else {
    span->data[byte_index] = static_cast<uint8_t>(span->data[byte_index] & ~mask);
  }

  return value != 0.0 ? 1.0 : 0.0;
}

double plcGetU8(double area, double offset) {
  return plcGetScalar<uint8_t>(area, offset);
}

double plcSetU8(double area, double offset, double value) {
  return plcSetScalar<uint8_t>(area, offset, value);
}

double plcGetS8(double area, double offset) {
  return plcGetScalar<int8_t>(area, offset);
}

double plcSetS8(double area, double offset, double value) {
  return plcSetScalar<int8_t>(area, offset, value);
}

double plcGetU16(double area, double offset) {
  return plcGetScalar<uint16_t>(area, offset);
}

double plcSetU16(double area, double offset, double value) {
  return plcSetScalar<uint16_t>(area, offset, value);
}

double plcGetS16(double area, double offset) {
  return plcGetScalar<int16_t>(area, offset);
}

double plcSetS16(double area, double offset, double value) {
  return plcSetScalar<int16_t>(area, offset, value);
}

double plcGetU32(double area, double offset) {
  return plcGetScalar<uint32_t>(area, offset);
}

double plcSetU32(double area, double offset, double value) {
  return plcSetScalar<uint32_t>(area, offset, value);
}

double plcGetS32(double area, double offset) {
  return plcGetScalar<int32_t>(area, offset);
}

double plcSetS32(double area, double offset, double value) {
  return plcSetScalar<int32_t>(area, offset, value);
}

double plcGetF32(double area, double offset) {
  return plcGetScalar<float>(area, offset);
}

double plcSetF32(double area, double offset, double value) {
  return plcSetScalar<float>(area, offset, value);
}

double plcGetF64(double area, double offset) {
  return plcGetScalar<double>(area, offset);
}

double plcSetF64(double area, double offset, double value) {
  return plcSetScalar<double>(area, offset, value);
}

bool exportTypeInfo(uint32_t export_type,
                    asynParamType* asyn_type_out,
                    ecmcEcDataType* data_type_out,
                    size_t* bytes_out) {
  if (!asyn_type_out || !data_type_out || !bytes_out) {
    return false;
  }

  switch (export_type) {
  case ECMC_STRUCPP_EXPORT_BOOL:
    *asyn_type_out = asynParamInt32;
    *data_type_out = ECMC_EC_U8;
    *bytes_out = 1;
    return true;
  case ECMC_STRUCPP_EXPORT_S8:
    *asyn_type_out = asynParamInt32;
    *data_type_out = ECMC_EC_S8;
    *bytes_out = 1;
    return true;
  case ECMC_STRUCPP_EXPORT_U8:
    *asyn_type_out = asynParamInt32;
    *data_type_out = ECMC_EC_U8;
    *bytes_out = 1;
    return true;
  case ECMC_STRUCPP_EXPORT_S16:
    *asyn_type_out = asynParamInt32;
    *data_type_out = ECMC_EC_S16;
    *bytes_out = 2;
    return true;
  case ECMC_STRUCPP_EXPORT_U16:
    *asyn_type_out = asynParamInt32;
    *data_type_out = ECMC_EC_U16;
    *bytes_out = 2;
    return true;
  case ECMC_STRUCPP_EXPORT_S32:
    *asyn_type_out = asynParamInt32;
    *data_type_out = ECMC_EC_S32;
    *bytes_out = 4;
    return true;
  case ECMC_STRUCPP_EXPORT_U32:
    *asyn_type_out = asynParamUInt32Digital;
    *data_type_out = ECMC_EC_U32;
    *bytes_out = 4;
    return true;
  case ECMC_STRUCPP_EXPORT_F32:
    *asyn_type_out = asynParamFloat64;
    *data_type_out = ECMC_EC_F32;
    *bytes_out = 4;
    return true;
  case ECMC_STRUCPP_EXPORT_F64:
    *asyn_type_out = asynParamFloat64;
    *data_type_out = ECMC_EC_F64;
    *bytes_out = 8;
    return true;
  default:
    return false;
  }
}

const ecmcStrucppExportedVar* logicExportedVars() {
  if (!g_logic.api || !g_logic.instance || !g_logic.api->get_exported_vars) {
    return nullptr;
  }
  return g_logic.api->get_exported_vars(g_logic.instance);
}

size_t logicExportedVarCount() {
  if (!g_logic.api || !g_logic.instance || !g_logic.api->get_exported_var_count) {
    return 0;
  }
  return static_cast<size_t>(g_logic.api->get_exported_var_count(g_logic.instance));
}

bool bindExportedVars(std::string* error_out) {
  g_exported_params.clear();

  const ecmcStrucppExportedVar* const vars = logicExportedVars();
  const size_t count = logicExportedVarCount();
  if (!vars || count == 0) {
    return true;
  }

  if (!g_asyn_port) {
    if (error_out) {
      *error_out = "Plugin asyn port is not initialized";
    }
    return false;
  }

  for (size_t i = 0; i < count; ++i) {
    const auto& export_var = vars[i];
    if (!export_var.name || !export_var.name[0]) {
      if (error_out) {
        *error_out = "Encountered exported ST variable with empty name";
      }
      return false;
    }
    if (!export_var.data) {
      if (error_out) {
        *error_out = std::string("Exported ST variable '") + export_var.name +
                     "' has null data pointer";
      }
      return false;
    }

    asynParamType asyn_type = asynParamNotDefined;
    ecmcEcDataType data_type = ECMC_EC_NONE;
    size_t bytes = 0;
    if (!exportTypeInfo(export_var.type, &asyn_type, &data_type, &bytes)) {
      if (error_out) {
        *error_out = std::string("Unsupported exported ST variable type for '") +
                     export_var.name + "'";
      }
      return false;
    }

    ecmcAsynDataItem* param = g_asyn_port->findAvailParam(export_var.name);
    if (param) {
      param->setEcmcDataPointer(static_cast<uint8_t*>(export_var.data), bytes);
      param->setEcmcDataType(data_type);
      param->setAllowWriteToEcmc(export_var.writable != 0);
      if (export_var.type == ECMC_STRUCPP_EXPORT_BOOL) {
        param->setEcmcBitCount(1);
      }
    } else {
      param = g_asyn_port->addNewAvailParam(export_var.name,
                                            asyn_type,
                                            static_cast<uint8_t*>(export_var.data),
                                            bytes,
                                            data_type,
                                            true);
      if (!param) {
        if (error_out) {
          *error_out = std::string("Failed to create asyn parameter for exported ST variable '") +
                       export_var.name + "'";
        }
        return false;
      }
      param->setAllowWriteToEcmc(export_var.writable != 0);
      if (export_var.type == ECMC_STRUCPP_EXPORT_BOOL) {
        param->setEcmcBitCount(1);
      }
    }

    param->refreshParamRT(1);
    g_exported_params.push_back({export_var.name, param, export_var.type, export_var.writable});
  }

  return true;
}

bool ensureAsynPort(std::string* error_out) {
  if (g_asyn_port) {
    return true;
  }

  try {
    g_asyn_port = new ecmcAsynPortDriver(g_config.asyn_port_name.c_str(),
                                         128,
                                         1,
                                         0,
                                         100.0);
  } catch (const std::exception& ex) {
    if (error_out) {
      *error_out = std::string("Failed to create plugin asyn port '") +
                   g_config.asyn_port_name + "': " + ex.what();
    }
    g_asyn_port = nullptr;
    return false;
  }

  if (!g_asyn_port) {
    if (error_out) {
      *error_out = std::string("Failed to create plugin asyn port '") +
                   g_config.asyn_port_name + "'";
    }
    return false;
  }

  return true;
}

void destroyAsynPort() {
  delete g_asyn_port;
  g_asyn_port = nullptr;
}

std::string trim(std::string value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(start, end - start);
}

bool parseSizeToken(const std::string& text,
                    const char* field_name,
                    size_t* out_value,
                    std::string* error_out) {
  if (!out_value) {
    if (error_out) {
      *error_out = std::string("Output pointer is null for ") + field_name;
    }
    return false;
  }

  char* end_ptr = nullptr;
  const unsigned long long parsed = std::strtoull(text.c_str(), &end_ptr, 0);
  if (!end_ptr || *end_ptr != '\0') {
    if (error_out) {
      *error_out = std::string("Invalid ") + field_name + " value: '" + text + "'";
    }
    return false;
  }

  *out_value = static_cast<size_t>(parsed);
  return true;
}

std::string formatAddress(const LocatedAddressKey& address) {
  std::ostringstream oss;
  oss << "%" << strucpp::area_to_char(address.area)
      << strucpp::size_to_char(address.size)
      << address.byte_index;
  if (address.size == strucpp::LocatedSize::Bit) {
    oss << "." << static_cast<int>(address.bit_index);
  }
  return oss.str();
}

std::string formatAddress(const strucpp::LocatedVar& var) {
  LocatedAddressKey key {var.area, var.size, var.byte_index, var.bit_index};
  return formatAddress(key);
}

const char* dataDirectionName(ecmcDataDir direction) {
  switch (direction) {
  case ECMC_DIR_WRITE:
    return "write";
  case ECMC_DIR_READ:
    return "read";
  case ECMC_DIR_INVALID:
  case ECMC_DIR_COUNT:
    return "invalid";
  }
  return "unknown";
}

const char* dataTypeName(ecmcEcDataType type) {
  switch (type) {
  case ECMC_EC_NONE:
    return "NONE";
  case ECMC_EC_B1:
    return "B1";
  case ECMC_EC_B2:
    return "B2";
  case ECMC_EC_B3:
    return "B3";
  case ECMC_EC_B4:
    return "B4";
  case ECMC_EC_U8:
    return "U8";
  case ECMC_EC_S8:
    return "S8";
  case ECMC_EC_U16:
    return "U16";
  case ECMC_EC_S16:
    return "S16";
  case ECMC_EC_U32:
    return "U32";
  case ECMC_EC_S32:
    return "S32";
  case ECMC_EC_U64:
    return "U64";
  case ECMC_EC_S64:
    return "S64";
  case ECMC_EC_F32:
    return "F32";
  case ECMC_EC_F64:
    return "F64";
  case ECMC_EC_S8_TO_U8:
    return "S8_TO_U8";
  case ECMC_EC_S16_TO_U16:
    return "S16_TO_U16";
  case ECMC_EC_S32_TO_U32:
    return "S32_TO_U32";
  case ECMC_EC_S64_TO_U64:
    return "S64_TO_U64";
  }
  return "UNKNOWN";
}

bool isIntegerLikeDataType(ecmcEcDataType type) {
  switch (type) {
  case ECMC_EC_B1:
  case ECMC_EC_B2:
  case ECMC_EC_B3:
  case ECMC_EC_B4:
  case ECMC_EC_U8:
  case ECMC_EC_S8:
  case ECMC_EC_U16:
  case ECMC_EC_S16:
  case ECMC_EC_U32:
  case ECMC_EC_S32:
  case ECMC_EC_U64:
  case ECMC_EC_S64:
  case ECMC_EC_S8_TO_U8:
  case ECMC_EC_S16_TO_U16:
  case ECMC_EC_S32_TO_U32:
  case ECMC_EC_S64_TO_U64:
    return true;
  case ECMC_EC_NONE:
  case ECMC_EC_F32:
  case ECMC_EC_F64:
    return false;
  }
  return false;
}

bool isCompatibleScalarType(strucpp::LocatedSize size, ecmcEcDataType type) {
  switch (size) {
  case strucpp::LocatedSize::Bit:
    return isIntegerLikeDataType(type);
  case strucpp::LocatedSize::Byte:
    return type == ECMC_EC_U8 || type == ECMC_EC_S8 || type == ECMC_EC_S8_TO_U8;
  case strucpp::LocatedSize::Word:
    return type == ECMC_EC_U16 || type == ECMC_EC_S16 || type == ECMC_EC_S16_TO_U16;
  case strucpp::LocatedSize::DWord:
    return type == ECMC_EC_U32 || type == ECMC_EC_S32 ||
           type == ECMC_EC_S32_TO_U32 || type == ECMC_EC_F32;
  case strucpp::LocatedSize::LWord:
    return type == ECMC_EC_U64 || type == ECMC_EC_S64 ||
           type == ECMC_EC_S64_TO_U64 || type == ECMC_EC_F64;
  }
  return false;
}

bool validateDirectionForArea(strucpp::LocatedArea area,
                              const ecmcDataItemInfo& info,
                              const std::string& item_name,
                              std::string* error_out) {
  if (info.dataDirection == ECMC_DIR_INVALID) {
    return true;
  }

  const ecmcDataDir expected =
    area == strucpp::LocatedArea::Output ? ECMC_DIR_WRITE : ECMC_DIR_READ;
  if (info.dataDirection != expected) {
    if (error_out) {
      std::ostringstream oss;
      oss << "ecmcDataItem '" << item_name << "' has direction "
          << dataDirectionName(info.dataDirection) << " but "
          << (area == strucpp::LocatedArea::Output ? "%Q" : "%I")
          << " expects " << dataDirectionName(expected);
      *error_out = oss.str();
    }
    return false;
  }
  return true;
}

bool validateDirectMappedItemType(const strucpp::LocatedVar& var,
                                  const ecmcDataItemInfo& info,
                                  const std::string& item_name,
                                  std::string* error_out) {
  if (!validateDirectionForArea(var.area, info, item_name, error_out)) {
    return false;
  }

  if (var.size == strucpp::LocatedSize::Bit) {
    if (info.dataSize < 1) {
      if (error_out) {
        *error_out = "ecmcDataItem '" + item_name + "' is too small for " +
                     formatAddress(var);
      }
      return false;
    }

    if (info.dataBitCount != 0 && var.bit_index >= info.dataBitCount) {
      if (error_out) {
        std::ostringstream oss;
        oss << "ecmcDataItem '" << item_name << "' only exposes "
            << info.dataBitCount << " bits but " << formatAddress(var)
            << " requires bit index " << static_cast<int>(var.bit_index);
        *error_out = oss.str();
      }
      return false;
    }

    if (info.dataType != ECMC_EC_NONE && !isCompatibleScalarType(var.size, info.dataType)) {
      if (error_out) {
        std::ostringstream oss;
        oss << "ecmcDataItem '" << item_name << "' has incompatible type "
            << dataTypeName(info.dataType) << " for " << formatAddress(var);
        *error_out = oss.str();
      }
      return false;
    }
    return true;
  }

  const size_t expected_bytes = var.byte_size();
  if (info.dataSize != expected_bytes) {
    if (error_out) {
      std::ostringstream oss;
      oss << "ecmcDataItem '" << item_name << "' exposes " << info.dataSize
          << " bytes but " << formatAddress(var) << " requires exactly "
          << expected_bytes;
      *error_out = oss.str();
    }
    return false;
  }

  if (info.dataElementSize != 0 && info.dataElementSize != expected_bytes) {
    if (error_out) {
      std::ostringstream oss;
      oss << "ecmcDataItem '" << item_name << "' has element size "
          << info.dataElementSize << " but " << formatAddress(var)
          << " requires " << expected_bytes;
      *error_out = oss.str();
    }
    return false;
  }

  if (info.dataType != ECMC_EC_NONE && !isCompatibleScalarType(var.size, info.dataType)) {
    if (error_out) {
      std::ostringstream oss;
      oss << "ecmcDataItem '" << item_name << "' has incompatible type "
          << dataTypeName(info.dataType) << " for " << formatAddress(var);
      *error_out = oss.str();
    }
    return false;
  }

  if (info.dataBitCount != 0 && info.dataBitCount != expected_bytes * 8) {
    if (error_out) {
      std::ostringstream oss;
      oss << "ecmcDataItem '" << item_name << "' reports " << info.dataBitCount
          << " bits but " << formatAddress(var) << " requires "
          << (expected_bytes * 8);
      *error_out = oss.str();
    }
    return false;
  }

  return true;
}

bool addressesEqual(const LocatedAddressKey& lhs, const LocatedAddressKey& rhs) {
  return lhs.area == rhs.area && lhs.size == rhs.size &&
         lhs.byte_index == rhs.byte_index && lhs.bit_index == rhs.bit_index;
}

bool addressesEqual(const strucpp::LocatedVar& lhs, const LocatedAddressKey& rhs) {
  return lhs.area == rhs.area && lhs.size == rhs.size &&
         lhs.byte_index == rhs.byte_index && lhs.bit_index == rhs.bit_index;
}

bool parseBindingList(const std::string& raw_value,
                      std::vector<ItemBindingSpec>* out_specs,
                      std::string* error_out) {
  if (!out_specs) {
    if (error_out) {
      *error_out = "Output binding vector is null";
    }
    return false;
  }

  out_specs->clear();
  const std::string value = trim(raw_value);
  if (value.empty()) {
    return true;
  }

  size_t cursor = 0;
  while (cursor < value.size()) {
    const size_t next = value.find(',', cursor);
    const std::string token =
      trim(value.substr(cursor, next == std::string::npos ? std::string::npos
                                                          : next - cursor));

    if (!token.empty()) {
      const size_t colon = token.find(':');
      if (colon == std::string::npos) {
        if (error_out) {
          *error_out = "Invalid binding token '" + token +
                       "'. Expected <offset>:<item>[@bytes]";
        }
        return false;
      }

      ItemBindingSpec spec {};
      if (!parseSizeToken(trim(token.substr(0, colon)),
                          "binding offset",
                          &spec.offset,
                          error_out)) {
        return false;
      }

      std::string item_part = trim(token.substr(colon + 1));
      const size_t at = item_part.rfind('@');
      if (at != std::string::npos) {
        const std::string bytes_part = trim(item_part.substr(at + 1));
        item_part = trim(item_part.substr(0, at));
        if (!parseSizeToken(bytes_part, "binding byte size", &spec.bytes, error_out)) {
          return false;
        }
        if (spec.bytes == 0) {
          if (error_out) {
            *error_out = "Binding byte size must be greater than zero";
          }
          return false;
        }
      }

      if (item_part.empty()) {
        if (error_out) {
          *error_out = "Binding token '" + token + "' is missing an item name";
        }
        return false;
      }

      spec.item_name = item_part;
      out_specs->push_back(spec);
    }

    if (next == std::string::npos) {
      break;
    }
    cursor = next + 1;
  }

  return true;
}

bool parseLocatedAddress(const std::string& text,
                         LocatedAddressKey* out_key,
                         std::string* error_out) {
  if (!out_key) {
    if (error_out) {
      *error_out = "Output address pointer is null";
    }
    return false;
  }

  const std::string value = trim(text);
  if (value.size() < 4 || value[0] != '%') {
    if (error_out) {
      *error_out = "Invalid located address '" + value + "'";
    }
    return false;
  }

  try {
    out_key->area = strucpp::parse_area(value[1]);
    out_key->size = strucpp::parse_size(value[2]);
  } catch (const std::exception&) {
    if (error_out) {
      *error_out = "Invalid located address '" + value + "'";
    }
    return false;
  }

  size_t number_start = 3;
  size_t dot = value.find('.', number_start);
  const std::string byte_text =
    value.substr(number_start, dot == std::string::npos ? std::string::npos
                                                        : dot - number_start);

  size_t byte_index = 0;
  if (!parseSizeToken(byte_text, "byte index", &byte_index, error_out)) {
    return false;
  }
  if (byte_index > 0xFFFFu) {
    if (error_out) {
      *error_out = "Located address byte index out of range: '" + value + "'";
    }
    return false;
  }
  out_key->byte_index = static_cast<uint16_t>(byte_index);

  if (out_key->size == strucpp::LocatedSize::Bit) {
    if (dot == std::string::npos) {
      if (error_out) {
        *error_out = "Bit located address is missing .bit suffix: '" + value + "'";
      }
      return false;
    }

    const std::string bit_text = value.substr(dot + 1);
    size_t bit_index = 0;
    if (!parseSizeToken(bit_text, "bit index", &bit_index, error_out)) {
      return false;
    }
    if (bit_index > 7) {
      if (error_out) {
        *error_out = "Located bit index out of range in '" + value + "'";
      }
      return false;
    }
    out_key->bit_index = static_cast<uint8_t>(bit_index);
  } else {
    if (dot != std::string::npos) {
      if (error_out) {
        *error_out = "Non-bit located address must not include .bit suffix: '" +
                     value + "'";
      }
      return false;
    }
    out_key->bit_index = 0;
  }

  return true;
}

bool loadMappingFile(const std::string& path,
                     std::vector<AddressMappingSpec>* out_specs,
                     std::string* error_out) {
  if (!out_specs) {
    if (error_out) {
      *error_out = "Output mapping vector is null";
    }
    return false;
  }

  out_specs->clear();
  std::ifstream file(path);
  if (!file.is_open()) {
    if (error_out) {
      *error_out = "Could not open mapping file '" + path + "'";
    }
    return false;
  }

  std::string line;
  size_t line_number = 0;
  while (std::getline(file, line)) {
    ++line_number;
    const size_t comment = line.find('#');
    const std::string stripped =
      trim(comment == std::string::npos ? line : line.substr(0, comment));

    if (stripped.empty()) {
      continue;
    }

    const size_t eq = stripped.find('=');
    if (eq == std::string::npos) {
      if (error_out) {
        std::ostringstream oss;
        oss << "Invalid mapping line " << line_number
            << ". Expected %<address>=<ecmcDataItem>";
        *error_out = oss.str();
      }
      return false;
    }

    AddressMappingSpec spec {};
    if (!parseLocatedAddress(trim(stripped.substr(0, eq)), &spec.address, error_out)) {
      if (error_out) {
        *error_out += " on line " + std::to_string(line_number);
      }
      return false;
    }

    spec.item_name = trim(stripped.substr(eq + 1));
    if (spec.item_name.empty()) {
      if (error_out) {
        std::ostringstream oss;
        oss << "Mapping line " << line_number << " is missing an ecmcDataItem name";
        *error_out = oss.str();
      }
      return false;
    }

    for (const auto& existing : *out_specs) {
      if (addressesEqual(existing.address, spec.address)) {
        if (error_out) {
          std::ostringstream oss;
          oss << "Duplicate mapping for " << formatAddress(spec.address)
              << " in '" << path << "'";
          *error_out = oss.str();
        }
        return false;
      }
    }

    out_specs->push_back(spec);
  }

  return true;
}

bool parseConfigString(const char* raw_config,
                       PluginConfig* out_config,
                       std::string* error_out) {
  if (!out_config) {
    if (error_out) {
      *error_out = "Output config pointer is null";
    }
    return false;
  }

  *out_config = PluginConfig {};
  out_config->asyn_port_name = kDefaultAsynPortName;

  if (!raw_config || raw_config[0] == '\0') {
    if (error_out) {
      *error_out =
        "Missing config string. Expected logic_lib=... plus one I/O binding mode";
    }
    return false;
  }

  std::string config(raw_config);
  size_t cursor = 0;

  while (cursor < config.size()) {
    const size_t next = config.find(';', cursor);
    const std::string token =
      trim(config.substr(cursor, next == std::string::npos ? std::string::npos
                                                           : next - cursor));

    if (!token.empty()) {
      const size_t eq = token.find('=');
      if (eq == std::string::npos) {
        if (error_out) {
          *error_out = "Invalid config token: '" + token + "'";
        }
        return false;
      }

      const std::string key = trim(token.substr(0, eq));
      const std::string value = trim(token.substr(eq + 1));

      if (key == "logic_lib") {
        out_config->logic_lib = value;
      } else if (key == "asyn_port") {
        out_config->asyn_port_name = value;
      } else if (key == "mapping_file") {
        out_config->mapping_file = value;
      } else if (key == "input_item") {
        out_config->input_item = value;
      } else if (key == "output_item") {
        out_config->output_item = value;
      } else if (key == "input_bindings") {
        if (!parseBindingList(value, &out_config->input_bindings, error_out)) {
          return false;
        }
      } else if (key == "output_bindings") {
        if (!parseBindingList(value, &out_config->output_bindings, error_out)) {
          return false;
        }
      } else if (key == "memory_bytes") {
        if (!parseSizeToken(value, "memory_bytes", &out_config->memory_bytes, error_out)) {
          return false;
        }
        if (out_config->memory_bytes == 0) {
          if (error_out) {
            *error_out = "Invalid memory_bytes value: '" + value + "'";
          }
          return false;
        }
      } else {
        if (error_out) {
          *error_out = "Unsupported config key: '" + key + "'";
        }
        return false;
      }
    }

    if (next == std::string::npos) {
      break;
    }
    cursor = next + 1;
  }

  if (out_config->logic_lib.empty()) {
    if (error_out) {
      *error_out = "Config is missing logic_lib";
    }
    return false;
  }

  if (!out_config->mapping_file.empty() &&
      (!out_config->input_item.empty() || !out_config->output_item.empty() ||
       !out_config->input_bindings.empty() || !out_config->output_bindings.empty())) {
    if (error_out) {
      *error_out =
        "Config may not mix mapping_file with input_item/output_item or input_bindings/output_bindings";
    }
    return false;
  }

  if (!out_config->input_item.empty() && !out_config->input_bindings.empty()) {
    if (error_out) {
      *error_out = "Config may not set both input_item and input_bindings";
    }
    return false;
  }

  if (!out_config->output_item.empty() && !out_config->output_bindings.empty()) {
    if (error_out) {
      *error_out = "Config may not set both output_item and output_bindings";
    }
    return false;
  }

  return true;
}

bool bindItemSpan(const std::string& item_name,
                  ecmcStrucppIoImageSpan* out_span,
                  ecmcDataItemInfo* out_info,
                  std::string* error_out) {
  if (!out_span) {
    if (error_out) {
      *error_out = "Output span pointer is null";
    }
    return false;
  }

  std::vector<char> mutable_name(item_name.begin(), item_name.end());
  mutable_name.push_back('\0');

  auto* item = static_cast<ecmcDataItem*>(getEcmcDataItem(mutable_name.data()));
  if (!item) {
    if (error_out) {
      *error_out = "Could not find ecmcDataItem '" + item_name + "'";
    }
    return false;
  }

  ecmcDataItemInfo* info = item->getDataItemInfo();
  if (!info || !info->dataPointerValid || !info->data || info->dataSize == 0) {
    if (error_out) {
      *error_out = "ecmcDataItem '" + item_name +
                   "' does not expose a valid data buffer";
    }
    return false;
  }

  out_span->data = info->data;
  out_span->size = info->dataSize;
  out_span->name = item_name;
  if (out_info) {
    *out_info = *info;
  }
  return true;
}

size_t requiredBytesForArea(const strucpp::LocatedVar* vars,
                            size_t count,
                            strucpp::LocatedArea area) {
  size_t required = 0;
  if (!vars) {
    return required;
  }

  for (size_t i = 0; i < count; ++i) {
    const auto& var = vars[i];
    if (var.area != area) {
      continue;
    }

    const size_t end = var.size == strucpp::LocatedSize::Bit
                         ? static_cast<size_t>(var.byte_index) + 1
                         : static_cast<size_t>(var.byte_index) + var.byte_size();
    required = std::max(required, end);
  }

  return required;
}

bool resolveBindingSpecs(const std::vector<ItemBindingSpec>& specs,
                         std::vector<BoundItemBinding>* out_bindings,
                         size_t* out_required_bytes,
                         std::string* error_out) {
  if (!out_bindings || !out_required_bytes) {
    if (error_out) {
      *error_out = "Binding outputs must not be null";
    }
    return false;
  }

  out_bindings->clear();
  *out_required_bytes = 0;

  for (const auto& spec : specs) {
    BoundItemBinding bound {};
    if (!bindItemSpan(spec.item_name, &bound.item, &bound.info, error_out)) {
      return false;
    }

    bound.offset = spec.offset;
    bound.bytes = spec.bytes == 0 ? bound.item.size : spec.bytes;

    if (bound.bytes == 0) {
      if (error_out) {
        *error_out = "Binding '" + spec.item_name + "' resolved to zero bytes";
      }
      return false;
    }

    if (bound.bytes > bound.item.size) {
      if (error_out) {
        std::ostringstream oss;
        oss << "Binding '" << spec.item_name << "' requests " << bound.bytes
            << " bytes, but the item only exposes " << bound.item.size
            << " bytes";
        *error_out = oss.str();
      }
      return false;
    }

    out_bindings->push_back(bound);
    *out_required_bytes = std::max(*out_required_bytes, bound.offset + bound.bytes);
  }

  std::sort(out_bindings->begin(),
            out_bindings->end(),
            [](const BoundItemBinding& lhs, const BoundItemBinding& rhs) {
              if (lhs.offset != rhs.offset) {
                return lhs.offset < rhs.offset;
              }
              return lhs.item.name < rhs.item.name;
            });

  for (size_t i = 1; i < out_bindings->size(); ++i) {
    const auto& prev = (*out_bindings)[i - 1];
    const auto& curr = (*out_bindings)[i];
    if (prev.offset + prev.bytes > curr.offset) {
      if (error_out) {
        std::ostringstream oss;
        oss << "Bindings '" << prev.item.name << "' and '" << curr.item.name
            << "' overlap in the virtual image";
        *error_out = oss.str();
      }
      return false;
    }
  }

  return true;
}

bool resolveAddressMappings(const std::vector<AddressMappingSpec>& specs,
                            std::vector<BoundAddressMapping>* out_bindings,
                            std::string* error_out) {
  if (!out_bindings) {
    if (error_out) {
      *error_out = "Output mapping vector is null";
    }
    return false;
  }

  out_bindings->clear();
  for (const auto& spec : specs) {
    BoundAddressMapping bound {};
    bound.address = spec.address;
    if (!bindItemSpan(spec.item_name, &bound.item, &bound.info, error_out)) {
      return false;
    }
    out_bindings->push_back(bound);
  }
  return true;
}

const BoundAddressMapping* findAddressMapping(const strucpp::LocatedVar& var,
                                              const std::vector<BoundAddressMapping>& bindings) {
  for (const auto& binding : bindings) {
    if (addressesEqual(var, binding.address)) {
      return &binding;
    }
  }
  return nullptr;
}

bool bindingCoversLocatedVars(const strucpp::LocatedVar* vars,
                              size_t count,
                              strucpp::LocatedArea area,
                              size_t image_size,
                              const std::vector<BoundItemBinding>& bindings,
                              std::string* error_out) {
  std::vector<uint8_t> coverage(image_size, 0);
  for (const auto& binding : bindings) {
    for (size_t i = 0; i < binding.bytes; ++i) {
      coverage[binding.offset + i] = 1;
    }
  }

  for (size_t i = 0; i < count; ++i) {
    const auto& var = vars[i];
    if (var.area != area) {
      continue;
    }

    const size_t byte_count =
      var.size == strucpp::LocatedSize::Bit ? 1 : var.byte_size();

    for (size_t byte = 0; byte < byte_count; ++byte) {
      const size_t image_byte = static_cast<size_t>(var.byte_index) + byte;
      if (image_byte >= coverage.size() || coverage[image_byte] == 0) {
        if (error_out) {
          *error_out = "No direct binding covers " + formatAddress(var);
        }
        return false;
      }
    }
  }

  return true;
}

bool validateBindingDirections(const std::vector<BoundItemBinding>& bindings,
                               strucpp::LocatedArea area,
                               std::string* error_out) {
  for (const auto& binding : bindings) {
    if (!validateDirectionForArea(area, binding.info, binding.item.name, error_out)) {
      return false;
    }
  }
  return true;
}

bool appendDirectMappedVar(const strucpp::LocatedVar& var,
                           const std::vector<BoundAddressMapping>& mappings,
                           const ecmcStrucppIoImageSpan& memory_span,
                           ecmcStrucppCompiledCopyPlan* out_plan,
                           std::string* error_out) {
  if (!out_plan) {
    if (error_out) {
      *error_out = "Copy plan output pointer is null";
    }
    return false;
  }

  if (!var.pointer) {
    if (error_out) {
      *error_out = "Located variable " + formatAddress(var) +
                   " is missing a storage pointer";
    }
    return false;
  }

  if (var.area == strucpp::LocatedArea::Memory) {
    if (!memory_span.data) {
      if (error_out) {
        *error_out = "No memory image configured for " + formatAddress(var);
      }
      return false;
    }

    const uint8_t* const image_ptr = memory_span.data + var.byte_index;
    if (var.size == strucpp::LocatedSize::Bit) {
      if (var.byte_index >= memory_span.size || var.bit_index > 7) {
        if (error_out) {
          *error_out = formatAddress(var) + " is outside memory image";
        }
        return false;
      }

      const uint8_t bit_mask = static_cast<uint8_t>(1U << var.bit_index);
      out_plan->memory_bits_to_var.push_back(
        {image_ptr, bit_mask, static_cast<bool*>(var.pointer)});
      out_plan->memory_bits_from_var.push_back({const_cast<uint8_t*>(image_ptr),
                                                bit_mask,
                                                static_cast<const bool*>(var.pointer)});
      return true;
    }

    if (var.byte_index + var.byte_size() > memory_span.size) {
      if (error_out) {
        *error_out = formatAddress(var) + " is outside memory image";
      }
      return false;
    }

    out_plan->memory_scalars_to_var.push_back({image_ptr, var.pointer, var.byte_size()});
    out_plan->memory_scalars_from_var.push_back(
      {const_cast<uint8_t*>(image_ptr), var.pointer, var.byte_size()});
    return true;
  }

  const BoundAddressMapping* const mapping = findAddressMapping(var, mappings);
  if (!mapping) {
    if (error_out) {
      *error_out = "No mapping entry for " + formatAddress(var);
    }
    return false;
  }

  if (!validateDirectMappedItemType(var, mapping->info, mapping->item.name, error_out)) {
    return false;
  }

  if (var.size == strucpp::LocatedSize::Bit) {
    if (mapping->item.size < 1 || var.bit_index > 7) {
      if (error_out) {
        *error_out = "Mapped item '" + mapping->item.name +
                     "' cannot satisfy bit address " + formatAddress(var);
      }
      return false;
    }

    const uint8_t bit_mask = static_cast<uint8_t>(1U << var.bit_index);
    if (var.area == strucpp::LocatedArea::Input) {
      out_plan->input_bits_to_var.push_back(
        {mapping->item.data, bit_mask, static_cast<bool*>(var.pointer)});
    } else {
      out_plan->output_bits_from_var.push_back({mapping->item.data,
                                                bit_mask,
                                                static_cast<const bool*>(var.pointer)});
    }
    return true;
  }

  const size_t byte_size = var.byte_size();
  if (mapping->item.size < byte_size) {
    if (error_out) {
      std::ostringstream oss;
      oss << "Mapped item '" << mapping->item.name << "' exposes "
          << mapping->item.size << " bytes but " << formatAddress(var)
          << " needs " << byte_size;
      *error_out = oss.str();
    }
    return false;
  }

  if (var.area == strucpp::LocatedArea::Input) {
    out_plan->input_scalars_to_var.push_back(
      {mapping->item.data, var.pointer, byte_size});
  } else {
    out_plan->output_scalars_from_var.push_back(
      {mapping->item.data, var.pointer, byte_size});
  }
  return true;
}

bool buildDirectMappedCopyPlan(const strucpp::LocatedVar* vars,
                               size_t count,
                               const std::vector<BoundAddressMapping>& mappings,
                               const ecmcStrucppIoImageSpan& memory_span,
                               ecmcStrucppCompiledCopyPlan* out_plan,
                               std::string* error_out) {
  if (!vars) {
    if (error_out) {
      *error_out = "Located variable table is null";
    }
    return false;
  }

  if (!out_plan) {
    if (error_out) {
      *error_out = "Copy plan output pointer is null";
    }
    return false;
  }

  *out_plan = ecmcStrucppCompiledCopyPlan {};
  for (size_t i = 0; i < count; ++i) {
    if (!appendDirectMappedVar(vars[i], mappings, memory_span, out_plan, error_out)) {
      return false;
    }
  }
  return true;
}

void zeroImage(ecmcStrucppIoImageSpan* span) {
  if (!span || !span->data || span->size == 0) {
    return;
  }
  std::memset(span->data, 0, span->size);
}

void gatherBindings(const std::vector<BoundItemBinding>& bindings,
                    ecmcStrucppIoImageSpan* target_span) {
  if (!target_span || !target_span->data) {
    return;
  }

  for (const auto& binding : bindings) {
    std::memcpy(target_span->data + binding.offset, binding.item.data, binding.bytes);
  }
}

void scatterBindings(const ecmcStrucppIoImageSpan& source_span,
                     const std::vector<BoundItemBinding>& bindings) {
  if (!source_span.data) {
    return;
  }

  for (const auto& binding : bindings) {
    std::memcpy(binding.item.data, source_span.data + binding.offset, binding.bytes);
  }
}

std::string describeBindingSpecs(const std::vector<ItemBindingSpec>& specs) {
  if (specs.empty()) {
    return "<none>";
  }

  std::ostringstream oss;
  for (size_t i = 0; i < specs.size(); ++i) {
    if (i != 0) {
      oss << ",";
    }
    oss << specs[i].offset << ":" << specs[i].item_name;
    if (specs[i].bytes != 0) {
      oss << "@" << specs[i].bytes;
    }
  }
  return oss.str();
}

void clearRuntimeState() {
  g_images = ecmcStrucppIoImages {};
  g_copy_plan = ecmcStrucppCompiledCopyPlan {};
  g_input_image.clear();
  g_output_image.clear();
  g_memory_image.clear();
  g_bound_input_bindings.clear();
  g_bound_output_bindings.clear();
  g_mapping_specs.clear();
  g_bound_mappings.clear();
  g_exported_params.clear();
}

void unloadLogicRuntime() {
  if (g_logic.api && g_logic.instance && g_logic.api->destroy_instance) {
    g_logic.api->destroy_instance(g_logic.instance);
  }

  if (g_logic.dl_handle) {
    dlclose(g_logic.dl_handle);
  }

  g_logic = LogicRuntime {};
}

bool loadLogicRuntime(const std::string& path, std::string* error_out) {
  unloadLogicRuntime();
  dlerror();

  g_logic.dl_handle = dlopen(path.c_str(), RTLD_NOW);
  if (!g_logic.dl_handle) {
    if (error_out) {
      *error_out = std::string("dlopen failed for '") + path + "': " + dlerror();
    }
    return false;
  }

  using GetApiFunc = const ecmcStrucppLogicApi* (*)();
  auto* get_api = reinterpret_cast<GetApiFunc>(
    dlsym(g_logic.dl_handle, "ecmc_strucpp_logic_get_api"));

  if (!get_api) {
    if (error_out) {
      *error_out =
        std::string("Logic library is missing ecmc_strucpp_logic_get_api: ") +
        dlerror();
    }
    unloadLogicRuntime();
    return false;
  }

  g_logic.api = get_api();
  if (!g_logic.api) {
    if (error_out) {
      *error_out = "Logic API pointer is null";
    }
    unloadLogicRuntime();
    return false;
  }

  if (g_logic.api->abi_version != ECMC_STRUCPP_LOGIC_ABI_VERSION) {
    if (error_out) {
      *error_out = "Logic ABI version mismatch";
    }
    unloadLogicRuntime();
    return false;
  }

  if (!g_logic.api->create_instance || !g_logic.api->destroy_instance ||
      !g_logic.api->run_cycle || !g_logic.api->get_located_vars ||
      !g_logic.api->get_located_var_count || !g_logic.api->get_exported_vars ||
      !g_logic.api->get_exported_var_count) {
    if (error_out) {
      *error_out = "Logic API is incomplete";
    }
    unloadLogicRuntime();
    return false;
  }

  g_logic.instance = g_logic.api->create_instance();
  if (!g_logic.instance) {
    if (error_out) {
      *error_out = "Logic create_instance() returned null";
    }
    unloadLogicRuntime();
    return false;
  }

  return true;
}

const strucpp::LocatedVar* logicLocatedVars() {
  if (!g_logic.api || !g_logic.instance) {
    return nullptr;
  }
  return g_logic.api->get_located_vars(g_logic.instance);
}

size_t logicLocatedVarCount() {
  if (!g_logic.api || !g_logic.instance) {
    return 0;
  }
  return static_cast<size_t>(g_logic.api->get_located_var_count(g_logic.instance));
}

static int construct(char* configStr) {
  if (alreadyLoaded) {
    logError("plugin already loaded and currently supports one host instance");
    return -1;
  }

  std::string error;
  if (!parseConfigString(configStr, &g_config, &error)) {
    logError("%s", error.c_str());
    return -1;
  }

  if (!loadLogicRuntime(g_config.logic_lib, &error)) {
    logError("%s", error.c_str());
    return -1;
  }

  if (!ensureAsynPort(&error)) {
    unloadLogicRuntime();
    logError("%s", error.c_str());
    return -1;
  }

  alreadyLoaded = 1;
  const std::string input_bindings = describeBindingSpecs(g_config.input_bindings);
  const std::string output_bindings = describeBindingSpecs(g_config.output_bindings);
  logInfo("configured logic_lib=%s asyn_port=%s mapping_file=%s input_item=%s input_bindings=%s output_item=%s output_bindings=%s memory_bytes=%zu",
          g_config.logic_lib.c_str(),
          g_config.asyn_port_name.c_str(),
          g_config.mapping_file.empty() ? "<none>" : g_config.mapping_file.c_str(),
          g_config.input_item.empty() ? "<none>" : g_config.input_item.c_str(),
          input_bindings.c_str(),
          g_config.output_item.empty() ? "<none>" : g_config.output_item.c_str(),
          output_bindings.c_str(),
          g_config.memory_bytes);
  return 0;
}

static void destruct(void) {
  unloadLogicRuntime();
  clearRuntimeState();
  destroyAsynPort();
  g_config = PluginConfig {};
  alreadyLoaded = 0;
}

static int enterRealtime(void) {
  std::string error;
  ecmcDataItemInfo image_info {};

  const strucpp::LocatedVar* const vars = logicLocatedVars();
  const size_t var_count = logicLocatedVarCount();
  const size_t required_input_bytes =
    requiredBytesForArea(vars, var_count, strucpp::LocatedArea::Input);
  const size_t required_output_bytes =
    requiredBytesForArea(vars, var_count, strucpp::LocatedArea::Output);
  const size_t required_memory_bytes =
    requiredBytesForArea(vars, var_count, strucpp::LocatedArea::Memory);

  clearRuntimeState();

  g_memory_image.assign(std::max(g_config.memory_bytes, required_memory_bytes), 0);
  g_images.memory.data = g_memory_image.data();
  g_images.memory.size = g_memory_image.size();
  g_images.memory.name = "plugin_memory";

  if (!g_config.mapping_file.empty()) {
    if (!loadMappingFile(g_config.mapping_file, &g_mapping_specs, &error)) {
      logError("%s", error.c_str());
      return -1;
    }

    if (!resolveAddressMappings(g_mapping_specs, &g_bound_mappings, &error)) {
      logError("%s", error.c_str());
      return -1;
    }

    if (!buildDirectMappedCopyPlan(vars,
                                   var_count,
                                   g_bound_mappings,
                                   g_images.memory,
                                   &g_copy_plan,
                                   &error)) {
      logError("%s", error.c_str());
      return -1;
    }

    g_images.input.name = "direct_mapping";
    g_images.output.name = "direct_mapping";
    logInfo("bound logic=%s using mapping_file=%s with %zu direct mappings, memory=%zu bytes",
            g_logic.api->name ? g_logic.api->name : "<unnamed>",
            g_config.mapping_file.c_str(),
            g_bound_mappings.size(),
            g_images.memory.size);
    return 0;
  }

  if (!g_config.input_bindings.empty()) {
    size_t binding_bytes = 0;
    if (!resolveBindingSpecs(g_config.input_bindings,
                             &g_bound_input_bindings,
                             &binding_bytes,
                             &error)) {
      logError("%s", error.c_str());
      return -1;
    }
    if (!validateBindingDirections(g_bound_input_bindings,
                                   strucpp::LocatedArea::Input,
                                   &error)) {
      logError("%s", error.c_str());
      return -1;
    }
    g_input_image.assign(std::max(required_input_bytes, binding_bytes), 0);
    g_images.input.data = g_input_image.empty() ? nullptr : g_input_image.data();
    g_images.input.size = g_input_image.size();
    g_images.input.name = "virtual_input_image";
    if (!bindingCoversLocatedVars(vars,
                                  var_count,
                                  strucpp::LocatedArea::Input,
                                  g_images.input.size,
                                  g_bound_input_bindings,
                                  &error)) {
      logError("%s", error.c_str());
      return -1;
    }
  } else if (!g_config.input_item.empty()) {
    if (!bindItemSpan(g_config.input_item, &g_images.input, &image_info, &error)) {
      logError("%s", error.c_str());
      return -1;
    }
    if (!validateDirectionForArea(strucpp::LocatedArea::Input,
                                  image_info,
                                  g_images.input.name,
                                  &error)) {
      logError("%s", error.c_str());
      return -1;
    }
  } else if (required_input_bytes != 0) {
    logError("Logic requires %zu input bytes, but neither input_item, input_bindings, nor mapping_file were configured",
             required_input_bytes);
    return -1;
  }

  if (!g_config.output_bindings.empty()) {
    size_t binding_bytes = 0;
    if (!resolveBindingSpecs(g_config.output_bindings,
                             &g_bound_output_bindings,
                             &binding_bytes,
                             &error)) {
      logError("%s", error.c_str());
      return -1;
    }
    if (!validateBindingDirections(g_bound_output_bindings,
                                   strucpp::LocatedArea::Output,
                                   &error)) {
      logError("%s", error.c_str());
      return -1;
    }
    g_output_image.assign(std::max(required_output_bytes, binding_bytes), 0);
    g_images.output.data = g_output_image.empty() ? nullptr : g_output_image.data();
    g_images.output.size = g_output_image.size();
    g_images.output.name = "virtual_output_image";
    if (!bindingCoversLocatedVars(vars,
                                  var_count,
                                  strucpp::LocatedArea::Output,
                                  g_images.output.size,
                                  g_bound_output_bindings,
                                  &error)) {
      logError("%s", error.c_str());
      return -1;
    }
  } else if (!g_config.output_item.empty()) {
    if (!bindItemSpan(g_config.output_item, &g_images.output, &image_info, &error)) {
      logError("%s", error.c_str());
      return -1;
    }
    if (!validateDirectionForArea(strucpp::LocatedArea::Output,
                                  image_info,
                                  g_images.output.name,
                                  &error)) {
      logError("%s", error.c_str());
      return -1;
    }
  } else if (required_output_bytes != 0) {
    logError("Logic requires %zu output bytes, but neither output_item, output_bindings, nor mapping_file were configured",
             required_output_bytes);
    return -1;
  }

  if (!ecmcStrucppBuildCopyPlan(vars, var_count, g_images, &g_copy_plan, &error)) {
    logError("%s", error.c_str());
    return -1;
  }

  if (!bindExportedVars(&error)) {
    logError("%s", error.c_str());
    return -1;
  }

  logInfo("bound logic=%s input=%s (%zu bytes) output=%s (%zu bytes) memory=%zu bytes",
          g_logic.api->name ? g_logic.api->name : "<unnamed>",
          g_images.input.name.empty() ? "<none>" : g_images.input.name.c_str(),
          g_images.input.size,
          g_images.output.name.empty() ? "<none>" : g_images.output.name.c_str(),
          g_images.output.size,
          g_images.memory.size);
  if (!g_exported_params.empty()) {
    logInfo("published %zu ST variables to asyn", g_exported_params.size());
  }
  return 0;
}

static int exitRealtime(void) {
  clearRuntimeState();
  return 0;
}

static int realtime(int) {
  if (!g_bound_input_bindings.empty()) {
    zeroImage(&g_images.input);
    gatherBindings(g_bound_input_bindings, &g_images.input);
  }

  if (!g_bound_output_bindings.empty()) {
    zeroImage(&g_images.output);
    gatherBindings(g_bound_output_bindings, &g_images.output);
  }

  ecmcStrucppExecuteInputCopyPlan(g_copy_plan);
  ecmcStrucppExecuteMemoryPreCopyPlan(g_copy_plan);

  g_logic.api->run_cycle(g_logic.instance);

  ecmcStrucppExecuteMemoryPostCopyPlan(g_copy_plan);
  ecmcStrucppExecuteOutputCopyPlan(g_copy_plan);

  if (!g_bound_output_bindings.empty()) {
    scatterBindings(g_images.output, g_bound_output_bindings);
  }

  for (auto& exported_param : g_exported_params) {
    if (exported_param.param) {
      exported_param.param->refreshParamRT(1);
    }
  }
  return 0;
}

static struct ecmcPluginData pluginDataDef = {
  .ifVersion = ECMC_PLUG_VERSION_MAGIC,
  .name = "ecmc_plugin_strucpp",
  .desc = "Generic ecmc host plugin for loadable STruCpp logic libraries.",
  .optionDesc =
    "logic_lib=<path>;asyn_port=<plugin-port>;[mapping_file=<path>|input_item=<name>|input_bindings=<offset:item[@bytes],...>];"
    "[mapping_file=<path>|output_item=<name>|output_bindings=<offset:item[@bytes],...>];memory_bytes=<n>\n"
    "PLC consts: STRUCPP_AREA_I=0, STRUCPP_AREA_Q=1, STRUCPP_AREA_M=2\n"
    "PLC funcs: strucpp_get_bit(a,b,bit), strucpp_set_bit(a,b,bit,v), "
    "strucpp_get_u8(a,b), strucpp_set_u8(a,b,v), strucpp_get_s8(a,b), strucpp_set_s8(a,b,v), "
    "strucpp_get_u16(a,b), strucpp_set_u16(a,b,v), strucpp_get_s16(a,b), strucpp_set_s16(a,b,v), "
    "strucpp_get_u32(a,b), strucpp_set_u32(a,b,v), strucpp_get_s32(a,b), strucpp_set_s32(a,b,v), "
    "strucpp_get_f32(a,b), strucpp_set_f32(a,b,v), "
    "strucpp_get_f64(a,b), strucpp_set_f64(a,b,v)",
  .version = ECMC_PLUGIN_VERSION,
  .constructFnc = construct,
  .destructFnc = destruct,
  .realtimeEnterFnc = enterRealtime,
  .realtimeExitFnc = exitRealtime,
  .realtimeFnc = realtime,
  .funcs[0] = {
    .funcName = "strucpp_get_bit",
    .funcDesc = "Read one bit from the plugin image area: area(0=I,1=Q,2=M), byte_offset, bit_index.",
    .funcArg3 = plcGetBit,
  },
  .funcs[1] = {
    .funcName = "strucpp_set_bit",
    .funcDesc = "Write one bit in the plugin image area: area(0=I,1=Q,2=M), byte_offset, bit_index, value.",
    .funcArg4 = plcSetBit,
  },
  .funcs[2] = {
    .funcName = "strucpp_get_u8",
    .funcDesc = "Read one unsigned byte from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetU8,
  },
  .funcs[3] = {
    .funcName = "strucpp_set_u8",
    .funcDesc = "Write one unsigned byte in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetU8,
  },
  .funcs[4] = {
    .funcName = "strucpp_get_s8",
    .funcDesc = "Read one signed byte from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetS8,
  },
  .funcs[5] = {
    .funcName = "strucpp_set_s8",
    .funcDesc = "Write one signed byte in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetS8,
  },
  .funcs[6] = {
    .funcName = "strucpp_get_u16",
    .funcDesc = "Read one unsigned 16-bit word from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetU16,
  },
  .funcs[7] = {
    .funcName = "strucpp_set_u16",
    .funcDesc = "Write one unsigned 16-bit word in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetU16,
  },
  .funcs[8] = {
    .funcName = "strucpp_get_s16",
    .funcDesc = "Read one signed 16-bit word from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetS16,
  },
  .funcs[9] = {
    .funcName = "strucpp_set_s16",
    .funcDesc = "Write one signed 16-bit word in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetS16,
  },
  .funcs[10] = {
    .funcName = "strucpp_get_u32",
    .funcDesc = "Read one unsigned 32-bit value from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetU32,
  },
  .funcs[11] = {
    .funcName = "strucpp_set_u32",
    .funcDesc = "Write one unsigned 32-bit value in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetU32,
  },
  .funcs[12] = {
    .funcName = "strucpp_get_s32",
    .funcDesc = "Read one signed 32-bit value from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetS32,
  },
  .funcs[13] = {
    .funcName = "strucpp_set_s32",
    .funcDesc = "Write one signed 32-bit value in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetS32,
  },
  .funcs[14] = {
    .funcName = "strucpp_get_f32",
    .funcDesc = "Read one 32-bit float from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetF32,
  },
  .funcs[15] = {
    .funcName = "strucpp_set_f32",
    .funcDesc = "Write one 32-bit float in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetF32,
  },
  .funcs[16] = {
    .funcName = "strucpp_get_f64",
    .funcDesc = "Read one 64-bit float from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
    .funcArg2 = plcGetF64,
  },
  .funcs[17] = {
    .funcName = "strucpp_set_f64",
    .funcDesc = "Write one 64-bit float in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
    .funcArg3 = plcSetF64,
  },
  .funcs[18] = {0},
  .consts[0] = {
    .constName = "STRUCPP_AREA_I",
    .constDesc = "Input image area selector for strucpp exprtk helper functions.",
    .constValue = kPlcAreaInput,
  },
  .consts[1] = {
    .constName = "STRUCPP_AREA_Q",
    .constDesc = "Output image area selector for strucpp exprtk helper functions.",
    .constValue = kPlcAreaOutput,
  },
  .consts[2] = {
    .constName = "STRUCPP_AREA_M",
    .constDesc = "Memory image area selector for strucpp exprtk helper functions.",
    .constValue = kPlcAreaMemory,
  },
  .consts[3] = {0},
};

ecmc_plugin_register(pluginDataDef);

}  // namespace
