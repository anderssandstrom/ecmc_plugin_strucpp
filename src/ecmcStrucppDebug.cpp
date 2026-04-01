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

  if (services->publish_debug_text) {
    services->publish_debug_text(message ? message : "");
  }
}

void ecmcStrucppSetDebugPrintEnabled(bool enable) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_control_word || !services->set_control_word) {
    return;
  }

  uint32_t control_word = services->get_control_word();
  if (enable) {
    control_word |= ECMC_STRUCPP_CONTROL_WORD_ENABLE_DEBUG_PRINTS_BIT;
  } else {
    control_word &= ~ECMC_STRUCPP_CONTROL_WORD_ENABLE_DEBUG_PRINTS_BIT;
  }
  services->set_control_word(control_word);
}

void ecmcStrucppSetDebugPrintEnabled(int enable) {
  ecmcStrucppSetDebugPrintEnabled(enable != 0);
}

}  // namespace strucpp

void ecmcStrucppDebugPrint(const char* message) {
  strucpp::ecmcStrucppDebugPrint(message);
}

void ecmcStrucppSetDebugPrintEnabled(int enable) {
  strucpp::ecmcStrucppSetDebugPrintEnabled(enable);
}
