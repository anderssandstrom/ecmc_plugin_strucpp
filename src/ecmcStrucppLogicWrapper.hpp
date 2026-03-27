#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "ecmcStrucppLogicIface.hpp"

namespace ecmcStrucpp {

template <typename ProgramT>
using InitExportedVarsFn = void (*)(ProgramT&, std::vector<ecmcStrucppExportedVar>&);

template <typename ProgramT, size_t N>
struct LogicInstance {
  ProgramT program;
  strucpp::LocatedVar located_vars[N];
  std::vector<ecmcStrucppExportedVar> exported_vars;

  explicit LogicInstance(const strucpp::LocatedVar (&vars)[N],
                         InitExportedVarsFn<ProgramT> init_exported_vars = nullptr) {
    std::copy(std::begin(vars), std::end(vars), std::begin(located_vars));
    if (init_exported_vars) {
      init_exported_vars(program, exported_vars);
    }
  }
};

template <typename ProgramT, size_t N>
void* createLogicInstance(const strucpp::LocatedVar (&vars)[N],
                          InitExportedVarsFn<ProgramT> init_exported_vars = nullptr) {
  return new LogicInstance<ProgramT, N>(vars, init_exported_vars);
}

template <typename ProgramT, size_t N>
void destroyLogicInstance(void* instance) {
  delete static_cast<LogicInstance<ProgramT, N>*>(instance);
}

template <typename ProgramT, size_t N>
void runLogicCycle(void* instance) {
  static_cast<LogicInstance<ProgramT, N>*>(instance)->program.run();
}

template <typename ProgramT, size_t N>
const strucpp::LocatedVar* getLogicLocatedVars(void* instance) {
  return static_cast<LogicInstance<ProgramT, N>*>(instance)->located_vars;
}

template <typename ProgramT, size_t N>
uint32_t getLogicLocatedVarCount(void*) {
  return static_cast<uint32_t>(N);
}

template <typename ProgramT, size_t N>
const ecmcStrucppExportedVar* getLogicExportedVars(void* instance) {
  const auto& exports =
    static_cast<LogicInstance<ProgramT, N>*>(instance)->exported_vars;
  return exports.empty() ? nullptr : exports.data();
}

template <typename ProgramT, size_t N>
uint32_t getLogicExportedVarCount(void* instance) {
  return static_cast<uint32_t>(
    static_cast<LogicInstance<ProgramT, N>*>(instance)->exported_vars.size());
}

}  // namespace ecmcStrucpp

#define ECMC_STRUCPP_DECLARE_LOGIC_API_WITH_EXPORTS(LOGIC_NAME, ProgramType, LocatedVarsSymbol, InitExportsFn) \
  namespace { \
  using EcmcStrucppProgramType_ = ProgramType; \
  constexpr size_t EcmcStrucppLocatedVarCount_ = \
    std::extent_v<std::remove_reference_t<decltype(LocatedVarsSymbol)>>; \
  static_assert(EcmcStrucppLocatedVarCount_ > 0, \
                "LocatedVarsSymbol must refer to a non-empty array"); \
  const ecmcStrucppLogicApi logic_api = { \
    ECMC_STRUCPP_LOGIC_ABI_VERSION, \
    LOGIC_NAME, \
    +[]() -> void* { \
      return ecmcStrucpp::createLogicInstance<EcmcStrucppProgramType_, \
                                              EcmcStrucppLocatedVarCount_>(LocatedVarsSymbol, InitExportsFn); \
    }, \
    +[](void* instance) { \
      ecmcStrucpp::destroyLogicInstance<EcmcStrucppProgramType_, \
                                        EcmcStrucppLocatedVarCount_>(instance); \
    }, \
    +[](void* instance) { \
      ecmcStrucpp::runLogicCycle<EcmcStrucppProgramType_, \
                                 EcmcStrucppLocatedVarCount_>(instance); \
    }, \
    +[](void* instance) -> const strucpp::LocatedVar* { \
      return ecmcStrucpp::getLogicLocatedVars<EcmcStrucppProgramType_, \
                                              EcmcStrucppLocatedVarCount_>(instance); \
    }, \
    +[](void* instance) -> uint32_t { \
      return ecmcStrucpp::getLogicLocatedVarCount<EcmcStrucppProgramType_, \
                                                  EcmcStrucppLocatedVarCount_>(instance); \
    }, \
    +[](void* instance) -> const ecmcStrucppExportedVar* { \
      return ecmcStrucpp::getLogicExportedVars<EcmcStrucppProgramType_, \
                                               EcmcStrucppLocatedVarCount_>(instance); \
    }, \
    +[](void* instance) -> uint32_t { \
      return ecmcStrucpp::getLogicExportedVarCount<EcmcStrucppProgramType_, \
                                                   EcmcStrucppLocatedVarCount_>(instance); \
    }, \
  }; \
  } \
  extern "C" const ecmcStrucppLogicApi* ecmc_strucpp_logic_get_api() { \
    return &logic_api; \
  }

#define ECMC_STRUCPP_DECLARE_LOGIC_API(LOGIC_NAME, ProgramType, LocatedVarsSymbol) \
  ECMC_STRUCPP_DECLARE_LOGIC_API_WITH_EXPORTS(LOGIC_NAME, ProgramType, LocatedVarsSymbol, nullptr)
