#include <array>

#include "ecmcStrucppLogicIface.hpp"
#include "ecmcStrucppMcWrapper.hpp"
#include "iec_var.hpp"

namespace {

class McPowerMoveAbsProgram {
public:
  McPowerMoveAbsProgram()
      : located_vars_{{makeBit(strucpp::LocatedArea::Input, 0, 0, power_enable_),
                       makeBit(strucpp::LocatedArea::Input, 0, 1, move_execute_),
                       makeScalar(strucpp::LocatedArea::Input, strucpp::LocatedSize::LWord, 8, target_position_),
                       makeScalar(strucpp::LocatedArea::Input, strucpp::LocatedSize::LWord, 16, velocity_),
                       makeScalar(strucpp::LocatedArea::Input, strucpp::LocatedSize::LWord, 24, acceleration_),
                       makeScalar(strucpp::LocatedArea::Input, strucpp::LocatedSize::LWord, 32, deceleration_),
                       makeBit(strucpp::LocatedArea::Output, 0, 0, power_status_),
                       makeBit(strucpp::LocatedArea::Output, 0, 1, power_valid_),
                       makeBit(strucpp::LocatedArea::Output, 0, 2, move_busy_),
                       makeBit(strucpp::LocatedArea::Output, 0, 3, move_done_),
                       makeBit(strucpp::LocatedArea::Output, 0, 4, move_error_),
                       makeScalar(strucpp::LocatedArea::Output, strucpp::LocatedSize::DWord, 4, move_error_id_),
                       makeScalar(strucpp::LocatedArea::Output, strucpp::LocatedSize::LWord, 8, actual_position_)}} {}

  void run() {
    const ecmcStrucpp::mc::AxisRef axis{0};

    mc_power_.run(axis, power_enable_.get());
    mc_move_abs_.run(axis,
                     power_enable_.get() && move_execute_.get(),
                     target_position_.get(),
                     velocity_.get(),
                     acceleration_.get(),
                     deceleration_.get());
    mc_read_pos_.run(axis, true);

    power_status_   = mc_power_.Status;
    power_valid_    = mc_power_.Valid;
    move_busy_      = mc_move_abs_.Busy;
    move_done_      = mc_move_abs_.Done;
    move_error_     = mc_move_abs_.Error;
    move_error_id_  = static_cast<int32_t>(mc_move_abs_.ErrorID);
    actual_position_ = mc_read_pos_.Position;
  }

  const strucpp::LocatedVar* locatedVars() const {
    return located_vars_.data();
  }

  uint32_t locatedVarCount() const {
    return static_cast<uint32_t>(located_vars_.size());
  }

private:
  template <typename T>
  static strucpp::LocatedVar makeScalar(strucpp::LocatedArea area,
                                        strucpp::LocatedSize size,
                                        uint16_t             byte_index,
                                        strucpp::IECVar<T>&  var) {
    return strucpp::LocatedVar{area, size, byte_index, 0, {0, 0, 0}, var.raw_ptr()};
  }

  static strucpp::LocatedVar makeBit(strucpp::LocatedArea area,
                                     uint16_t             byte_index,
                                     uint8_t              bit_index,
                                     strucpp::IECVar<bool>& var) {
    return strucpp::LocatedVar{area, strucpp::LocatedSize::Bit, byte_index, bit_index, {0, 0, 0}, var.raw_ptr()};
  }

  strucpp::IECVar<bool>    power_enable_{false};
  strucpp::IECVar<bool>    move_execute_{false};
  strucpp::IECVar<double>  target_position_{0.0};
  strucpp::IECVar<double>  velocity_{0.0};
  strucpp::IECVar<double>  acceleration_{0.0};
  strucpp::IECVar<double>  deceleration_{0.0};

  strucpp::IECVar<bool>    power_status_{false};
  strucpp::IECVar<bool>    power_valid_{false};
  strucpp::IECVar<bool>    move_busy_{false};
  strucpp::IECVar<bool>    move_done_{false};
  strucpp::IECVar<bool>    move_error_{false};
  strucpp::IECVar<int32_t> move_error_id_{0};
  strucpp::IECVar<double>  actual_position_{0.0};

  ecmcStrucpp::mc::MC_Power              mc_power_{};
  ecmcStrucpp::mc::MC_MoveAbsolute       mc_move_abs_{};
  ecmcStrucpp::mc::MC_ReadActualPosition mc_read_pos_{};

  std::array<strucpp::LocatedVar, 13> located_vars_{};
};

struct LogicInstance {
  McPowerMoveAbsProgram program;
};

void* createInstance() {
  return new LogicInstance();
}

void destroyInstance(void* instance) {
  delete static_cast<LogicInstance*>(instance);
}

void runCycle(void* instance) {
  static_cast<LogicInstance*>(instance)->program.run();
}

const strucpp::LocatedVar* getLocatedVars(void* instance) {
  return static_cast<LogicInstance*>(instance)->program.locatedVars();
}

uint32_t getLocatedVarCount(void* instance) {
  return static_cast<LogicInstance*>(instance)->program.locatedVarCount();
}

const ecmcStrucppLogicApi logic_api = {
    ECMC_STRUCPP_LOGIC_ABI_VERSION,
    "mc_power_move_abs_logic",
    &createInstance,
    &destroyInstance,
    &runCycle,
    &getLocatedVars,
    &getLocatedVarCount,
};

}  // namespace

extern "C" const ecmcStrucppLogicApi* ecmc_strucpp_logic_get_api() {
  return &logic_api;
}
