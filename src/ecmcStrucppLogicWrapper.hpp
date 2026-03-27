#pragma once

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "ecmcStrucppLogicIface.hpp"

namespace ecmcStrucpp {

template <typename ProgramT, size_t N>
struct LogicInstance {
  ProgramT program;
  strucpp::LocatedVar located_vars[N];

  explicit LogicInstance(const strucpp::LocatedVar (&vars)[N]) {
    std::copy(std::begin(vars), std::end(vars), std::begin(located_vars));
  }
};

template <typename ProgramT, size_t N>
void* createLogicInstance(const strucpp::LocatedVar (&vars)[N]) {
  return new LogicInstance<ProgramT, N>(vars);
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

}  // namespace ecmcStrucpp

#define ECMC_STRUCPP_DECLARE_LOGIC_API(LOGIC_NAME, ProgramType, LocatedVarsSymbol) \
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
                                              EcmcStrucppLocatedVarCount_>(LocatedVarsSymbol); \
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
  }; \
  } \
  extern "C" const ecmcStrucppLogicApi* ecmc_strucpp_logic_get_api() { \
    return &logic_api; \
  }
