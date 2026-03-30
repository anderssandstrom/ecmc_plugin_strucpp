#include <cstdint>

#include "ecmcStrucppLogicWrapper.hpp"

namespace strucpp {

uint32_t ecmcStrucppGetEcMasterStateWord(int32_t master_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_ec_master_state_word) {
    return 0u;
  }
  return services->get_ec_master_state_word(master_index);
}

uint32_t ecmcStrucppGetEcSlaveStateWord(int32_t master_index, int32_t slave_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_ec_slave_state_word) {
    return 0u;
  }
  return services->get_ec_slave_state_word(master_index, slave_index);
}

}  // namespace strucpp

uint32_t ecmcStrucppGetEcMasterStateWord(int32_t master_index) {
  return strucpp::ecmcStrucppGetEcMasterStateWord(master_index);
}

uint32_t ecmcStrucppGetEcSlaveStateWord(int32_t master_index, int32_t slave_index) {
  return strucpp::ecmcStrucppGetEcSlaveStateWord(master_index, slave_index);
}
