#pragma once

#include <cstdint>

#include "ecmcMcApi.h"

namespace ecmcStrucpp {
namespace mc {

constexpr uint32_t kCreateFailedErrorId = 0xffffffffu;

struct AxisRef {
  int axis_index = -1;
};

class FunctionBlockBase {
public:
  bool     Busy           = false;
  bool     Done           = false;
  bool     Error          = false;
  bool     CommandAborted = false;
  bool     Active         = false;
  uint32_t ErrorID        = 0;

protected:
  template <typename StatusT>
  void assignBase(const StatusT& status) {
    Busy           = status.base.Busy != 0;
    Done           = status.base.Done != 0;
    Error          = status.base.Error != 0;
    CommandAborted = status.base.CommandAborted != 0;
    Active         = status.base.Active != 0;
    ErrorID        = status.base.ErrorID;
  }

  void assignCreateFailed() {
    Busy           = false;
    Done           = false;
    Error          = true;
    CommandAborted = false;
    Active         = false;
    ErrorID        = kCreateFailedErrorId;
  }
};

class MC_Power : public FunctionBlockBase {
public:
  bool Status = false;
  bool Valid  = false;

  MC_Power() : handle_(ecmcMcPowerCreate()) {}

  ~MC_Power() {
    ecmcMcPowerDestroy(handle_);
  }

  int run(AxisRef axis, bool enable) {
    if (!handle_) {
      assignCreateFailed();
      Status = false;
      Valid  = false;
      return -1;
    }
    ecmcMcPowerStatus status{};
    const int error = ecmcMcPowerRun(handle_, axis.axis_index, enable, &status);
    assignBase(status);
    Status = status.Status != 0;
    Valid  = status.Valid != 0;
    return error;
  }

private:
  ecmcMcPowerHandle* handle_ = nullptr;
};

class MC_Reset : public FunctionBlockBase {
public:
  MC_Reset() : handle_(ecmcMcResetCreate()) {}

  ~MC_Reset() {
    ecmcMcResetDestroy(handle_);
  }

  int run(AxisRef axis, bool execute) {
    if (!handle_) {
      assignCreateFailed();
      return -1;
    }
    ecmcMcResetStatus status{};
    const int error = ecmcMcResetRun(handle_, axis.axis_index, execute, &status);
    assignBase(status);
    return error;
  }

private:
  ecmcMcResetHandle* handle_ = nullptr;
};

class MC_MoveAbsolute : public FunctionBlockBase {
public:
  MC_MoveAbsolute() : handle_(ecmcMcMoveAbsoluteCreate()) {}

  ~MC_MoveAbsolute() {
    ecmcMcMoveAbsoluteDestroy(handle_);
  }

  int run(AxisRef axis,
          bool    execute,
          double  position,
          double  velocity,
          double  acceleration,
          double  deceleration) {
    if (!handle_) {
      assignCreateFailed();
      return -1;
    }
    ecmcMcMoveAbsoluteStatus status{};
    const int error = ecmcMcMoveAbsoluteRun(handle_,
                                            axis.axis_index,
                                            execute,
                                            position,
                                            velocity,
                                            acceleration,
                                            deceleration,
                                            &status);
    assignBase(status);
    return error;
  }

private:
  ecmcMcMoveAbsoluteHandle* handle_ = nullptr;
};

class MC_MoveRelative : public FunctionBlockBase {
public:
  MC_MoveRelative() : handle_(ecmcMcMoveRelativeCreate()) {}

  ~MC_MoveRelative() {
    ecmcMcMoveRelativeDestroy(handle_);
  }

  int run(AxisRef axis,
          bool    execute,
          double  distance,
          double  velocity,
          double  acceleration,
          double  deceleration) {
    if (!handle_) {
      assignCreateFailed();
      return -1;
    }
    ecmcMcMoveRelativeStatus status{};
    const int error = ecmcMcMoveRelativeRun(handle_,
                                            axis.axis_index,
                                            execute,
                                            distance,
                                            velocity,
                                            acceleration,
                                            deceleration,
                                            &status);
    assignBase(status);
    return error;
  }

private:
  ecmcMcMoveRelativeHandle* handle_ = nullptr;
};

class MC_Home : public FunctionBlockBase {
public:
  MC_Home() : handle_(ecmcMcHomeCreate()) {}

  ~MC_Home() {
    ecmcMcHomeDestroy(handle_);
  }

  int run(AxisRef axis,
          bool    execute,
          int     seq_id,
          double  home_position,
          double  velocity_towards_cam,
          double  velocity_off_cam,
          double  acceleration,
          double  deceleration) {
    if (!handle_) {
      assignCreateFailed();
      return -1;
    }
    ecmcMcHomeStatus status{};
    const int error = ecmcMcHomeRun(handle_,
                                    axis.axis_index,
                                    execute,
                                    seq_id,
                                    home_position,
                                    velocity_towards_cam,
                                    velocity_off_cam,
                                    acceleration,
                                    deceleration,
                                    &status);
    assignBase(status);
    return error;
  }

private:
  ecmcMcHomeHandle* handle_ = nullptr;
};

class MC_MoveVelocity : public FunctionBlockBase {
public:
  bool InVelocity = false;

  MC_MoveVelocity() : handle_(ecmcMcMoveVelocityCreate()) {}

  ~MC_MoveVelocity() {
    ecmcMcMoveVelocityDestroy(handle_);
  }

  int run(AxisRef axis,
          bool    execute,
          double  velocity,
          double  acceleration,
          double  deceleration) {
    if (!handle_) {
      assignCreateFailed();
      InVelocity = false;
      return -1;
    }
    ecmcMcMoveVelocityStatus status{};
    const int error = ecmcMcMoveVelocityRun(handle_,
                                            axis.axis_index,
                                            execute,
                                            velocity,
                                            acceleration,
                                            deceleration,
                                            &status);
    assignBase(status);
    InVelocity = status.InVelocity != 0;
    return error;
  }

private:
  ecmcMcMoveVelocityHandle* handle_ = nullptr;
};

class MC_Halt : public FunctionBlockBase {
public:
  MC_Halt() : handle_(ecmcMcHaltCreate()) {}

  ~MC_Halt() {
    ecmcMcHaltDestroy(handle_);
  }

  int run(AxisRef axis, bool execute) {
    if (!handle_) {
      assignCreateFailed();
      return -1;
    }
    ecmcMcHaltStatus status{};
    const int error = ecmcMcHaltRun(handle_, axis.axis_index, execute, &status);
    assignBase(status);
    return error;
  }

private:
  ecmcMcHaltHandle* handle_ = nullptr;
};

class MC_ReadStatus : public FunctionBlockBase {
public:
  bool Valid              = false;
  bool ErrorStop          = false;
  bool Disabled           = false;
  bool Stopping           = false;
  bool Homing             = false;
  bool StandStill         = false;
  bool DiscreteMotion     = false;
  bool ContinuousMotion   = false;
  bool SynchronizedMotion = false;

  MC_ReadStatus() : handle_(ecmcMcReadStatusCreate()) {}

  ~MC_ReadStatus() {
    ecmcMcReadStatusDestroy(handle_);
  }

  int run(AxisRef axis, bool enable) {
    if (!handle_) {
      assignCreateFailed();
      Valid = false;
      return -1;
    }
    ecmcMcReadStatusStatus status{};
    const int error = ecmcMcReadStatusRun(handle_, axis.axis_index, enable, &status);
    assignBase(status);
    Valid              = status.Valid != 0;
    ErrorStop          = status.ErrorStop != 0;
    Disabled           = status.Disabled != 0;
    Stopping           = status.Stopping != 0;
    Homing             = status.Homing != 0;
    StandStill         = status.StandStill != 0;
    DiscreteMotion     = status.DiscreteMotion != 0;
    ContinuousMotion   = status.ContinuousMotion != 0;
    SynchronizedMotion = status.SynchronizedMotion != 0;
    return error;
  }

private:
  ecmcMcReadStatusHandle* handle_ = nullptr;
};

class MC_ReadActualPosition : public FunctionBlockBase {
public:
  bool   Valid    = false;
  double Position = 0.0;

  MC_ReadActualPosition() : handle_(ecmcMcReadActualPositionCreate()) {}

  ~MC_ReadActualPosition() {
    ecmcMcReadActualPositionDestroy(handle_);
  }

  int run(AxisRef axis, bool enable) {
    if (!handle_) {
      assignCreateFailed();
      Valid    = false;
      Position = 0.0;
      return -1;
    }
    ecmcMcReadActualPositionStatus status{};
    const int error = ecmcMcReadActualPositionRun(handle_, axis.axis_index, enable, &status);
    assignBase(status);
    Valid    = status.Valid != 0;
    Position = status.Position;
    return error;
  }

private:
  ecmcMcReadActualPositionHandle* handle_ = nullptr;
};

class MC_ReadActualVelocity : public FunctionBlockBase {
public:
  bool   Valid    = false;
  double Velocity = 0.0;

  MC_ReadActualVelocity() : handle_(ecmcMcReadActualVelocityCreate()) {}

  ~MC_ReadActualVelocity() {
    ecmcMcReadActualVelocityDestroy(handle_);
  }

  int run(AxisRef axis, bool enable) {
    if (!handle_) {
      assignCreateFailed();
      Valid    = false;
      Velocity = 0.0;
      return -1;
    }
    ecmcMcReadActualVelocityStatus status{};
    const int error = ecmcMcReadActualVelocityRun(handle_, axis.axis_index, enable, &status);
    assignBase(status);
    Valid    = status.Valid != 0;
    Velocity = status.Velocity;
    return error;
  }

private:
  ecmcMcReadActualVelocityHandle* handle_ = nullptr;
};

}  // namespace mc
}  // namespace ecmcStrucpp
