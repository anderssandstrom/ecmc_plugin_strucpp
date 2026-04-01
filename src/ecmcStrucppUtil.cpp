#include <cstdint>
#include <dlfcn.h>

#include "ecmcStrucppLogicWrapper.hpp"

namespace strucpp {

namespace {

int* lookupEpicsStartedFlag() {
  static int* cached_flag = static_cast<int*>(dlsym(RTLD_DEFAULT, "allowCallbackEpicsState"));
  return cached_flag;
}

}  // namespace

double ecmcStrucppGetCycleTimeS() {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_cycle_time_s) {
    return 0.0;
  }
  return services->get_cycle_time_s();
}

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

int32_t ecmcStrucppGetAxisTrajSource(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_traj_source) {
    return -1;
  }
  return services->get_axis_traj_source(axis_index);
}

int32_t ecmcStrucppGetAxisEncSource(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_enc_source) {
    return -1;
  }
  return services->get_axis_enc_source(axis_index);
}

double ecmcStrucppGetAxisActualPos(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_actual_pos) {
    return 0.0;
  }
  return services->get_axis_actual_pos(axis_index);
}

double ecmcStrucppGetAxisSetpointPos(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_setpoint_pos) {
    return 0.0;
  }
  return services->get_axis_setpoint_pos(axis_index);
}

double ecmcStrucppGetAxisActualVel(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_actual_vel) {
    return 0.0;
  }
  return services->get_axis_actual_vel(axis_index);
}

double ecmcStrucppGetAxisSetpointVel(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_setpoint_vel) {
    return 0.0;
  }
  return services->get_axis_setpoint_vel(axis_index);
}

int32_t ecmcStrucppGetAxisEnabled(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_enabled) {
    return 0;
  }
  return services->get_axis_enabled(axis_index);
}

int32_t ecmcStrucppGetAxisBusy(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_busy) {
    return 0;
  }
  return services->get_axis_busy(axis_index);
}

int32_t ecmcStrucppGetAxisError(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_error) {
    return 0;
  }
  return services->get_axis_error(axis_index);
}

int32_t ecmcStrucppGetAxisErrorId(int32_t axis_index) {
  const auto* services = ecmcStrucpp::getHostServices();
  if (!services || !services->get_axis_error_id) {
    return 0;
  }
  return services->get_axis_error_id(axis_index);
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

int32_t ecmcStrucppGetEpicsStarted() {
  const int* started_flag = lookupEpicsStartedFlag();
  if (!started_flag) {
    return 0;
  }
  return *started_flag != 0 ? 1 : 0;
}

}  // namespace strucpp

double ecmcStrucppGetCycleTimeS() {
  return strucpp::ecmcStrucppGetCycleTimeS();
}

uint32_t ecmcStrucppGetEcMasterStateWord(int32_t master_index) {
  return strucpp::ecmcStrucppGetEcMasterStateWord(master_index);
}

uint32_t ecmcStrucppGetEcSlaveStateWord(int32_t master_index, int32_t slave_index) {
  return strucpp::ecmcStrucppGetEcSlaveStateWord(master_index, slave_index);
}

int32_t ecmcStrucppGetAxisTrajSource(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisTrajSource(axis_index);
}

int32_t ecmcStrucppGetAxisEncSource(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisEncSource(axis_index);
}

double ecmcStrucppGetAxisActualPos(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisActualPos(axis_index);
}

double ecmcStrucppGetAxisSetpointPos(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisSetpointPos(axis_index);
}

double ecmcStrucppGetAxisActualVel(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisActualVel(axis_index);
}

double ecmcStrucppGetAxisSetpointVel(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisSetpointVel(axis_index);
}

int32_t ecmcStrucppGetAxisEnabled(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisEnabled(axis_index);
}

int32_t ecmcStrucppGetAxisBusy(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisBusy(axis_index);
}

int32_t ecmcStrucppGetAxisError(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisError(axis_index);
}

int32_t ecmcStrucppGetAxisErrorId(int32_t axis_index) {
  return strucpp::ecmcStrucppGetAxisErrorId(axis_index);
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

int32_t ecmcStrucppGetEpicsStarted() {
  return strucpp::ecmcStrucppGetEpicsStarted();
}
