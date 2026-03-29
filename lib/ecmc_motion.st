TYPE ECMC_AXIS_REF :
STRUCT
  AxisIndex : DINT;
END_STRUCT
END_TYPE

FUNCTION_BLOCK MC_Power
VAR_INPUT
  Axis   : ECMC_AXIS_REF;
  Enable : BOOL;
END_VAR
VAR_OUTPUT
  Status  : BOOL;
  Valid   : BOOL;
  Busy    : BOOL;
  Error   : BOOL;
  ErrorID : UDINT;
  Active  : BOOL;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcPowerHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcPowerCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    STATUS.set(false);
    VALID.set(false);
    BUSY.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
    ACTIVE.set(false);
  } else {
    ecmcMcPowerStatus fb_status{};
    ecmcMcPowerRun(handle, AXIS.AXISINDEX.get(), ENABLE.get() ? 1 : 0, &fb_status);
    STATUS.set(fb_status.Status != 0);
    VALID.set(fb_status.Valid != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
    ACTIVE.set(fb_status.base.Active != 0);
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_Reset
VAR_INPUT
  Axis    : ECMC_AXIS_REF;
  Execute : BOOL;
END_VAR
VAR_OUTPUT
  Done    : BOOL;
  Busy    : BOOL;
  Error   : BOOL;
  ErrorID : UDINT;
  Active  : BOOL;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcResetHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcResetCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    DONE.set(false);
    BUSY.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
    ACTIVE.set(false);
  } else {
    ecmcMcResetStatus fb_status{};
    ecmcMcResetRun(handle, AXIS.AXISINDEX.get(), EXECUTE.get() ? 1 : 0, &fb_status);
    DONE.set(fb_status.base.Done != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
    ACTIVE.set(fb_status.base.Active != 0);
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_MoveAbsolute
VAR_INPUT
  Axis         : ECMC_AXIS_REF;
  Execute      : BOOL;
  Position     : LREAL;
  Velocity     : LREAL;
  Acceleration : LREAL;
  Deceleration : LREAL;
END_VAR
VAR_OUTPUT
  Done            : BOOL;
  Busy            : BOOL;
  Active          : BOOL;
  CommandAborted  : BOOL;
  Error           : BOOL;
  ErrorID         : UDINT;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcMoveAbsoluteHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcMoveAbsoluteCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    DONE.set(false);
    BUSY.set(false);
    ACTIVE.set(false);
    COMMANDABORTED.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
  } else {
    ecmcMcMoveAbsoluteStatus fb_status{};
    ecmcMcMoveAbsoluteRun(handle,
                          AXIS.AXISINDEX.get(),
                          EXECUTE.get() ? 1 : 0,
                          POSITION.get(),
                          VELOCITY.get(),
                          ACCELERATION.get(),
                          DECELERATION.get(),
                          &fb_status);
    DONE.set(fb_status.base.Done != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ACTIVE.set(fb_status.base.Active != 0);
    COMMANDABORTED.set(fb_status.base.CommandAborted != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_MoveRelative
VAR_INPUT
  Axis         : ECMC_AXIS_REF;
  Execute      : BOOL;
  Distance     : LREAL;
  Velocity     : LREAL;
  Acceleration : LREAL;
  Deceleration : LREAL;
END_VAR
VAR_OUTPUT
  Done            : BOOL;
  Busy            : BOOL;
  Active          : BOOL;
  CommandAborted  : BOOL;
  Error           : BOOL;
  ErrorID         : UDINT;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcMoveRelativeHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcMoveRelativeCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    DONE.set(false);
    BUSY.set(false);
    ACTIVE.set(false);
    COMMANDABORTED.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
  } else {
    ecmcMcMoveRelativeStatus fb_status{};
    ecmcMcMoveRelativeRun(handle,
                          AXIS.AXISINDEX.get(),
                          EXECUTE.get() ? 1 : 0,
                          DISTANCE.get(),
                          VELOCITY.get(),
                          ACCELERATION.get(),
                          DECELERATION.get(),
                          &fb_status);
    DONE.set(fb_status.base.Done != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ACTIVE.set(fb_status.base.Active != 0);
    COMMANDABORTED.set(fb_status.base.CommandAborted != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_MoveVelocity
VAR_INPUT
  Axis         : ECMC_AXIS_REF;
  Execute      : BOOL;
  Velocity     : LREAL;
  Acceleration : LREAL;
  Deceleration : LREAL;
END_VAR
VAR_OUTPUT
  InVelocity      : BOOL;
  Busy            : BOOL;
  Active          : BOOL;
  CommandAborted  : BOOL;
  Error           : BOOL;
  ErrorID         : UDINT;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcMoveVelocityHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcMoveVelocityCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    INVELOCITY.set(false);
    BUSY.set(false);
    ACTIVE.set(false);
    COMMANDABORTED.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
  } else {
    ecmcMcMoveVelocityStatus fb_status{};
    ecmcMcMoveVelocityRun(handle,
                          AXIS.AXISINDEX.get(),
                          EXECUTE.get() ? 1 : 0,
                          VELOCITY.get(),
                          ACCELERATION.get(),
                          DECELERATION.get(),
                          &fb_status);
    INVELOCITY.set(fb_status.InVelocity != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ACTIVE.set(fb_status.base.Active != 0);
    COMMANDABORTED.set(fb_status.base.CommandAborted != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_Home
VAR_INPUT
  Axis               : ECMC_AXIS_REF;
  Execute            : BOOL;
  SeqId              : DINT;
  HomePosition       : LREAL;
  VelocityTowardsCam : LREAL;
  VelocityOffCam     : LREAL;
  Acceleration       : LREAL;
  Deceleration       : LREAL;
END_VAR
VAR_OUTPUT
  Done            : BOOL;
  Busy            : BOOL;
  Active          : BOOL;
  CommandAborted  : BOOL;
  Error           : BOOL;
  ErrorID         : UDINT;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcHomeHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcHomeCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    DONE.set(false);
    BUSY.set(false);
    ACTIVE.set(false);
    COMMANDABORTED.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
  } else {
    ecmcMcHomeStatus fb_status{};
    ecmcMcHomeRun(handle,
                  AXIS.AXISINDEX.get(),
                  EXECUTE.get() ? 1 : 0,
                  static_cast<int>(SEQID.get()),
                  HOMEPOSITION.get(),
                  VELOCITYTOWARDSCAM.get(),
                  VELOCITYOFFCAM.get(),
                  ACCELERATION.get(),
                  DECELERATION.get(),
                  &fb_status);
    DONE.set(fb_status.base.Done != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ACTIVE.set(fb_status.base.Active != 0);
    COMMANDABORTED.set(fb_status.base.CommandAborted != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_Halt
VAR_INPUT
  Axis    : ECMC_AXIS_REF;
  Execute : BOOL;
END_VAR
VAR_OUTPUT
  Done            : BOOL;
  Busy            : BOOL;
  Active          : BOOL;
  CommandAborted  : BOOL;
  Error           : BOOL;
  ErrorID         : UDINT;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcHaltHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcHaltCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    DONE.set(false);
    BUSY.set(false);
    ACTIVE.set(false);
    COMMANDABORTED.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
  } else {
    ecmcMcHaltStatus fb_status{};
    ecmcMcHaltRun(handle, AXIS.AXISINDEX.get(), EXECUTE.get() ? 1 : 0, &fb_status);
    DONE.set(fb_status.base.Done != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ACTIVE.set(fb_status.base.Active != 0);
    COMMANDABORTED.set(fb_status.base.CommandAborted != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_ReadStatus
VAR_INPUT
  Axis   : ECMC_AXIS_REF;
  Enable : BOOL;
END_VAR
VAR_OUTPUT
  Valid              : BOOL;
  Busy               : BOOL;
  Error              : BOOL;
  ErrorID            : UDINT;
  ErrorStop          : BOOL;
  Disabled           : BOOL;
  Stopping           : BOOL;
  Homing             : BOOL;
  StandStill         : BOOL;
  DiscreteMotion     : BOOL;
  ContinuousMotion   : BOOL;
  SynchronizedMotion : BOOL;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcReadStatusHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcReadStatusCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    VALID.set(false);
    BUSY.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
    ERRORSTOP.set(false);
    DISABLED.set(false);
    STOPPING.set(false);
    HOMING.set(false);
    STANDSTILL.set(false);
    DISCRETEMOTION.set(false);
    CONTINUOUSMOTION.set(false);
    SYNCHRONIZEDMOTION.set(false);
  } else {
    ecmcMcReadStatusStatus fb_status{};
    ecmcMcReadStatusRun(handle, AXIS.AXISINDEX.get(), ENABLE.get() ? 1 : 0, &fb_status);
    VALID.set(fb_status.Valid != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
    ERRORSTOP.set(fb_status.ErrorStop != 0);
    DISABLED.set(fb_status.Disabled != 0);
    STOPPING.set(fb_status.Stopping != 0);
    HOMING.set(fb_status.Homing != 0);
    STANDSTILL.set(fb_status.StandStill != 0);
    DISCRETEMOTION.set(fb_status.DiscreteMotion != 0);
    CONTINUOUSMOTION.set(fb_status.ContinuousMotion != 0);
    SYNCHRONIZEDMOTION.set(fb_status.SynchronizedMotion != 0);
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_ReadActualPosition
VAR_INPUT
  Axis   : ECMC_AXIS_REF;
  Enable : BOOL;
END_VAR
VAR_OUTPUT
  Valid    : BOOL;
  Busy     : BOOL;
  Error    : BOOL;
  ErrorID  : UDINT;
  Position : LREAL;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcReadActualPositionHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcReadActualPositionCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    VALID.set(false);
    BUSY.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
    POSITION.set(0.0);
  } else {
    ecmcMcReadActualPositionStatus fb_status{};
    ecmcMcReadActualPositionRun(handle, AXIS.AXISINDEX.get(), ENABLE.get() ? 1 : 0, &fb_status);
    VALID.set(fb_status.Valid != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
    POSITION.set(fb_status.Position);
  }
}
END_FUNCTION_BLOCK

FUNCTION_BLOCK MC_ReadActualVelocity
VAR_INPUT
  Axis   : ECMC_AXIS_REF;
  Enable : BOOL;
END_VAR
VAR_OUTPUT
  Valid    : BOOL;
  Busy     : BOOL;
  Error    : BOOL;
  ErrorID  : UDINT;
  Velocity : LREAL;
END_VAR
VAR
  StatePtr : ULINT;
END_VAR
{external
  auto *handle = reinterpret_cast<ecmcMcReadActualVelocityHandle *>(static_cast<uintptr_t>(STATEPTR.get()));
  if (!handle) {
    handle = ecmcMcReadActualVelocityCreate();
    STATEPTR.set(static_cast<ULINT_t>(reinterpret_cast<uintptr_t>(handle)));
  }

  if (!handle) {
    VALID.set(false);
    BUSY.set(false);
    ERROR.set(true);
    ERRORID.set(static_cast<UDINT_t>(0xffffffffu));
    VELOCITY.set(0.0);
  } else {
    ecmcMcReadActualVelocityStatus fb_status{};
    ecmcMcReadActualVelocityRun(handle, AXIS.AXISINDEX.get(), ENABLE.get() ? 1 : 0, &fb_status);
    VALID.set(fb_status.Valid != 0);
    BUSY.set(fb_status.base.Busy != 0);
    ERROR.set(fb_status.base.Error != 0);
    ERRORID.set(static_cast<UDINT_t>(fb_status.base.ErrorID));
    VELOCITY.set(fb_status.Velocity);
  }
}
END_FUNCTION_BLOCK
