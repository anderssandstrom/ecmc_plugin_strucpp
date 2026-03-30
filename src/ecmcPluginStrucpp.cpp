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

#include "asynPortDriver.h"
#include "ecmcDataItem.h"
#include "ecmcPluginClient.h"
#include "ecmcPluginDefs.h"

#include <pthread.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <sstream>
#include <set>
#include <string>
#include <sys/resource.h>
#include <time.h>
#if defined(__linux__)
#  include <sys/syscall.h>
#endif
#include <thread>
#include <unistd.h>
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
  double sample_rate_ms {0.0};
};

struct LogicRuntime {
  void* dl_handle {};
  const ecmcStrucppLogicApi* api {};
  void* instance {};
};

struct ExportedParamBinding {
  std::string name;
  int param_id {-1};
  uint32_t type {};
  uint32_t writable {};
  void* data {};
  size_t bytes {};
  std::vector<uint8_t> last_value;
  bool initialized {false};
};

struct GroupedBoolMember {
  void* data {};
  uint32_t bit_index {};
};

struct GroupedBoolParamBinding {
  std::string name;
  int param_id {-1};
  uint32_t writable {};
  uint32_t mask {0xFFFFFFFFu};
  std::vector<GroupedBoolMember> members;
  uint32_t last_value {};
  bool initialized {false};
};

ExportedParamBinding* paramBindingForReason(int reason);
GroupedBoolParamBinding* groupedBoolBindingForReason(int reason);
bool applyRuntimeSampleRateMs(double requested_ms, std::string* error_out);
bool syncBuiltinParams(bool force, bool defer_callbacks);
bool createBoundParam(const char* name,
                      uint32_t type,
                      uint32_t writable,
                      void* data,
                      std::vector<ExportedParamBinding>* out_bindings,
                      std::string* error_out);
void applyControlWord(uint32_t control_word);
void logError(const char* fmt, ...);

extern int32_t g_control_word;
extern bool g_execute_count_publish_due;

constexpr double kPlcAreaInput = 0.0;
constexpr double kPlcAreaOutput = 1.0;
constexpr double kPlcAreaMemory = 2.0;
constexpr const char* kDefaultAsynPortName = "PLUGIN.STRUCPP0";
constexpr const char* kBuiltinControlWordName = "plugin.strucpp0.ctrl.word";
constexpr const char* kBuiltinSampleRateName = "plugin.strucpp0.ctrl.rate_ms";
constexpr const char* kBuiltinActualSampleRateName = "plugin.strucpp0.stat.rate_ms";
constexpr const char* kBuiltinLastExecTimeName = "plugin.strucpp0.stat.exec_ms";
constexpr const char* kBuiltinLastTotalTimeName = "plugin.strucpp0.stat.total_ms";
constexpr const char* kBuiltinExecuteDividerName = "plugin.strucpp0.stat.div";
constexpr const char* kBuiltinExecuteCountName = "plugin.strucpp0.stat.count";
constexpr uint32_t kControlWordEnableExecutionBit =
  ECMC_STRUCPP_CONTROL_WORD_ENABLE_EXECUTION_BIT;
constexpr uint32_t kControlWordMeasureExecTimeBit =
  ECMC_STRUCPP_CONTROL_WORD_MEASURE_EXEC_TIME_BIT;
constexpr uint32_t kControlWordEnableDebugPrintsBit =
  ECMC_STRUCPP_CONTROL_WORD_ENABLE_DEBUG_PRINTS_BIT;
constexpr uint32_t kControlWordMeasureTotalTimeBit =
  ECMC_STRUCPP_CONTROL_WORD_MEASURE_TOTAL_TIME_BIT;

class StrucppAsynPort : public asynPortDriver {
 public:
  explicit StrucppAsynPort(const char* port_name)
    : asynPortDriver(port_name,
                     1,
                     asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask |
                       asynDrvUserMask,
                     asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask |
                     asynDrvUserMask,
                     0,
                     1,
                     0,
                     0) {
    callback_thread_ = std::thread([this]() { callbackWorker(); });
  }

  ~StrucppAsynPort() override {
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      stop_callback_thread_ = true;
      callback_pending_ = true;
    }
    callback_cv_.notify_one();
    if (callback_thread_.joinable()) {
      callback_thread_.join();
    }
  }

  bool syncExportedParams(std::vector<ExportedParamBinding>* bindings,
                          bool force,
                          bool defer_callbacks) {
    if (!bindings) {
      return false;
    }

    bool any_changed = false;
    for (auto& binding : *bindings) {
      if (syncOneParam(&binding, force)) {
        any_changed = true;
      }
    }

    if (any_changed) {
      if (defer_callbacks) {
        scheduleCallbacks();
      } else {
        callParamCallbacks();
      }
    }
    return true;
  }

  bool syncGroupedBoolParams(std::vector<GroupedBoolParamBinding>* bindings,
                             bool force,
                             bool defer_callbacks) {
    if (!bindings) {
      return false;
    }

    bool any_changed = false;
    for (auto& binding : *bindings) {
      if (syncOneGroupedBoolParam(&binding, force)) {
        any_changed = true;
      }
    }

    if (any_changed) {
      if (defer_callbacks) {
        scheduleCallbacks();
      } else {
        callParamCallbacks();
      }
    }
    return true;
  }

  asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value) override {
    ExportedParamBinding* const binding = bindingForReason(pasynUser ? pasynUser->reason : -1);
    if (!binding || binding->writable == 0 || !binding->data) {
      return asynError;
    }

    if (binding->name == kBuiltinControlWordName) {
      const uint32_t control_word = static_cast<uint32_t>(value);
      applyControlWord(control_word);
      binding->last_value.assign(reinterpret_cast<const uint8_t*>(&g_control_word),
                                 reinterpret_cast<const uint8_t*>(&g_control_word) + sizeof(g_control_word));
      binding->initialized = true;
      setIntegerParam(binding->param_id, g_control_word);
      syncBuiltinParams(false, false);
      return asynSuccess;
    }

    switch (binding->type) {
    case ECMC_STRUCPP_EXPORT_BOOL: {
      const uint8_t typed = value != 0 ? 1u : 0u;
      return writeScalar(binding, &typed, sizeof(typed), static_cast<epicsInt32>(typed));
    }
    case ECMC_STRUCPP_EXPORT_S8: {
      const int8_t typed = static_cast<int8_t>(value);
      return writeScalar(binding, &typed, sizeof(typed), static_cast<epicsInt32>(typed));
    }
    case ECMC_STRUCPP_EXPORT_U8: {
      const uint8_t typed = static_cast<uint8_t>(value);
      return writeScalar(binding, &typed, sizeof(typed), static_cast<epicsInt32>(typed));
    }
    case ECMC_STRUCPP_EXPORT_S16: {
      const int16_t typed = static_cast<int16_t>(value);
      return writeScalar(binding, &typed, sizeof(typed), static_cast<epicsInt32>(typed));
    }
    case ECMC_STRUCPP_EXPORT_U16: {
      const uint16_t typed = static_cast<uint16_t>(value);
      return writeScalar(binding, &typed, sizeof(typed), static_cast<epicsInt32>(typed));
    }
    case ECMC_STRUCPP_EXPORT_S32: {
      const int32_t typed = static_cast<int32_t>(value);
      return writeScalar(binding, &typed, sizeof(typed), static_cast<epicsInt32>(typed));
    }
    default:
      return asynError;
    }
  }

  asynStatus writeUInt32Digital(asynUser* pasynUser,
                                epicsUInt32 value,
                                epicsUInt32 mask) override {
    GroupedBoolParamBinding* const grouped_binding =
      groupedBoolBindingForReason(pasynUser ? pasynUser->reason : -1);
    if (grouped_binding) {
      if (grouped_binding->writable == 0) {
        return asynError;
      }

      uint32_t typed = grouped_binding->initialized ? grouped_binding->last_value : 0u;
      typed = (typed & ~static_cast<uint32_t>(mask)) | (static_cast<uint32_t>(value) & static_cast<uint32_t>(mask));
      if (!writeGroupedBool(grouped_binding, typed)) {
        return asynError;
      }
      setUIntDigitalParam(grouped_binding->param_id, typed, grouped_binding->mask);
      callParamCallbacks();
      return asynSuccess;
    }

    ExportedParamBinding* const binding = bindingForReason(pasynUser ? pasynUser->reason : -1);
    if (!binding || binding->writable == 0 || !binding->data) {
      return asynError;
    }

    if (binding->type != ECMC_STRUCPP_EXPORT_U32) {
      return asynError;
    }

    uint32_t typed = 0;
    std::memcpy(&typed, binding->data, sizeof(typed));
    typed = (typed & ~mask) | (static_cast<uint32_t>(value) & mask);
    if (binding->initialized &&
        binding->last_value.size() == sizeof(typed) &&
        std::memcmp(binding->last_value.data(), &typed, sizeof(typed)) == 0) {
      return asynSuccess;
    }
    std::memcpy(binding->data, &typed, sizeof(typed));

    binding->last_value.assign(reinterpret_cast<const uint8_t*>(&typed),
                               reinterpret_cast<const uint8_t*>(&typed) + sizeof(typed));
    binding->initialized = true;
    setUIntDigitalParam(binding->param_id, typed, 0xFFFFFFFFu);
    callParamCallbacks();
    return asynSuccess;
  }

  asynStatus writeFloat64(asynUser* pasynUser, epicsFloat64 value) override {
    ExportedParamBinding* const binding = bindingForReason(pasynUser ? pasynUser->reason : -1);
    if (!binding || binding->writable == 0 || !binding->data) {
      return asynError;
    }

    if (binding->name == kBuiltinSampleRateName) {
      std::string error;
      if (!applyRuntimeSampleRateMs(static_cast<double>(value), &error)) {
        logError("%s", error.c_str());
        return asynError;
      }
      syncBuiltinParams(false, false);
      return asynSuccess;
    }

    switch (binding->type) {
    case ECMC_STRUCPP_EXPORT_F32: {
      const float typed = static_cast<float>(value);
      if (binding->initialized &&
          binding->last_value.size() == sizeof(typed) &&
          std::memcmp(binding->last_value.data(), &typed, sizeof(typed)) == 0) {
        return asynSuccess;
      }
      std::memcpy(binding->data, &typed, sizeof(typed));
      binding->last_value.assign(reinterpret_cast<const uint8_t*>(&typed),
                                 reinterpret_cast<const uint8_t*>(&typed) + sizeof(typed));
      binding->initialized = true;
      setDoubleParam(binding->param_id, static_cast<double>(typed));
      callParamCallbacks();
      return asynSuccess;
    }
    case ECMC_STRUCPP_EXPORT_F64: {
      const double typed = static_cast<double>(value);
      if (binding->initialized &&
          binding->last_value.size() == sizeof(typed) &&
          std::memcmp(binding->last_value.data(), &typed, sizeof(typed)) == 0) {
        return asynSuccess;
      }
      std::memcpy(binding->data, &typed, sizeof(typed));
      binding->last_value.assign(reinterpret_cast<const uint8_t*>(&typed),
                                 reinterpret_cast<const uint8_t*>(&typed) + sizeof(typed));
      binding->initialized = true;
      setDoubleParam(binding->param_id, typed);
      callParamCallbacks();
      return asynSuccess;
    }
    default:
      return asynError;
    }
  }

 private:
  ExportedParamBinding* bindingForReason(int reason) {
    return paramBindingForReason(reason);
  }

  asynStatus writeScalar(ExportedParamBinding* binding,
                         const void* value,
                         size_t bytes,
                         epicsInt32 readback) {
    if (!binding || !value || !binding->data || binding->bytes != bytes) {
      return asynError;
    }

    if (binding->initialized &&
        binding->last_value.size() == bytes &&
        std::memcmp(binding->last_value.data(), value, bytes) == 0) {
      return asynSuccess;
    }

    std::memcpy(binding->data, value, bytes);
    const auto* byte_value = static_cast<const uint8_t*>(value);
    binding->last_value.assign(byte_value, byte_value + bytes);
    binding->initialized = true;
    setIntegerParam(binding->param_id, readback);
    callParamCallbacks();
    return asynSuccess;
  }

  bool syncOneParam(ExportedParamBinding* binding, bool force) {
    if (!binding || binding->param_id < 0 || !binding->data || binding->bytes == 0) {
      return false;
    }

    if (!force && binding->name == kBuiltinExecuteCountName &&
        !g_execute_count_publish_due) {
      return false;
    }

    const auto* current = static_cast<const uint8_t*>(binding->data);
    if (!force && binding->initialized &&
        binding->last_value.size() == binding->bytes &&
        std::memcmp(binding->last_value.data(), current, binding->bytes) == 0) {
      return false;
    }

    binding->last_value.assign(current, current + binding->bytes);
    binding->initialized = true;

    switch (binding->type) {
    case ECMC_STRUCPP_EXPORT_BOOL: {
      const epicsInt32 value = current[0] != 0 ? 1 : 0;
      setIntegerParam(binding->param_id, value);
      return true;
    }
    case ECMC_STRUCPP_EXPORT_S8: {
      int8_t value = 0;
      std::memcpy(&value, current, sizeof(value));
      setIntegerParam(binding->param_id, static_cast<epicsInt32>(value));
      return true;
    }
    case ECMC_STRUCPP_EXPORT_U8: {
      uint8_t value = 0;
      std::memcpy(&value, current, sizeof(value));
      setIntegerParam(binding->param_id, static_cast<epicsInt32>(value));
      return true;
    }
    case ECMC_STRUCPP_EXPORT_S16: {
      int16_t value = 0;
      std::memcpy(&value, current, sizeof(value));
      setIntegerParam(binding->param_id, static_cast<epicsInt32>(value));
      return true;
    }
    case ECMC_STRUCPP_EXPORT_U16: {
      uint16_t value = 0;
      std::memcpy(&value, current, sizeof(value));
      setIntegerParam(binding->param_id, static_cast<epicsInt32>(value));
      return true;
    }
    case ECMC_STRUCPP_EXPORT_S32: {
      int32_t value = 0;
      std::memcpy(&value, current, sizeof(value));
      setIntegerParam(binding->param_id, static_cast<epicsInt32>(value));
      return true;
    }
    case ECMC_STRUCPP_EXPORT_U32: {
      uint32_t value = 0;
      std::memcpy(&value, current, sizeof(value));
      setUIntDigitalParam(binding->param_id, value, 0xFFFFFFFFu);
      return true;
    }
    case ECMC_STRUCPP_EXPORT_F32: {
      float value = 0;
      std::memcpy(&value, current, sizeof(value));
      setDoubleParam(binding->param_id, static_cast<double>(value));
      return true;
    }
    case ECMC_STRUCPP_EXPORT_F64: {
      double value = 0;
      std::memcpy(&value, current, sizeof(value));
      setDoubleParam(binding->param_id, value);
      return true;
    }
    default:
      return false;
    }
  }

  bool syncOneGroupedBoolParam(GroupedBoolParamBinding* binding, bool force) {
    if (!binding || binding->param_id < 0) {
      return false;
    }

    uint32_t value = 0u;
    for (const auto& member : binding->members) {
      if (!member.data) {
        continue;
      }
      const auto* raw = static_cast<const uint8_t*>(member.data);
      if (raw[0] != 0u) {
        value |= (1u << member.bit_index);
      }
    }

    if (!force && binding->initialized && binding->last_value == value) {
      return false;
    }

    binding->last_value = value;
    binding->initialized = true;
    setUIntDigitalParam(binding->param_id, value, binding->mask);
    return true;
  }

  bool writeGroupedBool(GroupedBoolParamBinding* binding, uint32_t value) {
    if (!binding) {
      return false;
    }

    if (binding->initialized && binding->last_value == value) {
      return true;
    }

    for (const auto& member : binding->members) {
      if (!member.data) {
        return false;
      }
      auto* raw = static_cast<uint8_t*>(member.data);
      raw[0] = ((value >> member.bit_index) & 0x1u) ? 1u : 0u;
    }

    binding->last_value = value;
    binding->initialized = true;
    return true;
  }

  void scheduleCallbacks() {
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      callback_pending_ = true;
    }
    callback_cv_.notify_one();
  }

  void callbackWorker() {
    lowerCurrentThreadPriority();
    std::unique_lock<std::mutex> lock(callback_mutex_);
    for (;;) {
      callback_cv_.wait(lock, [this]() {
        return callback_pending_ || stop_callback_thread_;
      });

      if (stop_callback_thread_) {
        return;
      }

      callback_pending_ = false;
      lock.unlock();
      callParamCallbacks();
      lock.lock();
    }
  }

  static void lowerCurrentThreadPriority() {
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
#elif defined(__linux__)
    sched_param sched {};
    pthread_setschedparam(pthread_self(), SCHED_OTHER, &sched);
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10);
#else
    sched_param sched {};
    pthread_setschedparam(pthread_self(), SCHED_OTHER, &sched);
#endif
  }

  std::mutex callback_mutex_;
  std::condition_variable callback_cv_;
  std::thread callback_thread_;
  bool callback_pending_ {false};
  bool stop_callback_thread_ {false};
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
std::vector<ExportedParamBinding> g_builtin_params;
std::vector<ExportedParamBinding> g_exported_params;
std::vector<GroupedBoolParamBinding> g_grouped_bool_params;
StrucppAsynPort* g_asyn_port = nullptr;
size_t g_runtime_execute_divider = 1;
size_t g_execute_divider_counter = 0;
int32_t g_control_word = 1;
uint8_t g_execution_enabled = 1;
uint8_t g_measure_exec_time_enabled = 0;
uint8_t g_measure_total_time_enabled = 0;
double g_requested_sample_rate_ms = 0.0;
double g_actual_sample_rate_ms = 0.0;
double g_last_exec_time_ms = 0.0;
double g_last_total_time_ms = 0.0;
int32_t g_execute_divider_pv = 1;
int32_t g_execute_count = 0;
size_t g_execute_count_publish_divider = 1;
size_t g_execute_count_publish_counter = 0;
bool g_execute_count_publish_due = false;

uint32_t getHostControlWord() {
  return static_cast<uint32_t>(g_control_word);
}

const ecmcStrucppHostServices g_host_services = {
  1,
  &getHostControlWord,
};

double plcNaN() {
  return std::numeric_limits<double>::quiet_NaN();
}

void logError(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "[ecmc_plugin_strucpp] ERROR: ");
  std::vfprintf(stderr, fmt, args);
  std::fprintf(stderr, "\n");
  std::fflush(stderr);
  va_end(args);
}

void logInfo(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::fprintf(stdout, "[ecmc_plugin_strucpp] ");
  std::vfprintf(stdout, fmt, args);
  std::fprintf(stdout, "\n");
  std::fflush(stdout);
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
                    size_t* bytes_out) {
  if (!asyn_type_out || !bytes_out) {
    return false;
  }

  switch (export_type) {
  case ECMC_STRUCPP_EXPORT_BOOL:
    *asyn_type_out = asynParamInt32;
    *bytes_out = 1;
    return true;
  case ECMC_STRUCPP_EXPORT_S8:
    *asyn_type_out = asynParamInt32;
    *bytes_out = 1;
    return true;
  case ECMC_STRUCPP_EXPORT_U8:
    *asyn_type_out = asynParamInt32;
    *bytes_out = 1;
    return true;
  case ECMC_STRUCPP_EXPORT_S16:
    *asyn_type_out = asynParamInt32;
    *bytes_out = 2;
    return true;
  case ECMC_STRUCPP_EXPORT_U16:
    *asyn_type_out = asynParamInt32;
    *bytes_out = 2;
    return true;
  case ECMC_STRUCPP_EXPORT_S32:
    *asyn_type_out = asynParamInt32;
    *bytes_out = 4;
    return true;
  case ECMC_STRUCPP_EXPORT_U32:
    *asyn_type_out = asynParamUInt32Digital;
    *bytes_out = 4;
    return true;
  case ECMC_STRUCPP_EXPORT_F32:
    *asyn_type_out = asynParamFloat64;
    *bytes_out = 4;
    return true;
  case ECMC_STRUCPP_EXPORT_F64:
    *asyn_type_out = asynParamFloat64;
    *bytes_out = 8;
    return true;
  default:
    return false;
  }
}

const char* exportTypeName(uint32_t export_type) {
  switch (export_type) {
  case ECMC_STRUCPP_EXPORT_BOOL:
    return "BOOL";
  case ECMC_STRUCPP_EXPORT_S8:
    return "S8";
  case ECMC_STRUCPP_EXPORT_U8:
    return "U8";
  case ECMC_STRUCPP_EXPORT_S16:
    return "S16";
  case ECMC_STRUCPP_EXPORT_U16:
    return "U16";
  case ECMC_STRUCPP_EXPORT_S32:
    return "S32";
  case ECMC_STRUCPP_EXPORT_U32:
    return "U32";
  case ECMC_STRUCPP_EXPORT_F32:
    return "F32";
  case ECMC_STRUCPP_EXPORT_F64:
    return "F64";
  default:
    return "UNKNOWN";
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

bool bindBuiltinVars(std::string* error_out) {
  g_builtin_params.clear();

  if (!createBoundParam(kBuiltinControlWordName,
                        ECMC_STRUCPP_EXPORT_S32,
                        1,
                        &g_control_word,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }
  if (!createBoundParam(kBuiltinSampleRateName,
                        ECMC_STRUCPP_EXPORT_F64,
                        1,
                        &g_requested_sample_rate_ms,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }
  if (!createBoundParam(kBuiltinActualSampleRateName,
                        ECMC_STRUCPP_EXPORT_F64,
                        0,
                        &g_actual_sample_rate_ms,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }
  if (!createBoundParam(kBuiltinLastExecTimeName,
                        ECMC_STRUCPP_EXPORT_F64,
                        0,
                        &g_last_exec_time_ms,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }
  if (!createBoundParam(kBuiltinLastTotalTimeName,
                        ECMC_STRUCPP_EXPORT_F64,
                        0,
                        &g_last_total_time_ms,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }
  if (!createBoundParam(kBuiltinExecuteDividerName,
                        ECMC_STRUCPP_EXPORT_S32,
                        0,
                        &g_execute_divider_pv,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }
  if (!createBoundParam(kBuiltinExecuteCountName,
                        ECMC_STRUCPP_EXPORT_S32,
                        0,
                        &g_execute_count,
                        &g_builtin_params,
                        error_out)) {
    return false;
  }

  syncBuiltinParams(true, false);
  return true;
}

bool bindExportedVars(std::string* error_out) {
  g_exported_params.clear();
  g_grouped_bool_params.clear();

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

  std::set<std::string> seen_scalar_names;
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
    size_t bytes = 0;
    if (!exportTypeInfo(export_var.type, &asyn_type, &bytes)) {
      if (error_out) {
        *error_out = std::string("Unsupported exported ST variable type for '") +
                     export_var.name + "'";
      }
      return false;
    }

    const bool grouped_bool =
      (export_var.flags & ECMC_STRUCPP_EXPORT_FLAG_GROUPED_BOOL) != 0u;
    if (grouped_bool) {
      if (export_var.type != ECMC_STRUCPP_EXPORT_BOOL) {
        if (error_out) {
          *error_out = std::string("Grouped BOOL export '") + export_var.name +
                       "' must use BOOL type";
        }
        return false;
      }

      auto group_it = std::find_if(g_grouped_bool_params.begin(),
                                   g_grouped_bool_params.end(),
                                   [&](const GroupedBoolParamBinding& binding) {
                                     return binding.name == export_var.name;
                                   });
      if (group_it == g_grouped_bool_params.end()) {
        int param_id = -1;
        if (g_asyn_port->createParam(0, export_var.name, asynParamUInt32Digital, &param_id) != asynSuccess) {
          if (error_out) {
            *error_out = std::string("Failed to create grouped BOOL asyn parameter for exported ST variable '") +
                         export_var.name + "'";
          }
          return false;
        }
        g_grouped_bool_params.push_back({export_var.name, param_id, export_var.writable, 0xFFFFFFFFu});
        group_it = std::prev(g_grouped_bool_params.end());
      } else if (group_it->writable != export_var.writable) {
        if (error_out) {
          *error_out = std::string("Conflicting grouped BOOL writable mode for export '") +
                       export_var.name + "'";
        }
        return false;
      }

      const auto duplicate_bit = std::find_if(group_it->members.begin(),
                                              group_it->members.end(),
                                              [&](const GroupedBoolMember& member) {
                                                return member.bit_index == export_var.bit_index;
                                              });
      if (duplicate_bit != group_it->members.end()) {
        if (error_out) {
          *error_out = std::string("Duplicate grouped BOOL bit ") +
                       std::to_string(export_var.bit_index) + " for export '" +
                       export_var.name + "'";
        }
        return false;
      }

      group_it->members.push_back({export_var.data, export_var.bit_index});
      continue;
    }

    if (!seen_scalar_names.insert(export_var.name).second) {
      if (error_out) {
        *error_out = std::string("Duplicate exported ST variable name: '") +
                     export_var.name + "'";
      }
      return false;
    }

    int param_id = -1;
    if (g_asyn_port->createParam(0, export_var.name, asyn_type, &param_id) != asynSuccess) {
      if (error_out) {
        *error_out = std::string("Failed to create asyn parameter for exported ST variable '") +
                     export_var.name + "'";
      }
      return false;
    }

    g_exported_params.push_back({export_var.name,
                                 param_id,
                                 export_var.type,
                                 export_var.writable,
                                 export_var.data,
                                 bytes,
                                 {},
                                 false});
  }

  g_asyn_port->syncExportedParams(&g_exported_params, true, false);
  g_asyn_port->syncGroupedBoolParams(&g_grouped_bool_params, true, false);

  return true;
}

std::string describeBuiltinVars() {
  if (g_builtin_params.empty()) {
    return "<none>";
  }

  std::ostringstream oss;
  for (size_t i = 0; i < g_builtin_params.size(); ++i) {
    if (i != 0) {
      oss << ",";
    }
    oss << g_builtin_params[i].name;
  }
  return oss.str();
}

std::string describeExportedVars() {
  if (g_exported_params.empty() && g_grouped_bool_params.empty()) {
    return "<none>";
  }

  std::ostringstream oss;
  for (size_t i = 0; i < g_exported_params.size(); ++i) {
    if (i != 0) {
      oss << ",";
    }
    const auto& binding = g_exported_params[i];
    oss << binding.name << "[" << exportTypeName(binding.type) << ","
        << (binding.writable != 0 ? "rw" : "ro") << "]";
  }
  for (size_t i = 0; i < g_grouped_bool_params.size(); ++i) {
    if (!oss.str().empty()) {
      oss << ",";
    }
    const auto& binding = g_grouped_bool_params[i];
    oss << binding.name << "[BOOL_GROUP,"
        << (binding.writable != 0 ? "rw" : "ro")
        << ",bits=" << binding.members.size() << "]";
  }
  return oss.str();
}

bool ensureAsynPort(std::string* error_out) {
  if (g_asyn_port) {
    return true;
  }

  try {
    g_asyn_port = new StrucppAsynPort(g_config.asyn_port_name.c_str());
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

bool parseDoubleToken(const std::string& text,
                      const char* field_name,
                      double* out_value,
                      std::string* error_out) {
  if (!out_value) {
    if (error_out) {
      *error_out = std::string("Output pointer is null for ") + field_name;
    }
    return false;
  }

  char* end_ptr = nullptr;
  const double parsed = std::strtod(text.c_str(), &end_ptr);
  if (!end_ptr || *end_ptr != '\0' || !std::isfinite(parsed)) {
    if (error_out) {
      *error_out = std::string("Invalid ") + field_name + " value: '" + text + "'";
    }
    return false;
  }

  *out_value = parsed;
  return true;
}

bool resolveExecuteDivider(const PluginConfig& config,
                           size_t* out_divider,
                           double* out_sample_time_ms,
                           std::string* error_out) {
  if (!out_divider) {
    if (error_out) {
      *error_out = "Output divider pointer is null";
    }
    return false;
  }

  *out_divider = 1;
  const double base_sample_time_ms = getEcmcSampleTimeMS();
  const bool base_sample_time_valid =
    std::isfinite(base_sample_time_ms) && base_sample_time_ms > 0.0;

  if (out_sample_time_ms) {
    *out_sample_time_ms = base_sample_time_valid ? base_sample_time_ms : 0.0;
  }

  if (config.sample_rate_ms <= 0.0) {
    return true;
  }

  if (!base_sample_time_valid) {
    if (error_out) {
      *error_out =
        "Cannot resolve sample_rate_ms because ecmc sample time is not available";
    }
    return false;
  }

  const double ratio = config.sample_rate_ms / base_sample_time_ms;
  const size_t divider =
    static_cast<size_t>(std::max(1.0, std::ceil(ratio - 1e-12)));

  *out_divider = divider;
  return true;
}

void applyControlWord(uint32_t control_word) {
  g_control_word = static_cast<int32_t>(control_word);
  g_execution_enabled =
    (control_word & kControlWordEnableExecutionBit) != 0 ? 1u : 0u;
  g_measure_exec_time_enabled =
    (control_word & kControlWordMeasureExecTimeBit) != 0 ? 1u : 0u;
  g_measure_total_time_enabled =
    (control_word & kControlWordMeasureTotalTimeBit) != 0 ? 1u : 0u;
  if (!g_measure_exec_time_enabled) {
    g_last_exec_time_ms = 0.0;
  }
  if (!g_measure_total_time_enabled) {
    g_last_total_time_ms = 0.0;
  }
}

bool applyRuntimeSampleRateMs(double requested_ms,
                              std::string* error_out) {
  PluginConfig temp = g_config;
  temp.sample_rate_ms = requested_ms;

  size_t divider = 1;
  double base_sample_time_ms = 0.0;
  if (!resolveExecuteDivider(temp, &divider, &base_sample_time_ms, error_out)) {
    return false;
  }

  g_requested_sample_rate_ms = requested_ms;
  g_runtime_execute_divider = divider;
  g_actual_sample_rate_ms = base_sample_time_ms * static_cast<double>(divider);
  g_execute_divider_pv = static_cast<int32_t>(divider);
  if (g_actual_sample_rate_ms > 0.0) {
    g_execute_count_publish_divider = static_cast<size_t>(
      std::max(1.0, std::ceil(100.0 / g_actual_sample_rate_ms - 1e-12)));
  } else {
    g_execute_count_publish_divider = 1;
  }
  g_execute_count_publish_counter = 0;
  g_execute_count_publish_due = true;
  return true;
}

bool createBoundParam(const char* name,
                      uint32_t type,
                      uint32_t writable,
                      void* data,
                      std::vector<ExportedParamBinding>* out_bindings,
                      std::string* error_out) {
  if (!name || !data || !out_bindings) {
    if (error_out) {
      *error_out = "Invalid built-in parameter configuration";
    }
    return false;
  }

  if (!g_asyn_port) {
    if (error_out) {
      *error_out = "Plugin asyn port is not initialized";
    }
    return false;
  }

  asynParamType asyn_type = asynParamNotDefined;
  size_t bytes = 0;
  if (!exportTypeInfo(type, &asyn_type, &bytes)) {
    if (error_out) {
      *error_out = std::string("Unsupported built-in parameter type for '") + name + "'";
    }
    return false;
  }

  int param_id = -1;
  if (g_asyn_port->createParam(0, name, asyn_type, &param_id) != asynSuccess) {
    if (error_out) {
      *error_out = std::string("Failed to create asyn parameter '") + name + "'";
    }
    return false;
  }

  out_bindings->push_back(
    {name, param_id, type, writable, data, bytes, {}, false});
  return true;
}

ExportedParamBinding* findBindingForReason(std::vector<ExportedParamBinding>* bindings,
                                           int reason) {
  if (!bindings || reason < 0) {
    return nullptr;
  }

  for (auto& binding : *bindings) {
    if (binding.param_id == reason) {
      return &binding;
    }
  }
  return nullptr;
}

GroupedBoolParamBinding* findGroupedBoolBindingForReason(std::vector<GroupedBoolParamBinding>* bindings,
                                                         int reason) {
  if (!bindings || reason < 0) {
    return nullptr;
  }

  for (auto& binding : *bindings) {
    if (binding.param_id == reason) {
      return &binding;
    }
  }
  return nullptr;
}

ExportedParamBinding* paramBindingForReason(int reason) {
  if (auto* binding = findBindingForReason(&g_builtin_params, reason)) {
    return binding;
  }
  return findBindingForReason(&g_exported_params, reason);
}

GroupedBoolParamBinding* groupedBoolBindingForReason(int reason) {
  return findGroupedBoolBindingForReason(&g_grouped_bool_params, reason);
}

bool syncBuiltinParams(bool force, bool defer_callbacks) {
  if (!g_asyn_port || g_builtin_params.empty()) {
    return true;
  }
  return g_asyn_port->syncExportedParams(&g_builtin_params, force, defer_callbacks);
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
      } else if (key == "sample_rate_ms") {
        if (value.empty()) {
          out_config->sample_rate_ms = 0.0;
        }
        if (!value.empty()) {
          if (!parseDoubleToken(value, "sample_rate_ms", &out_config->sample_rate_ms, error_out)) {
            return false;
          }
          if (out_config->sample_rate_ms <= 0.0) {
            if (error_out) {
              *error_out = "Invalid sample_rate_ms value: '" + value + "'";
            }
            return false;
          }
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

  if (out_config->mapping_file.empty() && out_config->input_item.empty() &&
      out_config->output_item.empty() && out_config->input_bindings.empty() &&
      out_config->output_bindings.empty()) {
    out_config->mapping_file = out_config->logic_lib + ".map";
  }

  return true;
}

int currentMasterIndex() {
  return getEcmcMasterIndex();
}

bool resolveItemName(const std::string& item_name,
                     std::string* resolved_name,
                     std::string* error_out) {
  if (!resolved_name) {
    if (error_out) {
      *error_out = "Resolved item name output is null";
    }
    return false;
  }

  *resolved_name = item_name;
  if (item_name.rfind("ec.s", 0) != 0) {
    return true;
  }

  const int master_index = currentMasterIndex();
  if (master_index < 0) {
    if (error_out) {
      *error_out = "Cannot resolve default EtherCAT master for shorthand item '" +
                   item_name + "'";
    }
    return false;
  }

  *resolved_name = "ec" + std::to_string(master_index) + item_name.substr(2);
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

  std::string resolved_name;
  if (!resolveItemName(item_name, &resolved_name, error_out)) {
    return false;
  }

  std::vector<char> mutable_name(resolved_name.begin(), resolved_name.end());
  mutable_name.push_back('\0');

  auto* item = static_cast<ecmcDataItem*>(getEcmcDataItem(mutable_name.data()));
  if (!item) {
    if (error_out) {
      *error_out = "Could not find ecmcDataItem '" + resolved_name + "'";
      if (resolved_name != item_name) {
        *error_out += " (resolved from '" + item_name + "')";
      }
    }
    return false;
  }

  ecmcDataItemInfo* info = item->getDataItemInfo();
  if (!info || !info->dataPointerValid || !info->data || info->dataSize == 0) {
    if (error_out) {
      *error_out = "ecmcDataItem '" + resolved_name +
                   "' does not expose a valid data buffer";
    }
    return false;
  }

  out_span->data = info->data;
  out_span->size = info->dataSize;
  out_span->name = resolved_name;
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

  logInfo("loadLogicRuntime dlopen path=%s", path.c_str());
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

  logInfo("loadLogicRuntime get_api");
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

  if (!g_logic.api->set_host_services) {
    if (error_out) {
      *error_out = "Logic API is missing host services hook";
    }
    unloadLogicRuntime();
    return false;
  }

  g_logic.api->set_host_services(&g_host_services);

  logInfo("loadLogicRuntime create_instance");
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

  logInfo("construct begin");
  std::string error;
  if (!parseConfigString(configStr, &g_config, &error)) {
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct parsed config logic_lib=%s asyn_port=%s sample_rate_ms=%g",
          g_config.logic_lib.c_str(),
          g_config.asyn_port_name.c_str(),
          g_config.sample_rate_ms);

  double base_sample_time_ms = 0.0;
  if (!resolveExecuteDivider(g_config,
                             &g_runtime_execute_divider,
                             &base_sample_time_ms,
                             &error)) {
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct resolved execute divider=%zu base_sample_ms=%g",
          g_runtime_execute_divider,
          base_sample_time_ms);

  applyControlWord(kControlWordEnableExecutionBit);
  g_execute_count = 0;
  if (!applyRuntimeSampleRateMs(g_config.sample_rate_ms, &error)) {
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct applied runtime sample rate actual_ms=%g", g_actual_sample_rate_ms);

  if (!loadLogicRuntime(g_config.logic_lib, &error)) {
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct logic runtime loaded");

  if (!ensureAsynPort(&error)) {
    unloadLogicRuntime();
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct asyn port ready");

  if (!bindBuiltinVars(&error)) {
    destroyAsynPort();
    unloadLogicRuntime();
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct builtin vars bound");

  if (!bindExportedVars(&error)) {
    destroyAsynPort();
    unloadLogicRuntime();
    logError("%s", error.c_str());
    return -1;
  }
  logInfo("construct exported vars bound");

  alreadyLoaded = 1;
  const std::string input_bindings = describeBindingSpecs(g_config.input_bindings);
  const std::string output_bindings = describeBindingSpecs(g_config.output_bindings);
  logInfo("configured logic_lib=%s asyn_port=%s mapping_file=%s input_item=%s input_bindings=%s output_item=%s output_bindings=%s memory_bytes=%zu sample_rate_ms=%g execute_divider=%zu",
          g_config.logic_lib.c_str(),
          g_config.asyn_port_name.c_str(),
          g_config.mapping_file.empty() ? "<none>" : g_config.mapping_file.c_str(),
          g_config.input_item.empty() ? "<none>" : g_config.input_item.c_str(),
          input_bindings.c_str(),
          g_config.output_item.empty() ? "<none>" : g_config.output_item.c_str(),
          output_bindings.c_str(),
          g_config.memory_bytes,
          g_config.sample_rate_ms,
          g_runtime_execute_divider);
  if (g_config.sample_rate_ms > 0.0) {
    logInfo("sample_rate base_ms=%g requested_ms=%g actual_ms=%g",
            base_sample_time_ms,
            g_config.sample_rate_ms,
            base_sample_time_ms * static_cast<double>(g_runtime_execute_divider));
  }
  logInfo("builtin_vars=%s", describeBuiltinVars().c_str());
  logInfo("exported_vars=%s", describeExportedVars().c_str());
  return 0;
}

static void destruct(void) {
  g_builtin_params.clear();
  g_exported_params.clear();
  g_grouped_bool_params.clear();
  unloadLogicRuntime();
  clearRuntimeState();
  destroyAsynPort();
  g_config = PluginConfig {};
  g_runtime_execute_divider = 1;
  g_control_word = static_cast<int32_t>(kControlWordEnableExecutionBit);
  g_execution_enabled = 1;
  g_measure_exec_time_enabled = 0;
  g_measure_total_time_enabled = 0;
  g_requested_sample_rate_ms = 0.0;
  g_actual_sample_rate_ms = 0.0;
  g_last_exec_time_ms = 0.0;
  g_last_total_time_ms = 0.0;
  g_execute_divider_pv = 1;
  g_execute_count = 0;
  g_execute_count_publish_divider = 1;
  g_execute_count_publish_counter = 0;
  g_execute_count_publish_due = false;
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
  g_execute_divider_counter = 0;
  g_execute_count = 0;
  g_execute_count_publish_counter = 0;
  g_execute_count_publish_due = true;
  syncBuiltinParams(true, false);

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
    logInfo("mapping_summary direct_mappings=%zu exports=%zu",
            g_bound_mappings.size(),
            g_exported_params.size() + g_grouped_bool_params.size());
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

  logInfo("bound logic=%s input=%s (%zu bytes) output=%s (%zu bytes) memory=%zu bytes",
          g_logic.api->name ? g_logic.api->name : "<unnamed>",
          g_images.input.name.empty() ? "<none>" : g_images.input.name.c_str(),
          g_images.input.size,
          g_images.output.name.empty() ? "<none>" : g_images.output.name.c_str(),
          g_images.output.size,
          g_images.memory.size);
  if (!g_exported_params.empty() || !g_grouped_bool_params.empty()) {
    logInfo("published %zu scalar ST variables and %zu grouped BOOL exports to asyn",
            g_exported_params.size(),
            g_grouped_bool_params.size());
  }
  logInfo("binding_summary mode=%s exports=%zu",
          (!g_config.input_bindings.empty() || !g_config.output_bindings.empty()) ?
            "explicit_bindings" :
            ((g_config.input_item.empty() && g_config.output_item.empty()) ? "none" :
             "contiguous_image"),
          g_exported_params.size() + g_grouped_bool_params.size());
  return 0;
}

static int exitRealtime(void) {
  clearRuntimeState();
  return 0;
}

static int realtime(int) {
  const bool execute_now = (g_execute_divider_counter == 0);
  g_execute_divider_counter = (g_execute_divider_counter + 1) % g_runtime_execute_divider;
  if (!execute_now) {
    return 0;
  }

  if (g_execution_enabled == 0u) {
    g_last_exec_time_ms = 0.0;
    g_last_total_time_ms = 0.0;
    syncBuiltinParams(false, true);
    return 0;
  }

  timespec total_start {};
  timespec total_end {};
  timespec exec_start {};
  timespec exec_end {};
  const bool measure_exec_time = g_measure_exec_time_enabled != 0u;
  const bool measure_total_time = g_measure_total_time_enabled != 0u;
  if (measure_total_time) {
    clock_gettime(CLOCK_MONOTONIC, &total_start);
  }
  if (measure_exec_time) {
    clock_gettime(CLOCK_MONOTONIC, &exec_start);
  }

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
  if (g_execute_count < std::numeric_limits<int32_t>::max()) {
    g_execute_count += 1;
  }
  if (measure_exec_time) {
    clock_gettime(CLOCK_MONOTONIC, &exec_end);
    const int64_t start_ns =
      static_cast<int64_t>(exec_start.tv_sec) * 1000000000ll + exec_start.tv_nsec;
    const int64_t end_ns =
      static_cast<int64_t>(exec_end.tv_sec) * 1000000000ll + exec_end.tv_nsec;
    g_last_exec_time_ms =
      static_cast<double>(end_ns - start_ns) / 1000000.0;
  } else {
    g_last_exec_time_ms = 0.0;
  }
  g_execute_count_publish_counter += 1;
  if (g_execute_count_publish_counter >= g_execute_count_publish_divider) {
    g_execute_count_publish_counter = 0;
    g_execute_count_publish_due = true;
  } else {
    g_execute_count_publish_due = false;
  }

  ecmcStrucppExecuteMemoryPostCopyPlan(g_copy_plan);
  ecmcStrucppExecuteOutputCopyPlan(g_copy_plan);

  if (!g_bound_output_bindings.empty()) {
    scatterBindings(g_images.output, g_bound_output_bindings);
  }

  if (g_asyn_port && !g_exported_params.empty()) {
    g_asyn_port->syncExportedParams(&g_exported_params, false, true);
  }
  if (g_asyn_port && !g_grouped_bool_params.empty()) {
    g_asyn_port->syncGroupedBoolParams(&g_grouped_bool_params, false, true);
  }
  if (measure_total_time) {
    clock_gettime(CLOCK_MONOTONIC, &total_end);
    const int64_t start_ns =
      static_cast<int64_t>(total_start.tv_sec) * 1000000000ll + total_start.tv_nsec;
    const int64_t end_ns =
      static_cast<int64_t>(total_end.tv_sec) * 1000000000ll + total_end.tv_nsec;
    g_last_total_time_ms =
      static_cast<double>(end_ns - start_ns) / 1000000.0;
  } else {
    g_last_total_time_ms = 0.0;
  }
  syncBuiltinParams(false, true);
  g_execute_count_publish_due = false;
  return 0;
}

ecmcPluginData makePluginData() {
  ecmcPluginData data {};

  data.ifVersion = ECMC_PLUG_VERSION_MAGIC;
  data.name = "ecmc_plugin_strucpp";
  data.desc = "Generic ecmc host plugin for loadable STruCpp logic libraries.";
  data.optionDesc =
    "logic_lib=<path>;asyn_port=<plugin-port>;[mapping_file=<path>|input_item=<name>|input_bindings=<offset:item[@bytes],...>];"
    "[mapping_file=<path>|output_item=<name>|output_bindings=<offset:item[@bytes],...>];memory_bytes=<n>;sample_rate_ms=<n>\n"
    "PLC consts: STRUCPP_AREA_I=0, STRUCPP_AREA_Q=1, STRUCPP_AREA_M=2\n"
    "PLC funcs: strucpp_get_bit(a,b,bit), strucpp_set_bit(a,b,bit,v), "
    "strucpp_get_u8(a,b), strucpp_set_u8(a,b,v), strucpp_get_s8(a,b), strucpp_set_s8(a,b,v), "
    "strucpp_get_u16(a,b), strucpp_set_u16(a,b,v), strucpp_get_s16(a,b), strucpp_set_s16(a,b,v), "
    "strucpp_get_u32(a,b), strucpp_set_u32(a,b,v), strucpp_get_s32(a,b), strucpp_set_s32(a,b,v), "
    "strucpp_get_f32(a,b), strucpp_set_f32(a,b,v), "
    "strucpp_get_f64(a,b), strucpp_set_f64(a,b,v)";
  data.version = ECMC_PLUGIN_VERSION;
  data.constructFnc = construct;
  data.destructFnc = destruct;
  data.realtimeEnterFnc = enterRealtime;
  data.realtimeExitFnc = exitRealtime;
  data.realtimeFnc = realtime;

  auto setFunc2 = [&data](size_t index,
                          const char* name,
                          const char* desc,
                          double (*func)(double, double)) {
    auto& entry = data.funcs[index];
    entry.funcName = name;
    entry.funcDesc = desc;
    entry.funcArg2 = func;
  };

  auto setFunc3 = [&data](size_t index,
                          const char* name,
                          const char* desc,
                          double (*func)(double, double, double)) {
    auto& entry = data.funcs[index];
    entry.funcName = name;
    entry.funcDesc = desc;
    entry.funcArg3 = func;
  };

  auto setFunc4 = [&data](size_t index,
                          const char* name,
                          const char* desc,
                          double (*func)(double, double, double, double)) {
    auto& entry = data.funcs[index];
    entry.funcName = name;
    entry.funcDesc = desc;
    entry.funcArg4 = func;
  };

  setFunc3(0,
           "strucpp_get_bit",
           "Read one bit from the plugin image area: area(0=I,1=Q,2=M), byte_offset, bit_index.",
           plcGetBit);
  setFunc4(1,
           "strucpp_set_bit",
           "Write one bit in the plugin image area: area(0=I,1=Q,2=M), byte_offset, bit_index, value.",
           plcSetBit);
  setFunc2(2,
           "strucpp_get_u8",
           "Read one unsigned byte from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetU8);
  setFunc3(3,
           "strucpp_set_u8",
           "Write one unsigned byte in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetU8);
  setFunc2(4,
           "strucpp_get_s8",
           "Read one signed byte from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetS8);
  setFunc3(5,
           "strucpp_set_s8",
           "Write one signed byte in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetS8);
  setFunc2(6,
           "strucpp_get_u16",
           "Read one unsigned 16-bit word from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetU16);
  setFunc3(7,
           "strucpp_set_u16",
           "Write one unsigned 16-bit word in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetU16);
  setFunc2(8,
           "strucpp_get_s16",
           "Read one signed 16-bit word from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetS16);
  setFunc3(9,
           "strucpp_set_s16",
           "Write one signed 16-bit word in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetS16);
  setFunc2(10,
           "strucpp_get_u32",
           "Read one unsigned 32-bit value from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetU32);
  setFunc3(11,
           "strucpp_set_u32",
           "Write one unsigned 32-bit value in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetU32);
  setFunc2(12,
           "strucpp_get_s32",
           "Read one signed 32-bit value from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetS32);
  setFunc3(13,
           "strucpp_set_s32",
           "Write one signed 32-bit value in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetS32);
  setFunc2(14,
           "strucpp_get_f32",
           "Read one 32-bit float from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetF32);
  setFunc3(15,
           "strucpp_set_f32",
           "Write one 32-bit float in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetF32);
  setFunc2(16,
           "strucpp_get_f64",
           "Read one 64-bit float from the plugin image area: area(0=I,1=Q,2=M), byte_offset.",
           plcGetF64);
  setFunc3(17,
           "strucpp_set_f64",
           "Write one 64-bit float in the plugin image area: area(0=I,1=Q,2=M), byte_offset, value.",
           plcSetF64);

  auto setConst = [&data](size_t index,
                          const char* name,
                          const char* desc,
                          double value) {
    auto& entry = data.consts[index];
    entry.constName = name;
    entry.constDesc = desc;
    entry.constValue = value;
  };

  setConst(0,
           "STRUCPP_AREA_I",
           "Input image area selector for strucpp exprtk helper functions.",
           kPlcAreaInput);
  setConst(1,
           "STRUCPP_AREA_Q",
           "Output image area selector for strucpp exprtk helper functions.",
           kPlcAreaOutput);
  setConst(2,
           "STRUCPP_AREA_M",
           "Memory image area selector for strucpp exprtk helper functions.",
           kPlcAreaMemory);

  return data;
}

static ecmcPluginData pluginDataDef = makePluginData();

}  // namespace

extern "C" {
ecmc_plugin_register(pluginDataDef);
}
