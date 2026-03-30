#include <cstdio>

#include "ecmcStrucppLogicWrapper.hpp"

namespace strucpp {

void ecmcStrucppDebugPrint(const char* message) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_control_word) {
    return;
  }

  const uint32_t control_word = services->get_control_word();
  if ((control_word & ECMC_STRUCPP_CONTROL_WORD_ENABLE_DEBUG_PRINTS_BIT) == 0u) {
    return;
  }

  std::fprintf(stdout,
               "[ecmc_plugin_strucpp] ST: %s\n",
               message ? message : "");
  std::fflush(stdout);
}

}  // namespace strucpp

void ecmcStrucppDebugPrint(const char* message) {
  strucpp::ecmcStrucppDebugPrint(message);
}
