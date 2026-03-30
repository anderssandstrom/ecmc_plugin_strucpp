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

int32_t ecmcStrucppSetAxisTrajSource(int32_t axis_index, int32_t source) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->set_axis_traj_source) {
    return -1;
  }
  return services->set_axis_traj_source(axis_index, source);
}

int32_t ecmcStrucppSetAxisEncSource(int32_t axis_index, int32_t source) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->set_axis_enc_source) {
    return -1;
  }
  return services->set_axis_enc_source(axis_index, source);
}

int32_t ecmcStrucppSetAxisExtSetPos(int32_t axis_index, double value) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->set_axis_ext_set_pos) {
    return -1;
  }
  return services->set_axis_ext_set_pos(axis_index, value);
}

int32_t ecmcStrucppSetAxisExtActPos(int32_t axis_index, double value) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->set_axis_ext_act_pos) {
    return -1;
  }
  return services->set_axis_ext_act_pos(axis_index, value);
}

}  // namespace strucpp

uint32_t ecmcStrucppGetEcMasterStateWord(int32_t master_index) {
  return strucpp::ecmcStrucppGetEcMasterStateWord(master_index);
}

uint32_t ecmcStrucppGetEcSlaveStateWord(int32_t master_index, int32_t slave_index) {
  return strucpp::ecmcStrucppGetEcSlaveStateWord(master_index, slave_index);
}

int32_t ecmcStrucppSetAxisTrajSource(int32_t axis_index, int32_t source) {
  return strucpp::ecmcStrucppSetAxisTrajSource(axis_index, source);
}

int32_t ecmcStrucppSetAxisEncSource(int32_t axis_index, int32_t source) {
  return strucpp::ecmcStrucppSetAxisEncSource(axis_index, source);
}

int32_t ecmcStrucppSetAxisExtSetPos(int32_t axis_index, double value) {
  return strucpp::ecmcStrucppSetAxisExtSetPos(axis_index, value);
}

int32_t ecmcStrucppSetAxisExtActPos(int32_t axis_index, double value) {
  return strucpp::ecmcStrucppSetAxisExtActPos(axis_index, value);
}
